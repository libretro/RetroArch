/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2017 - Andrés Suárez
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

#include <stddef.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>

#include <lists/file_list.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_http_parse.h>
#include "../../network/netplay/netplay.h"
#include "../network/netplay/netplay_discovery.h"
#endif

#if defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../frontend/drivers/platform_unix.h"
#endif

#include "menu_content.h"
#include "menu_driver.h"
#include "menu_shader.h"
#include "widgets/menu_dialog.h"
#include "widgets/menu_list.h"
#include "widgets/menu_filebrowser.h"
#include "menu_cbs.h"

#include "../audio/audio_driver.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../defaults.h"
#include "../managers/cheat_manager.h"
#include "../managers/core_option_manager.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../core.h"
#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_driver.h"
#include "../config.features.h"
#include "../version_git.h"
#include "../input/input_config.h"
#include "../list_special.h"
#include "../performance_counters.h"
#include "../core_info.h"
#include "../wifi/wifi_driver.h"
#include "../tasks/tasks_internal.h"

static char new_path_entry[4096]        = {0};
static char new_lbl_entry[4096]         = {0};
static char new_entry[4096]             = {0};
static enum msg_hash_enums new_type     = MSG_UNKNOWN;

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
static char *core_buf                   = NULL;
static size_t core_len                  = 0;

void cb_net_generic_subdir(void *task_data, void *user_data, const char *err)
{
   char subdir_path[PATH_MAX_LENGTH];
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state       = (menu_file_transfer_t*)user_data;

   subdir_path[0] = '\0';

   if (!data || err)
      goto finish;

   memcpy(subdir_path, data->data, data->len * sizeof(char));
   subdir_path[data->len] = '\0';

finish:
   if (!err && !strstr(subdir_path, file_path_str(FILE_PATH_INDEX_DIRS_URL)))
   {
      char parent_dir[PATH_MAX_LENGTH];

      parent_dir[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, sizeof(parent_dir));

      /*generic_action_ok_displaylist_push(parent_dir, NULL,
            subdir_path, 0, 0, 0, ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST);*/
   }

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (user_data)
      free(user_data);
}

void cb_net_generic(void *task_data, void *user_data, const char *err)
{
   bool refresh                = false;
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state = (menu_file_transfer_t*)user_data;

   if (core_buf)
      free(core_buf);


   core_buf = NULL;
   core_len = 0;

   if (!data || err)
      goto finish;

   core_buf = (char*)malloc((data->len+1) * sizeof(char));

   if (!core_buf)
      goto finish;

   memcpy(core_buf, data->data, data->len * sizeof(char));
   core_buf[data->len] = '\0';
   core_len      = data->len;

finish:
   refresh = true;
   menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (!err && !strstr(state->path, file_path_str(FILE_PATH_INDEX_DIRS_URL)))
   {
      char parent_dir[PATH_MAX_LENGTH];
      menu_file_transfer_t *transf     = NULL;

      parent_dir[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, sizeof(parent_dir));
      strlcat(parent_dir, file_path_str(FILE_PATH_INDEX_DIRS_URL), sizeof(parent_dir));

      transf           = (menu_file_transfer_t*)malloc(sizeof(*transf));

      transf->enum_idx = MSG_UNKNOWN;
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      task_push_http_transfer(parent_dir, true, "index_dirs", cb_net_generic_subdir, transf);
   }

   if (state)
      free(state);
}

static void print_buf_lines(file_list_t *list, char *buf,
      const char *label, int buf_size,
      enum msg_file_type type, bool append, bool extended)
{
   char c;
   int i, j = 0;
   char *line_start = buf;

   if (!buf || !buf_size)
   {
      menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0);
      return;
   }

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;
      const char *core_date        = NULL;
      const char *core_crc         = NULL;
      const char *core_pathname    = NULL;
      struct string_list *str_list = NULL;

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

      str_list      = string_split(line_start, " ");

      if (str_list->elems[0].data)
         core_date     = str_list->elems[0].data;
      if (str_list->elems[1].data)
         core_crc      = str_list->elems[1].data;
      if (str_list->elems[2].data)
         core_pathname = str_list->elems[2].data;

      (void)core_date;
      (void)core_crc;

      if (extended)
      {
         if (append)
            menu_entries_append_enum(list, core_pathname, "",
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
         else
            menu_entries_prepend(list, core_pathname, "",
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
      }
      else
      {
         if (append)
            menu_entries_append_enum(list, line_start, label,
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
         else
            menu_entries_prepend(list, line_start, label,
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
      }

      switch (type)
      {
         case FILE_TYPE_DOWNLOAD_CORE:
            {
               settings_t *settings      = config_get_ptr();

               if (settings)
               {
                  char display_name[255];
                  char core_path[PATH_MAX_LENGTH];
                  char *last                         = NULL;

                  display_name[0] = core_path[0]     = '\0';

                  fill_pathname_join_noext(
                        core_path,
                        settings->paths.path_libretro_info,
                        (extended && !string_is_empty(core_pathname))
                        ? core_pathname : line_start,
                        sizeof(core_path));
                  path_remove_extension(core_path);

                  last = (char*)strrchr(core_path, '_');

                  if (!string_is_empty(last))
                  {
                     if (string_is_not_equal_fast(last, "_libretro", 9))
                        *last = '\0';
                  }

                  strlcat(core_path,
                        file_path_str(FILE_PATH_CORE_INFO_EXTENSION),
                        sizeof(core_path));

                  if (
                           path_file_exists(core_path)
                        && core_info_get_display_name(
                           core_path, display_name, sizeof(display_name)))
                     menu_entries_set_alt_at_offset(list, j, display_name);
               }
            }
            break;
         default:
         case FILE_TYPE_NONE:
            break;
      }

      j++;

      string_list_free(str_list);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start     = buf + i + 1;
   }

   if (append)
      file_list_sort_on_alt(list);
   /* If the buffer was completely full, and didn't end
    * with a newline, just ignore the partial last line. */
}

#if !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU)
#include <net/net_ifinfo.h>

static int menu_displaylist_parse_network_info(menu_displaylist_info_t *info)
{
   unsigned k              = 0;
   net_ifinfo_t      list;

   if (!net_ifinfo_new(&list))
      return -1;

   for (k = 0; k < list.size; k++)
   {
      char tmp[255];

      tmp[0] = '\0';

      snprintf(tmp, sizeof(tmp), "%s (%s) : %s\n",
            msg_hash_to_str(MSG_INTERFACE),
            list.entries[k].name, list.entries[k].host);
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_NETWORK_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   net_ifinfo_free(&list);
   return 0;
}
#endif

#endif

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS),
            MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS,
            0, 0, 0);
      return;
   }

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_entries_append_enum(info->list,
               counters[i]->ident, "",
               (enum msg_hash_enums)(id + i),
               id + i , 0, 0);
}

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info)
{
   unsigned i;
   char tmp[PATH_MAX_LENGTH];
   core_info_t *core_info    = NULL;

   tmp[0] = '\0';

   core_info_get_current_core(&core_info);

   if (!core_info || !core_info->config_data)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE),
            MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE,
            0, 0, 0);
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_DELETE),
            MENU_ENUM_LABEL_CORE_DELETE,
            MENU_SETTING_ACTION_CORE_DELETE, 0, 0);

      return 0;
   }

   fill_pathname_noext(tmp,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME),
         ": ",
         sizeof(tmp));
   if (core_info->core_name)
      strlcat(tmp, core_info->core_name, sizeof(tmp));

   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   fill_pathname_noext(tmp,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL),
         ": ",
         sizeof(tmp));
   if (core_info->display_name)
      strlcat(tmp, core_info->display_name, sizeof(tmp));
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   if (core_info->systemname)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME),
            ": ",
            sizeof(tmp));
      strlcat(tmp, core_info->systemname, sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->system_manufacturer)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER),
            ": ",
            sizeof(tmp));
      strlcat(tmp, core_info->system_manufacturer, sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->categories_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->categories_list, ", ");
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->authors_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->authors_list, ", ");
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->permissions_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->permissions_list, ", ");
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->licenses_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->licenses_list, ", ");
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->supported_extensions_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->supported_extensions_list, ", ");
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->firmware_count > 0)
   {
      core_info_ctx_firmware_t firmware_info;
      settings_t *settings           = config_get_ptr();

      firmware_info.path             = core_info->path;
      firmware_info.directory.system = settings->paths.directory_system;

      if (core_info_list_update_missing_firmware(&firmware_info))
      {
         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE),
               ": ",
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

         /* FIXME: This looks hacky and probably
          * needs to be improved for good translation support. */

         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (core_info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "(!) %s, %s: %s",
                     core_info->firmware[i].missing ?
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING) :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT),
                     core_info->firmware[i].optional ?
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPTIONAL) :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REQUIRED),
                     core_info->firmware[i].desc ?
                     core_info->firmware[i].desc :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME)
                     );

               menu_entries_append_enum(info->list, tmp, "",
                     MENU_ENUM_LABEL_CORE_INFO_ENTRY,
                     MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
            }
         }
      }
   }

   if (core_info->notes)
   {
      for (i = 0; i < core_info->note_list->size; i++)
      {
         strlcpy(tmp,
               core_info->note_list->elems[i].data, sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

  menu_entries_append_enum(info->list,
        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE),
        msg_hash_to_str(MENU_ENUM_LABEL_CORE_DELETE),
        MENU_ENUM_LABEL_CORE_DELETE,
        MENU_SETTING_ACTION_CORE_DELETE, 0, 0);

   return 0;
}


static uint64_t bytes_to_kb(uint64_t bytes)
{
   return bytes / 1024;
}

static uint64_t bytes_to_mb(uint64_t bytes)
{
   return bytes / 1024 / 1024;
}

static uint64_t bytes_to_gb(uint64_t bytes)
{
   return bytes_to_kb(bytes) / 1024 / 1024;
}

static int menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   int controller;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   gfx_ctx_ident_t ident_info;
#endif
   char tmp[PATH_MAX_LENGTH];
   char feat_str[255];
#ifdef ANDROID
   bool perms                            = false;
#endif
   const char *tmp_string                = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();

   tmp[0] = feat_str[0] = '\0';

   snprintf(tmp, sizeof(tmp), "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE), __DATE__);
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   (void)tmp_string;

#ifdef HAVE_GIT_VERSION
   fill_pathname_noext(tmp,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION),
         ": ",
         sizeof(tmp));
   strlcat(tmp, retroarch_git_version, sizeof(tmp));
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
#endif

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#ifdef ANDROID
   perms = test_permissions(internal_storage_path);

   snprintf(tmp, sizeof(tmp), "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS),
         perms ? "read-write" : "read-only");
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#endif
   {
      char cpu_str[255];

      cpu_str[0] = '\0';

      fill_pathname_noext(cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES),
            ": ",
            sizeof(cpu_str));

      retroarch_get_capabilities(RARCH_CAPABILITIES_CPU,
            cpu_str, sizeof(cpu_str));
      menu_entries_append_enum(info->list, cpu_str, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   {
      char cpu_str[255];
      char cpu_arch_str[PATH_MAX_LENGTH];
      char cpu_text_str[PATH_MAX_LENGTH];
      enum frontend_architecture arch = frontend_driver_get_cpu_architecture();

      cpu_str[0] = cpu_arch_str[0] = cpu_text_str[0] = '\0';

      strlcpy(cpu_text_str,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE),
            sizeof(cpu_text_str));

      switch (arch)
      {
         case FRONTEND_ARCH_X86:
            strlcpy(cpu_arch_str, "x86", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_X86_64:
            strlcpy(cpu_arch_str, "x86-64", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_PPC:
            strlcpy(cpu_arch_str, "PPC", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_ARM:
            strlcpy(cpu_arch_str, "ARM", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_ARMV7:
            strlcpy(cpu_arch_str, "ARMv7", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_ARMV8:
            strlcpy(cpu_arch_str, "ARMv8", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_MIPS:
            strlcpy(cpu_arch_str, "MIPS", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_TILE:
            strlcpy(cpu_arch_str, "Tilera", sizeof(cpu_arch_str));
            break;
         case FRONTEND_ARCH_NONE:
         default:
            strlcpy(cpu_arch_str,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
                  sizeof(cpu_arch_str));
            break;
      }

      snprintf(cpu_str, sizeof(cpu_str), "%s %s", cpu_text_str, cpu_arch_str);

      menu_entries_append_enum(info->list, cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_CPU_ARCHITECTURE),
            MENU_ENUM_LABEL_CPU_ARCHITECTURE, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   {
      char cpu_str[PATH_MAX_LENGTH];
      unsigned         amount_cores = cpu_features_get_core_amount();

      cpu_str[0] = '\0';

      snprintf(cpu_str, sizeof(cpu_str),
            "%s %d\n", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_CORES), amount_cores);
      menu_entries_append_enum(info->list, cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_CPU_CORES),
            MENU_ENUM_LABEL_CPU_CORES, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }


   for(controller = 0; controller < MAX_USERS; controller++)
   {
       if (input_is_autoconfigured(controller))
       {
           snprintf(tmp, sizeof(tmp), "Port #%d device name: %s (#%d)",
                 controller,
                 input_config_get_device_name(controller),
                 input_autoconfigure_get_device_name_index(controller));
           menu_entries_append_enum(info->list, tmp, "",
                 MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                 MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
           snprintf(tmp, sizeof(tmp), "Port #%d device VID/PID: %d/%d",
                 controller,
                 input_config_get_vid(controller),
                 input_config_get_pid(controller));
           menu_entries_append_enum(info->list, tmp, "",
                 MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                 MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
       }
   }

   if (frontend)
   {
      char tmp2[PATH_MAX_LENGTH];
      int                  major = 0;
      int                  minor = 0;

      tmp2[0] = '\0';

      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER),
            ": ",
            sizeof(tmp));
      strlcat(tmp, frontend->ident, sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#ifdef HAVE_LAKKA
      if (frontend->get_lakka_version)
      {
         frontend->get_lakka_version(tmp2, sizeof(tmp2));

         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION),
               ": ",
               sizeof(tmp));
         strlcat(tmp, frontend->get_lakka_version ?
               tmp2 : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
#endif

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));

         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME),
               ": ",
               sizeof(tmp));
         strlcat(tmp, frontend->get_name ?
               tmp2 : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s : %s %d.%d",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS),
               frontend->get_os
               ? tmp2 : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               major, minor);
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      snprintf(tmp, sizeof(tmp), "%s : %d",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
            frontend->get_rating ? frontend->get_rating() : -1);
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

      {
         char tmp[PATH_MAX_LENGTH];
         char tmp2[PATH_MAX_LENGTH];
         char tmp3[PATH_MAX_LENGTH];
         uint64_t memory_used       = frontend_driver_get_used_memory();
         uint64_t memory_total      = frontend_driver_get_total_memory();

         tmp[0] = tmp2[0] = tmp3[0] = '\0';

         if (memory_used != 0 && memory_total != 0)
         {
#ifdef _WIN32
            snprintf(tmp, sizeof(tmp),
                  "%s %s: %Iu/%Iu B",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_BYTES),
                  memory_used,
                  memory_total
                  );
            snprintf(tmp2, sizeof(tmp2),
                  "%s %s: %Iu/%Iu MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  bytes_to_mb(memory_used),
                  bytes_to_mb(memory_total)
                  );
            snprintf(tmp3, sizeof(tmp3),
                  "%s %s: %Iu/%Iu GB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_GIGABYTES),
                  bytes_to_gb(memory_used),
                  bytes_to_gb(memory_total)
                  );
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
            snprintf(tmp, sizeof(tmp),
                  "%s %s  : %llu/%llu B",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_BYTES),
                  (unsigned long long)memory_used,
                  (unsigned long long)memory_total
                  );
            snprintf(tmp2, sizeof(tmp2),
                  "%s %s : %llu/%llu MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  (unsigned long long)bytes_to_mb(memory_used),
                  (unsigned long long)bytes_to_mb(memory_total)
                  );
            snprintf(tmp3, sizeof(tmp3),
                  "%s %s: %llu/%llu GB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_GIGABYTES),
                  (unsigned long long)bytes_to_gb(memory_used),
                  (unsigned long long)bytes_to_gb(memory_total)
                  );
#else
            snprintf(tmp, sizeof(tmp),
                  "%s %s: %lu/%lu B",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_BYTES),
                  memory_used,
                  memory_total
                  );
            snprintf(tmp2, sizeof(tmp2),
                  "%s %s : %lu/%lu MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  bytes_to_mb(memory_used),
                  bytes_to_mb(memory_total)
                  );
            snprintf(tmp3, sizeof(tmp3),
                  "%s %s : %lu/%lu GB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_GIGABYTES),
                  bytes_to_gb(memory_used),
                  bytes_to_gb(memory_total)
                  );
#endif
            menu_entries_append_enum(info->list, tmp, "",
                  MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
            menu_entries_append_enum(info->list, tmp2, "",
                  MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
            menu_entries_append_enum(info->list, tmp3, "",
                  MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
         }
      }

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
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_NO_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGING:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_CHARGED:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
            case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING),
                     sizeof(tmp));
               strlcat(tmp2, ")", sizeof(tmp));
               break;
         }

         fill_pathname_noext(tmp,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE),
               ": ",
               sizeof(tmp));
         strlcat(tmp, tmp2, sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   video_context_driver_get_ident(&ident_info);
   tmp_string = ident_info.ident;

   fill_pathname_noext(tmp,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER),
         ": ",
         sizeof(tmp));
   strlcat(tmp, tmp_string ? tmp_string
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
         sizeof(tmp));
   menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   {
      gfx_ctx_metrics_t metrics;
      float val = 0.0f;

      metrics.type  = DISPLAY_METRIC_MM_WIDTH;
      metrics.value = &val;

      if (video_context_driver_get_metrics(&metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH),
               val);
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      metrics.type  = DISPLAY_METRIC_MM_HEIGHT;

      if (video_context_driver_get_metrics(&metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT),
               val);
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      metrics.type  = DISPLAY_METRIC_DPI;

      if (video_context_driver_get_metrics(&metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI),
               val);
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }
#endif

   fill_pathname_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT),
         ": ",
         sizeof(feat_str));
   strlcat(feat_str,
         _libretrodb_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   fill_pathname_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT),
         ": ",
         sizeof(feat_str));
   strlcat(feat_str, _overlay_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   fill_pathname_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT),
         ": ",
         sizeof(feat_str));
   strlcat(feat_str, _command_supp
         ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s : %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT),
         _network_command_supp
         ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s : %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT),
         _network_gamepad_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT),
          _cocoa_supp ?
          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT),
         _rpng_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT),
         _rjpeg_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT),
         _rbmp_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT),
         _rtga_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT),
         _sdl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT),
         _sdl2_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT),
         _vulkan_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT),
         _opengl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT),
         _opengles_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT),
         _thread_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT),
         _kms_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT),
         _udev_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT),
         _vg_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT),
         _egl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT),
         _x11_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT),
         _wayland_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT),
         _xvideo_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT),
         _alsa_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT),
         _oss_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT),
         _al_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT),
         _sl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT),
         _rsound_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT),
         _roar_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT),
         _jack_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT),
         _pulse_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT),
         _dsound_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT),
         _wasapi_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT),
         _xaudio_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT),
         _zlib_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT),
         _7zip_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT),
         _dylib_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT),
         _dynamic_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT),
         _cg_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT),
         _glsl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT),
         _hlsl_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT),
         _libxml2_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT),
         _sdl_image_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT),
         _fbo_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT),
         _ffmpeg_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT),
         _coretext_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT),
         _freetype_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT),
         _netplay_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT),
         _python_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT),
         _v4l2_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT),
         _libusb_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   return 0;
}

static int menu_displaylist_parse_playlist(menu_displaylist_info_t *info,
      playlist_t *playlist, const char *path_playlist, bool is_history)
{
   unsigned i;
   size_t list_size = 0;
   size_t selection = menu_navigation_get_selection();

   if (!playlist)
      return -1;

   list_size = playlist_size(playlist);

   if (list_size == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE,
            MENU_INFO_MESSAGE, 0, 0);
      return 0;
   }

   if (!string_is_empty(info->path))
   {
      char lpl_basename[PATH_MAX_LENGTH];
      lpl_basename[0] = '\0';
      fill_pathname_base_noext(lpl_basename, info->path, sizeof(lpl_basename));
      menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));
   }

   for (i = 0; i < list_size; i++)
   {
      char fill_buf[PATH_MAX_LENGTH];
      char path_copy[PATH_MAX_LENGTH];
      const char *core_name           = NULL;
      const char *path                = NULL;
      const char *label               = NULL;

      fill_buf[0] = path_copy[0]      = '\0';

      strlcpy(path_copy, info->path, sizeof(path_copy));

      path = path_copy;

      playlist_get_index(playlist, i,
            &path, &label, NULL, &core_name, NULL, NULL);

      if (core_name)
         strlcpy(fill_buf, core_name, sizeof(fill_buf));

      if (!is_history && i == selection)
      {
         char content_basename[PATH_MAX_LENGTH];
         strlcpy(content_basename, label, sizeof(content_basename));
         menu_driver_set_thumbnail_content(content_basename, PATH_MAX_LENGTH);
         menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH, NULL);
         menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE, NULL);
      }

      if (path)
      {
         char path_short[PATH_MAX_LENGTH];

         path_short[0] = '\0';

         fill_short_pathname_representation(path_short, path,
               sizeof(path_short));
         strlcpy(fill_buf,
               (!string_is_empty(label)) ? label : path_short,
               sizeof(fill_buf));

         if (!string_is_empty(core_name))
         {
            if (!string_is_equal(core_name,
                     file_path_str(FILE_PATH_DETECT)))
            {
               char tmp[PATH_MAX_LENGTH];

               tmp[0] = '\0';

               snprintf(tmp, sizeof(tmp), " (%s)", core_name);
               strlcat(fill_buf, tmp, sizeof(fill_buf));
            }
         }
      }

      if (!path)
         menu_entries_append_enum(info->list, fill_buf, path_playlist,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_PLAYLIST_ENTRY, 0, i);
      else if (is_history)
         menu_entries_append_enum(info->list, fill_buf,
               path, MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_RPL_ENTRY, 0, i);
      else
         menu_entries_append_enum(info->list, label,
               path, MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_RPL_ENTRY, 0, i);
   }

   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   unsigned pass_count = menu_shader_manager_get_amount_passes();

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES),
         msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES),
         MENU_ENUM_LABEL_SHADER_APPLY_CHANGES,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET,
         FILE_TYPE_PATH, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES),
         MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES,
         0, 0, 0);

   for (i = 0; i < pass_count; i++)
   {
      char buf_tmp[64];
      char buf[64];

      buf[0] = buf_tmp[0] = '\0';

      snprintf(buf_tmp, sizeof(buf_tmp),
            "%s #%u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER), i);

      menu_entries_append_enum(info->list, buf_tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PASS),
            MENU_ENUM_LABEL_VIDEO_SHADER_PASS,
            MENU_SETTINGS_SHADER_PASS_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s %s", buf_tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILTER));
      menu_entries_append_enum(info->list, buf,
            msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS),
            MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS,
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0, 0);

      snprintf(buf, sizeof(buf), "%s %s", buf_tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCALE));
      menu_entries_append_enum(info->list, buf,
            msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS),
            MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS,
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0, 0);
   }

   return 0;
}

#ifdef HAVE_LIBRETRODB
static int create_string_list_rdb_entry_string(
      enum msg_hash_enums enum_idx,
      const char *desc, const char *label,
      const char *actual_string, const char *path,
      file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char *output_label               = NULL;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;
   tmp[0] = '\0';

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

   fill_pathname_noext(tmp, desc, ": ", sizeof(tmp));
   strlcat(tmp, actual_string, sizeof(tmp));
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int create_string_list_rdb_entry_int(
      enum msg_hash_enums enum_idx,
      const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   char str[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char *output_label               = NULL;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();

   if (!str_list)
      return -1;

   attr.i = 0;
   tmp[0] = str[0] = '\0';

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
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static int menu_displaylist_parse_database_entry(menu_displaylist_info_t *info)
{
   unsigned i, j, k;
   char path_playlist[PATH_MAX_LENGTH];
   char path_base[PATH_MAX_LENGTH];
   char query[PATH_MAX_LENGTH];
   playlist_t *playlist                = NULL;
   database_info_list_t *db_info       = NULL;
   menu_handle_t *menu                 = NULL;
   settings_t *settings                = config_get_ptr();

   path_playlist[0] = path_base[0] = query[0] = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto error;

   database_info_build_query_enum(query, sizeof(query),
         DATABASE_QUERY_ENTRY, info->path_b);

   db_info = database_info_list_new(info->path, query);
   if (!db_info)
      goto error;

   fill_short_pathname_representation_noext(path_base, info->path,
         sizeof(path_base));

   menu_driver_set_thumbnail_system(path_base, sizeof(path_base));

   strlcat(path_base,
         file_path_str(FILE_PATH_LPL_EXTENSION),
         sizeof(path_base));

   fill_pathname_join(path_playlist,
         settings->paths.directory_playlist, path_base,
         sizeof(path_playlist));

   playlist = playlist_init(path_playlist, COLLECTION_SIZE);

   if (playlist)
      strlcpy(menu->db_playlist_file, path_playlist,
            sizeof(menu->db_playlist_file));

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH];
      char thumbnail_content[PATH_MAX_LENGTH];
      char crc_str[20];
      database_info_t *db_info_entry = &db_info->list[i];
      bool show_advanced_settings    = settings->bools.menu_show_advanced_settings;

      crc_str[0] = tmp[0] = thumbnail_content[0] = '\0';

      snprintf(crc_str, sizeof(crc_str), "%08X", db_info_entry->crc32);

      if (!string_is_empty(db_info_entry->name))
         strlcpy(thumbnail_content, db_info_entry->name, sizeof(thumbnail_content));

      if (!string_is_empty(thumbnail_content))
         menu_driver_set_thumbnail_content(thumbnail_content, sizeof(thumbnail_content));

      menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH, NULL);
      menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE, NULL);

      if (playlist)
      {
         for (j = 0; j < playlist_size(playlist); j++)
         {
            char elem0[PATH_MAX_LENGTH];
            char elem1[PATH_MAX_LENGTH];
            const char *crc32                = NULL;
            bool match_found                 = false;
            struct string_list *tmp_str_list = NULL;

            elem0[0] = elem1[0] = '\0';

            playlist_get_index(playlist, j,
                  NULL, NULL, NULL, NULL,
                  NULL, &crc32);

            tmp_str_list                     = string_split(crc32, "|");

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
               strlcpy(elem0, tmp_str_list->elems[0].data, sizeof(elem0));
            if (tmp_str_list->size > 1)
               strlcpy(elem1, tmp_str_list->elems[1].data, sizeof(elem1));

            switch (msg_hash_to_file_type(msg_hash_calculate(elem1)))
            {
               case FILE_TYPE_CRC:
                  if (string_is_equal(crc_str, elem0))
                     match_found = true;
                  break;
               case FILE_TYPE_SHA1:
                  if (string_is_equal(db_info_entry->sha1, elem0))
                     match_found = true;
                  break;
               case FILE_TYPE_MD5:
                  if (string_is_equal(db_info_entry->md5, elem0))
                     match_found = true;
                  break;
               default:
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
         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME),
               ": ",
               sizeof(tmp));
         strlcat(tmp, db_info_entry->name, sizeof(tmp));
         menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_NAME),
               MENU_ENUM_LABEL_RDB_ENTRY_NAME,
               0, 0, 0);
      }
      if (db_info_entry->description)
      {
         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION),
               ": ",
               sizeof(tmp));
         strlcat(tmp, db_info_entry->description, sizeof(tmp));
         menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION),
               MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION,
               0, 0, 0);
      }
      if (db_info_entry->genre)
      {
         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE),
               ": ",
               sizeof(tmp));
         strlcat(tmp, db_info_entry->genre, sizeof(tmp));
         menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_GENRE),
               MENU_ENUM_LABEL_RDB_ENTRY_GENRE,
               0, 0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER),
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
                        MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER),
                        msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER),
                        db_info_entry->developer->elems[k].data,
                        info->path, info->list) == -1)
                  goto error;
            }
         }
      }

      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN),
                  db_info_entry->origin, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE),
                  db_info_entry->franchise, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS),
                  db_info_entry->max_users,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->tgdb_rating)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_TGDB_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_TGDB_RATING),
                  db_info_entry->tgdb_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING),
                  db_info_entry->famitsu_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_review)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW),
                  db_info_entry->edge_magazine_review, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING),
                  db_info_entry->edge_magazine_rating,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE),
                  db_info_entry->edge_magazine_issue,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH),
                  db_info_entry->releasemonth,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int(
                  MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR),
                  db_info_entry->releaseyear,
                  info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING),
                  db_info_entry->bbfc_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING),
                  db_info_entry->esrb_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING),
                  db_info_entry->elspa_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING),
                  db_info_entry->pegi_rating, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW),
                  db_info_entry->enhancement_hw, info->path, info->list) == -1)
            goto error;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING),
                  db_info_entry->cero_rating, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->serial)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_SERIAL,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_SERIAL),
                  db_info_entry->serial, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->analog_supported == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ANALOG,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ANALOG),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->rumble_supported == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_RUMBLE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_RUMBLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->coop_supported == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_COOP,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_COOP),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (!show_advanced_settings)
         continue;

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_CRC32,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_CRC32),
                  crc_str,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_SHA1,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_SHA1),
                  db_info_entry->sha1,
                  info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_MD5,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_MD5),
                  db_info_entry->md5,
                  info->path, info->list) == -1)
            goto error;
      }
   }

   if (db_info->count < 1)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
            MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE,
            0, 0, 0);

   playlist_free(playlist);
   database_info_list_free(db_info);
   free(db_info);

   return 0;

error:
   if (db_info)
   {
      database_info_list_free(db_info);
      free(db_info);
   }
   playlist_free(playlist);

   return -1;
}

static int menu_database_parse_query(file_list_t *list, const char *path,
      const char *query)
{
   unsigned i;
   database_info_list_t *db_list = database_info_list_new(path, query);

   if (!db_list)
      return -1;

   for (i = 0; i < db_list->count; i++)
   {
      if (!string_is_empty(db_list->list[i].name))
         menu_entries_append_enum(list, db_list->list[i].name,
               path, MENU_ENUM_LABEL_RDB_ENTRY, FILE_TYPE_RDB_ENTRY, 0, 0);
   }

   database_info_list_free(db_list);
   free(db_list);

   return 0;
}
#endif


#ifdef HAVE_SHADER_MANAGER
static int deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   size_t list_size = shader->num_parameters;

   if (list_size == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_SHADER_PARAMETERS),
            MENU_ENUM_LABEL_NO_SHADER_PARAMETERS,
            0, 0, 0);
      return 0;
   }

   for (i = 0; i < list_size; i++)
      menu_entries_append_enum(info->list, shader->parameters[i].desc,
            info->label, MENU_ENUM_LABEL_SHADER_PARAMETERS_ENTRY,
            base_parameter + i, 0, 0);

   return 0;
}
#endif

static int menu_displaylist_parse_settings_internal(void *data,
      menu_displaylist_info_t *info,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry,
      rarch_setting_t *setting
      )
{
   enum setting_type precond;
   size_t             count  = 0;
   uint64_t flags            = 0;
   settings_t *settings      = config_get_ptr();

   if (!setting)
      return -1;

   flags = setting->flags;

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
      case PARSE_ONLY_HEX:
         precond = ST_HEX;
         break;
      case PARSE_ONLY_STRING:
         precond = ST_STRING;
         break;
      case PARSE_ONLY_PATH:
         precond = ST_PATH;
         break;
      case PARSE_ONLY_DIR:
         precond = ST_DIR;
         break;
      case PARSE_ONLY_STRING_OPTIONS:
         precond = ST_STRING_OPTIONS;
         break;
      case PARSE_ONLY_GROUP:
      default:
         precond = ST_END_GROUP;
         break;
   }

   for (;;)
   {
      bool time_to_exit             = false;
      const char *short_description = setting->short_description;
      const char *name              = setting->name;
      enum setting_type type        = setting_get_type(setting);
      rarch_setting_t **list        = &setting;

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
         case PARSE_ONLY_HEX:
            if (type == ST_HEX)
               break;
            goto loop;
         case PARSE_ONLY_STRING:
            if (type == ST_STRING)
               break;
            goto loop;
         case PARSE_ONLY_PATH:
            if (type == ST_PATH)
               break;
            goto loop;
         case PARSE_ONLY_DIR:
            if (type == ST_DIR)
               break;
            goto loop;
         case PARSE_ONLY_STRING_OPTIONS:
            if (type == ST_STRING_OPTIONS)
               break;
            goto loop;
      }

#ifdef HAVE_LAKKA
      if ((flags & SD_FLAG_ADVANCED || flags & SD_FLAG_LAKKA_ADVANCED) &&
            !settings->bools.menu_show_advanced_settings)
         goto loop;
#else
      if (flags & SD_FLAG_ADVANCED &&
            !settings->bools.menu_show_advanced_settings)
         goto loop;
#endif


      menu_entries_append(info->list, short_description,
            name, menu_setting_set_flags(setting), 0, 0);
      count++;

loop:
      switch (parse_type)
      {
         case PARSE_NONE:
         case PARSE_GROUP:
         case PARSE_ONLY_GROUP:
         case PARSE_SUB_GROUP:
            if (setting_get_type(setting) == precond)
               time_to_exit = true;
            break;
         case PARSE_ONLY_BIND:
         case PARSE_ONLY_FLOAT:
         case PARSE_ONLY_HEX:
         case PARSE_ONLY_BOOL:
         case PARSE_ONLY_INT:
         case PARSE_ONLY_UINT:
         case PARSE_ONLY_STRING:
         case PARSE_ONLY_PATH:
         case PARSE_ONLY_DIR:
         case PARSE_ONLY_STRING_OPTIONS:
         case PARSE_ACTION:
            time_to_exit = true;
            break;
      }

      if (time_to_exit)
         break;
      (*list = *list + 1);
   }

   if (count == 0 && add_empty_entry)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
            MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
            0, 0, 0);

   return 0;
}

static int menu_displaylist_parse_settings_internal_enum(void *data,
      menu_displaylist_info_t *info,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry,
      rarch_setting_t *setting,
      enum msg_hash_enums enum_idx
      )
{
   enum setting_type precond;
   size_t             count  = 0;
   uint64_t flags            = 0;
   settings_t *settings      = config_get_ptr();

   if (!setting)
      return -1;

   flags            = setting->flags;

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
      case PARSE_ONLY_HEX:
         precond = ST_HEX;
         break;
      case PARSE_ONLY_STRING:
         precond = ST_STRING;
         break;
      case PARSE_ONLY_PATH:
         precond = ST_PATH;
         break;
      case PARSE_ONLY_DIR:
         precond = ST_DIR;
         break;
      case PARSE_ONLY_STRING_OPTIONS:
         precond = ST_STRING_OPTIONS;
         break;
      case PARSE_ONLY_GROUP:
      default:
         precond = ST_END_GROUP;
         break;
   }

   for (;;)
   {
      bool time_to_exit             = false;
      const char *short_description = setting->short_description;
      const char *name              = setting->name;
      enum setting_type type        = setting_get_type(setting);
      rarch_setting_t **list        = &setting;

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
         case PARSE_ONLY_HEX:
            if (type == ST_HEX)
               break;
            goto loop;
         case PARSE_ONLY_STRING:
            if (type == ST_STRING)
               break;
            goto loop;
         case PARSE_ONLY_PATH:
            if (type == ST_PATH)
               break;
            goto loop;
         case PARSE_ONLY_DIR:
            if (type == ST_DIR)
               break;
            goto loop;
         case PARSE_ONLY_STRING_OPTIONS:
            if (type == ST_STRING_OPTIONS)
               break;
            goto loop;
      }

#ifdef HAVE_LAKKA
      if ((flags & SD_FLAG_ADVANCED || flags & SD_FLAG_LAKKA_ADVANCED) &&
            !settings->bools.menu_show_advanced_settings)
         goto loop;
#else
      if (flags & SD_FLAG_ADVANCED &&
            !settings->bools.menu_show_advanced_settings)
         goto loop;
#endif


      menu_entries_append_enum(info->list, short_description,
            name, enum_idx, menu_setting_set_flags(setting), 0, 0);
      count++;

loop:
      switch (parse_type)
      {
         case PARSE_NONE:
         case PARSE_GROUP:
         case PARSE_ONLY_GROUP:
         case PARSE_SUB_GROUP:
            if (setting_get_type(setting) == precond)
               time_to_exit = true;
            break;
         case PARSE_ONLY_BIND:
         case PARSE_ONLY_FLOAT:
         case PARSE_ONLY_HEX:
         case PARSE_ONLY_BOOL:
         case PARSE_ONLY_INT:
         case PARSE_ONLY_UINT:
         case PARSE_ONLY_PATH:
         case PARSE_ONLY_DIR:
         case PARSE_ONLY_STRING:
         case PARSE_ONLY_STRING_OPTIONS:
         case PARSE_ACTION:
            time_to_exit = true;
            break;
      }

      if (time_to_exit)
         break;
      (*list = *list + 1);
   }

   if (count == 0)
   {
      if (add_empty_entry)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
               MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
               0, 0, 0);
      return -1;
   }

   return 0;
}

static int menu_displaylist_parse_settings(void *data,
      menu_displaylist_info_t *info,
      const char *info_label,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry)
{
   return menu_displaylist_parse_settings_internal(data,
         info,
         parse_type,
         add_empty_entry,
         menu_setting_find(info_label)
         );
}

static int menu_displaylist_parse_settings_enum(void *data,
      menu_displaylist_info_t *info,
      enum msg_hash_enums label,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry)
{
   return menu_displaylist_parse_settings_internal_enum(data,
         info,
         parse_type,
         add_empty_entry,
         menu_setting_find_enum(label),
         label
         );
}

static void menu_displaylist_set_new_playlist(
      menu_handle_t *menu, const char *path)
{
   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_FREE, NULL);
   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_INIT,
         (void*)path);
   strlcpy(
         menu->db_playlist_file,
         path,
         sizeof(menu->db_playlist_file));
}


static int menu_displaylist_parse_horizontal_list(
      menu_displaylist_info_t *info)
{
   menu_ctx_list_t list_info;
   menu_ctx_list_t list_horiz_info;
   char lpl_basename[PATH_MAX_LENGTH];
   char path_playlist[PATH_MAX_LENGTH];
   bool is_historylist                 = false;
   playlist_t *playlist                = NULL;
   menu_handle_t        *menu          = NULL;
   struct item_file *item              = NULL;
   settings_t      *settings           = config_get_ptr();

   lpl_basename[0] = path_playlist[0]  = '\0';

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

   fill_pathname_base_noext(lpl_basename, item->path, sizeof(lpl_basename));

   fill_pathname_join(
         path_playlist,
         settings->paths.directory_playlist,
         item->path,
         sizeof(path_playlist));

   menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));

   menu_displaylist_set_new_playlist(menu, path_playlist);

   strlcpy(path_playlist,
         msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION),
         sizeof(path_playlist));

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   playlist_qsort(playlist);

   if (string_is_equal_fast(lpl_basename, "content_history", 15))
      is_historylist = true;

   menu_displaylist_parse_playlist(info,
         playlist, path_playlist, is_historylist);

   return 0;
}

static int menu_displaylist_parse_load_content_settings(
      menu_displaylist_info_t *info)
{
   menu_handle_t *menu    = NULL;
#if defined(HAVE_CHEEVOS) || defined(HAVE_LAKKA)
   settings_t *settings   = config_get_ptr();
#endif

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
#ifdef HAVE_LAKKA
      bool show_advanced_settings    = settings->bools.menu_show_advanced_settings;
#endif
      rarch_system_info_t *system    = runloop_get_system_info();

#if 0
      const struct retro_subsystem_info* subsystem = system ? system->subsystem.data : NULL;

      if (subsystem)
      {
         unsigned p;

         for (p = 0; p < system->subsystem.size; p++, subsystem++)
         {
            char s[PATH_MAX_LENGTH];
            snprintf(s, sizeof(s), "%s (%s)", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST), subsystem->desc);
            menu_entries_append_enum(info->list,
                  s,
                  msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL),
                  MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL,
                  MENU_SETTING_ACTION, 0, 0);
         }
      }
#endif
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT),
            msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT),
            MENU_ENUM_LABEL_RESUME_CONTENT,
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT),
            msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT),
            MENU_ENUM_LABEL_RESTART_CONTENT,
            MENU_SETTING_ACTION_RUN, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT),
            msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT),
            MENU_ENUM_LABEL_CLOSE_CONTENT,
            MENU_SETTING_ACTION_CLOSE, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
            msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
            MENU_ENUM_LABEL_TAKE_SCREENSHOT,
            MENU_SETTING_ACTION_SCREENSHOT, 0, 0);

      menu_displaylist_parse_settings_enum(menu, info,
            MENU_ENUM_LABEL_STATE_SLOT, PARSE_ONLY_INT, true);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_STATE),
            msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE),
            MENU_ENUM_LABEL_SAVE_STATE,
            MENU_SETTING_ACTION_SAVESTATE, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_STATE),
            msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE),
            MENU_ENUM_LABEL_LOAD_STATE,
            MENU_SETTING_ACTION_LOADSTATE, 0, 0);

#ifdef HAVE_LAKKA
      if (show_advanced_settings)
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE),
               MENU_ENUM_LABEL_UNDO_LOAD_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0);

#ifdef HAVE_LAKKA
      if (show_advanced_settings)
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE),
               MENU_ENUM_LABEL_UNDO_SAVE_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES),
            msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES),
            MENU_ENUM_LABEL_ADD_TO_FAVORITES, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS),
            MENU_ENUM_LABEL_CORE_OPTIONS,
            MENU_SETTING_ACTION, 0, 0);

#if 0
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_SETTINGS),
            MENU_ENUM_LABEL_NETPLAY_SETTINGS,
            MENU_SETTING_ACTION, 0, 0);
#endif

      if (core_has_set_input_descriptor())
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS),
               MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS,
               MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LAKKA
      if (show_advanced_settings)
#endif
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS),
            MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS,
            MENU_SETTING_ACTION, 0, 0);
      if (     (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            && system->disk_control_cb.get_num_images)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS),
               MENU_ENUM_LABEL_DISK_OPTIONS,
               MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0);
#ifdef HAVE_SHADER_MANAGER
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS),
            MENU_ENUM_LABEL_SHADER_OPTIONS,
            MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_LAKKA
      if (show_advanced_settings)
#endif
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
            MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
            MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LAKKA
      if (show_advanced_settings)
#endif
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
            msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
            MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
            MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_CHEEVOS
      if(settings->bools.cheevos_enable)
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST),
            MENU_ENUM_LABEL_ACHIEVEMENT_LIST,
            MENU_SETTING_ACTION, 0, 0);
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE),
            MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE,
            MENU_SETTING_ACTION, 0, 0);
      }
#endif
   }
   else
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
            MENU_ENUM_LABEL_NO_ITEMS,
            MENU_SETTING_NO_ITEM, 0, 0);

   return 0;
}

static int menu_displaylist_parse_horizontal_content_actions(
      menu_displaylist_info_t *info)
{
   unsigned idx                    = rpl_entry_selection_ptr;
   menu_handle_t *menu             = NULL;
   const char *label               = NULL;
   const char *entry_path          = NULL;
   const char *core_path           = NULL;
   const char *core_name           = NULL;
   const char *db_name             = NULL;
   playlist_t *playlist            = NULL;
   settings_t *settings            = config_get_ptr();
   const char *fullpath            = path_get(RARCH_PATH_CONTENT);

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   playlist_get_index(playlist, idx,
         &entry_path, &label, &core_path, &core_name, NULL, &db_name);

   if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, fullpath))
      menu_displaylist_parse_load_content_settings(info);
   else
   {
      const char *ext = NULL;

      if (!string_is_empty(entry_path))
         ext = path_get_extension(entry_path);

      if (!string_is_empty(ext) &&
            audio_driver_mixer_extension_supported(ext))
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER),
               MENU_ENUM_LABEL_ADD_TO_MIXER,
               FILE_TYPE_PLAYLIST_ENTRY, 0, idx);
      }

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN),
            msg_hash_to_str(MENU_ENUM_LABEL_RUN),
            MENU_ENUM_LABEL_RUN, FILE_TYPE_PLAYLIST_ENTRY, 0, idx);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME),
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME),
            MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME, FILE_TYPE_PLAYLIST_ENTRY, 0, idx);

	  if (settings->bools.playlist_entry_remove)
	  menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY),
            msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY),
            MENU_ENUM_LABEL_DELETE_ENTRY,
            MENU_SETTING_ACTION_DELETE_ENTRY, 0, 0);
   }

   if (!string_is_empty(db_name))
   {
      char db_path[PATH_MAX_LENGTH];

      db_path[0] = '\0';

      fill_pathname_join_noext(db_path, settings->paths.path_content_database,
            db_name, sizeof(db_path));
      strlcat(db_path, file_path_str(FILE_PATH_RDB_EXTENSION),
            sizeof(db_path));

      if (path_file_exists(db_path))
      menu_entries_append_enum(info->list, label,
            db_path,
            MENU_ENUM_LABEL_INFORMATION, FILE_TYPE_RDB_ENTRY, 0, idx);
   }

   return 0;
}

static int menu_displaylist_parse_information_list(
      menu_displaylist_info_t *info)
{
   core_info_t *core_info         = NULL;
   rarch_system_info_t *system    = runloop_get_system_info();

   core_info_get_current_core(&core_info);

   if (  system &&
         (!string_is_empty(system->info.library_name) &&
          !string_is_equal(system->info.library_name,
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
         )
         && core_info && core_info->config_data
      )
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_INFORMATION),
            MENU_ENUM_LABEL_CORE_INFORMATION,
            MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_NETWORKING
#ifndef HAVE_SOCKET_LEGACY
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION),
         MENU_ENUM_LABEL_NETWORK_INFORMATION,
         MENU_SETTING_ACTION, 0, 0);
#endif
#endif

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_INFORMATION),
         MENU_ENUM_LABEL_SYSTEM_INFORMATION,
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST),
         MENU_ENUM_LABEL_DATABASE_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST),
         MENU_ENUM_LABEL_CURSOR_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0);
#endif

   if (rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_FRONTEND_COUNTERS),
            MENU_ENUM_LABEL_FRONTEND_COUNTERS,
            MENU_SETTING_ACTION, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_COUNTERS),
            MENU_ENUM_LABEL_CORE_COUNTERS,
            MENU_SETTING_ACTION, 0, 0);
   }

   return 0;
}

static int menu_displaylist_parse_configurations_list(
      menu_displaylist_info_t *info)
{
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATIONS),
         msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS),
         MENU_ENUM_LABEL_CONFIGURATIONS,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG),
         msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG),
         MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG),
         msg_hash_to_str(MENU_ENUM_LABEL_SAVE_NEW_CONFIG),
         MENU_ENUM_LABEL_SAVE_NEW_CONFIG,
         MENU_SETTING_ACTION, 0, 0);
   return 0;
}

static int menu_displaylist_parse_add_content_list(
      menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
         MENU_ENUM_LABEL_SCAN_DIRECTORY,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
         MENU_ENUM_LABEL_SCAN_FILE,
         MENU_SETTING_ACTION, 0, 0);
#endif

#if !defined(HAVE_NETWORKING) && !defined(HAVE_LIBRETRODB)
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
         msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
         MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
         FILE_TYPE_NONE, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_scan_directory_list(
      menu_displaylist_info_t *info)
{

#ifdef HAVE_LIBRETRODB
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
         MENU_ENUM_LABEL_SCAN_DIRECTORY,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
         MENU_ENUM_LABEL_SCAN_FILE,
         MENU_SETTING_ACTION, 0, 0);
#endif

   return 0;
}

static int menu_displaylist_parse_netplay_room_list(
      menu_displaylist_info_t *info)
{

#ifdef HAVE_NETWORKING
   netplay_refresh_rooms_menu(info->list);
#endif

   return 0;
}

static int menu_displaylist_parse_options(
      menu_displaylist_info_t *info)
{
#ifdef HAVE_LAKKA
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_LAKKA),
         MENU_ENUM_LABEL_UPDATE_LAKKA,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
         msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
         MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
         MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
         MENU_SETTING_ACTION, 0, 0);
#else
#ifdef HAVE_NETWORKING
   settings_t *settings         = config_get_ptr();

   if (settings->bools.menu_show_core_updater)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
            MENU_ENUM_LABEL_CORE_UPDATER_LIST,
            MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
         msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
         MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_NETWORKING
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
         MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
         MENU_SETTING_ACTION, 0, 0);
#endif

   if (settings->bools.menu_show_core_updater)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES),
            msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES),
            MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES,
            MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_UPDATE_ASSETS
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_ASSETS),
         MENU_ENUM_LABEL_UPDATE_ASSETS,
         MENU_SETTING_ACTION, 0, 0);
#endif

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES),
         MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CHEATS),
         MENU_ENUM_LABEL_UPDATE_CHEATS,
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_LIBRETRODB
#if !defined(VITA)
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_DATABASES),
         MENU_ENUM_LABEL_UPDATE_DATABASES,
         MENU_SETTING_ACTION, 0, 0);
#endif
#endif

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_OVERLAYS),
         MENU_ENUM_LABEL_UPDATE_OVERLAYS,
         MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_CG
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CG_SHADERS),
         MENU_ENUM_LABEL_UPDATE_CG_SHADERS,
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_GLSL
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS),
         MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS,
         MENU_SETTING_ACTION, 0, 0);
#endif

#ifdef HAVE_VULKAN
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS),
         MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS,
         MENU_SETTING_ACTION, 0, 0);
#endif

#else
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
         msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
         MENU_ENUM_LABEL_NO_ITEMS,
         MENU_SETTING_NO_ITEM, 0, 0);
#endif
#endif

   return 0;
}

static int menu_displaylist_parse_options_cheats(
      menu_displaylist_info_t *info)
{
   unsigned i;

   if (!cheat_manager_alloc_if_empty())
      return -1;

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES),
         MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD),
         MENU_ENUM_LABEL_CHEAT_FILE_LOAD,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS),
         MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_NUM_PASSES),
         MENU_ENUM_LABEL_CHEAT_NUM_PASSES,
         0, 0, 0);

   for (i = 0; i < cheat_manager_get_size(); i++)
   {
      char cheat_label[64];

      cheat_label[0] = '\0';

      snprintf(cheat_label, sizeof(cheat_label),
            "%s #%u: ", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT), i);
      if (cheat_manager_get_desc(i))
         strlcat(cheat_label, cheat_manager_get_desc(i), sizeof(cheat_label));
      menu_entries_append_enum(info->list,
            cheat_label, "", MSG_UNKNOWN,
            MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0);
   }

   return 0;
}

static int menu_displaylist_parse_options_remappings(
      menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   rarch_system_info_t *system = NULL;
   menu_handle_t       *menu   = NULL;
   unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return -1;

   for (p = 0; p < max_users; p++)
   {
      char key_type[PATH_MAX_LENGTH];
      char key_analog[PATH_MAX_LENGTH];
      unsigned val = p + 1;

      key_type[0] = key_analog[0] = '\0';

      snprintf(key_type, sizeof(key_type),
            msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE), val);
      snprintf(key_analog, sizeof(key_analog),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE), val);

      menu_displaylist_parse_settings(menu, info,
            key_type, PARSE_ONLY_UINT, true);
      menu_displaylist_parse_settings(menu, info,
            key_analog, PARSE_ONLY_UINT, true);
   }

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_LOAD),
         MENU_ENUM_LABEL_REMAP_FILE_LOAD,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE),
         MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME),
         MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME,
         MENU_SETTING_ACTION, 0, 0);

   if (rarch_ctl(RARCH_CTL_IS_REMAPS_CORE_ACTIVE, NULL))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE,
            MENU_SETTING_ACTION, 0, 0);
   }

   if (rarch_ctl(RARCH_CTL_IS_REMAPS_GAME_ACTIVE, NULL))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME,
            MENU_SETTING_ACTION, 0, 0);
   }

   system    = runloop_get_system_info();

   if (system)
   {
      for (p = 0; p < max_users; p++)
      {
         for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND + 4; retro_id++)
         {
            char desc_label[64];
            unsigned user           = p + 1;
            unsigned desc_offset    = retro_id;
            const char *description = NULL;

            desc_label[0]           = '\0';

            if (desc_offset >= RARCH_FIRST_CUSTOM_BIND)
               desc_offset = RARCH_FIRST_CUSTOM_BIND
                  + (desc_offset - RARCH_FIRST_CUSTOM_BIND) * 2;

            description = system->input_desc_btn[p][desc_offset];

            if (!description)
               continue;

            snprintf(desc_label, sizeof(desc_label),
                  "%s %u %s : ", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER),
                  user, description);
            menu_entries_append_enum(info->list, desc_label, "",
                  MSG_UNKNOWN,
                  MENU_SETTINGS_INPUT_DESC_BEGIN +
                  (p * (RARCH_FIRST_CUSTOM_BIND + 4)) +  retro_id, 0, 0);
         }
      }
   }

   return 0;
}

static int menu_displaylist_parse_playlists(
      menu_displaylist_info_t *info, bool horizontal)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;
   unsigned items_found         = 0;
   settings_t *settings         = config_get_ptr();

   if (string_is_empty(info->path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
      return 0;
   }

   str_list = dir_list_new(info->path, NULL, true, settings->bools.show_hidden_files, true, false);

   if (!str_list)
   {
      const char *str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND);
      (void)str;
      return 0;
   }

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (!horizontal)
   {
#ifdef HAVE_LIBRETRODB
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
            msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
            MENU_ENUM_LABEL_SCAN_DIRECTORY,
            MENU_SETTING_ACTION, 0, 0);
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
            msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
            MENU_ENUM_LABEL_SCAN_FILE,
            MENU_SETTING_ACTION, 0, 0);
#endif
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
            MENU_ENUM_LABEL_GOTO_FAVORITES,
            MENU_SETTING_ACTION, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
            MENU_ENUM_LABEL_GOTO_IMAGES,
            MENU_SETTING_ACTION, 0, 0);

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
            MENU_ENUM_LABEL_GOTO_MUSIC,
            MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_FFMPEG
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
            MENU_ENUM_LABEL_GOTO_VIDEO,
            MENU_SETTING_ACTION, 0, 0);
#endif
   }

   if (list_size == 0)
   {
      string_list_free(str_list);

      if (!horizontal)
         goto no_playlists;

      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      char label[PATH_MAX_LENGTH];
      const char *path              = NULL;
      enum msg_file_type file_type  = FILE_TYPE_NONE;

      label[0] = '\0';

      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = FILE_TYPE_DIRECTORY;
            break;
         case RARCH_PLAIN_FILE:
         default:
            file_type = (enum msg_file_type)info->type_default;
            break;
      }

      if (file_type == FILE_TYPE_DIRECTORY)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (!strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)) ||
         ((strcasestr(path, "content") && strcasestr(path, "history"))))
         continue;

      file_type = FILE_TYPE_PLAYLIST_COLLECTION;

      if (!string_is_empty(info->path))
         path = path_basename(path);

      items_found++;
      menu_entries_append_enum(info->list, path, label,
            MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY,
            file_type, 0, 0);
   }

   string_list_free(str_list);

   if (items_found == 0 && !horizontal)
      goto no_playlists;

   return 0;

no_playlists:
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS),
         msg_hash_to_str(MENU_ENUM_LABEL_NO_PLAYLISTS),
         MENU_ENUM_LABEL_NO_PLAYLISTS,
         MENU_SETTING_NO_ITEM, 0, 0);
   return 0;
}

static int menu_displaylist_parse_cores(
      menu_handle_t       *menu,
      menu_displaylist_info_t *info)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;
   unsigned items_found         = 0;
   settings_t *settings         = config_get_ptr();

   if (string_is_empty(info->path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
      return 0;
   }

   str_list = dir_list_new(info->path, info->exts,
         true, settings->bools.show_hidden_files, true, false);

   {
      char out_dir[PATH_MAX_LENGTH];

      out_dir[0] = '\0';

      fill_pathname_parent_dir(out_dir, info->path, sizeof(out_dir));

      if (string_is_empty(out_dir))
      {
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY),
               info->path,
               MENU_ENUM_LABEL_PARENT_DIRECTORY,
               FILE_TYPE_PARENT_DIRECTORY, 0, 0);
      }
   }

   if (!str_list)
   {
      const char *str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      menu_entries_append_enum(info->list, str, "",
            MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND, 0, 0, 0);
      return 0;
   }

   info->download_core = true;

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
            MENU_ENUM_LABEL_NO_ITEMS,
            MENU_SETTING_NO_ITEM, 0, 0);

      string_list_free(str_list);

      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      char label[PATH_MAX_LENGTH];
      const char *path              = NULL;
      enum msg_hash_enums enum_idx  = MSG_UNKNOWN;
      enum msg_file_type file_type  = FILE_TYPE_NONE;

      label[0] = '\0';

      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = FILE_TYPE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = FILE_TYPE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = FILE_TYPE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            file_type = (enum msg_file_type)info->type_default;
            break;
      }

      is_dir = (file_type == FILE_TYPE_DIRECTORY);

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (!string_is_empty(info->path))
         path = path_basename(path);

#ifndef HAVE_DYNAMIC
      if (frontend_driver_has_fork())
      {
         char salamander_name[PATH_MAX_LENGTH];

         salamander_name[0] = '\0';

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
      if (file_type == FILE_TYPE_CARCHIVE)
         continue;

      if (is_dir)
      {
         file_type = FILE_TYPE_DIRECTORY;
         enum_idx  = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
      }
      else
      {
         file_type = FILE_TYPE_CORE;
         enum_idx  = MENU_ENUM_LABEL_FILE_BROWSER_CORE;
      }

      items_found++;

      menu_entries_append_enum(info->list, path, label,
            enum_idx,
            file_type, 0, 0);
   }

   string_list_free(str_list);

   if (items_found == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
            MENU_ENUM_LABEL_NO_ITEMS,
            MENU_SETTING_NO_ITEM, 0, 0);

      return 0;
   }

   {
      enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
      core_info_list_t *list         = NULL;
      const char *dir                = NULL;

      core_info_get_list(&list);

      menu_entries_get_last_stack(&dir, NULL, NULL, &enum_idx, NULL);

      list_size = file_list_get_size(info->list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX_LENGTH];
         char display_name[PATH_MAX_LENGTH];
         unsigned type                      = 0;
         const char *path                   = NULL;

         core_path[0] = display_name[0]     = '\0';

         menu_entries_get_at_offset(info->list,
               i, &path, NULL, &type, NULL,
               NULL);

         if (type != FILE_TYPE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (core_info_list_get_display_name(list,
                  core_path, display_name, sizeof(display_name)))
            menu_entries_set_alt_at_offset(info->list, i, display_name);
      }
      info->need_sort = true;
   }


   return 0;
}

static void menu_displaylist_parse_playlist_associations(
      menu_displaylist_info_t *info)
{
   settings_t      *settings    = config_get_ptr();
   struct string_list *str_list = dir_list_new_special(
         settings->paths.directory_playlist,
         DIR_LIST_COLLECTIONS, NULL);

   if (str_list && str_list->size)
   {
      unsigned i;
      char new_playlist_names[PATH_MAX_LENGTH];
      char new_playlist_cores[PATH_MAX_LENGTH];
      struct string_list *stnames  = string_split(settings->arrays.playlist_names, ";");
      struct string_list *stcores  = string_split(settings->arrays.playlist_cores, ";");

      new_playlist_names[0] = new_playlist_cores[0] = '\0';

      for (i = 0; i < str_list->size; i++)
      {
         char path_base[PATH_MAX_LENGTH];
         char core_path[PATH_MAX_LENGTH];
         union string_list_elem_attr attr;
         unsigned found                   = 0;
         const char *path                 =
            path_basename(str_list->elems[i].data);

         attr.i       = 0;
         path_base[0] = core_path[0]      = '\0';

         if (!menu_content_playlist_find_associated_core(
                  path, core_path, sizeof(core_path)))
            strlcpy(core_path, file_path_str(FILE_PATH_DETECT), sizeof(core_path));

         strlcpy(path_base, path, sizeof(path_base));

         found = string_list_find_elem(stnames, path_base);

         if (found)
            string_list_set(stcores, found-1, core_path);
         else
         {
            string_list_append(stnames, path_base, attr);
            string_list_append(stcores, core_path, attr);
         }

         path_remove_extension(path_base);
         menu_entries_append_enum(info->list,
               path_base,
               path,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY,
               MENU_SETTINGS_PLAYLIST_ASSOCIATION_START + i,
               0, 0);
      }

      string_list_join_concat(new_playlist_names,
            sizeof(new_playlist_names), stnames, ";");
      string_list_join_concat(new_playlist_cores,
            sizeof(new_playlist_cores), stcores, ";");

      strlcpy(settings->arrays.playlist_names,
            new_playlist_names, sizeof(settings->arrays.playlist_names));
      strlcpy(settings->arrays.playlist_cores,
            new_playlist_cores, sizeof(settings->arrays.playlist_cores));

      string_list_free(stnames);
      string_list_free(stcores);
   }

   string_list_free(str_list);
}

static bool menu_displaylist_push_internal(
      const char *label,
      menu_displaylist_ctx_entry_t *entry,
      menu_displaylist_info_t *info)
{
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_FAVORITES, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SETTINGS_ALL, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;
      strlcpy(info->exts,
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT),
            sizeof(info->exts));
      strlcpy(info->label,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
            sizeof(info->label));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_MUSIC_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;
      strlcpy(info->exts,
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT),
            sizeof(info->exts));
      strlcpy(info->label,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
            sizeof(info->label));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_VIDEO_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;
      strlcpy(info->exts,
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT),
            sizeof(info->exts));
      strlcpy(info->label,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
            sizeof(info->label));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#if 0
      if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
               MENU_ENUM_LABEL_TAKE_SCREENSHOT,
               MENU_SETTING_ACTION_SCREENSHOT, 0, 0);
      else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
               msg_hash_to_str(
                  MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
               MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE,
               MENU_INFO_MESSAGE, 0, 0);

#endif
      menu_displaylist_ctl(DISPLAYLIST_IMAGES_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      settings_t *settings  = config_get_ptr();

      filebrowser_clear_type();
      info->type = 42;
      strlcpy(info->exts,
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT),
            sizeof(info->exts));
      strlcpy(info->label,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
            sizeof(info->label));

      if (string_is_empty(settings->paths.directory_playlist))
      {
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
               msg_hash_to_str(
                  MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
               MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE,
               MENU_INFO_MESSAGE, 0, 0);
         info->need_refresh = true;
         info->need_push    = true;

         return true;
      }
      else
      {
         strlcpy(
               info->path,
               settings->paths.directory_playlist,
               sizeof(info->path));

         if (menu_displaylist_ctl(
                  DISPLAYLIST_DATABASE_PLAYLISTS, info))
            return true;
      }
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_SCAN_DIRECTORY_LIST, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_NETPLAY_ROOM_LIST, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_HORIZONTAL, info))
         return true;
   }

   return false;
}

bool menu_displaylist_push(menu_displaylist_ctx_entry_t *entry)
{
   menu_file_list_cbs_t *cbs      = NULL;
   const char *path               = NULL;
   const char *label              = NULL;
   unsigned type                  = 0;
   enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
   menu_displaylist_info_t info   = {0};

   if (!entry)
      return false;

   menu_entries_get_last_stack(&path, &label, &type, &enum_idx, NULL);

   info.list      = entry->list;
   info.menu_list = entry->stack;
   info.type      = type;
   info.enum_idx  = enum_idx;

   if (!string_is_empty(path))
      strlcpy(info.path,  path,  sizeof(info.path));

   if (!string_is_empty(label))
      strlcpy(info.label, label, sizeof(info.label));

   if (!info.list)
      return false;

   if (menu_displaylist_push_internal(label, entry, &info))
      return menu_displaylist_process(&info);

   cbs = menu_entries_get_last_stack_actiondata();

   if (cbs && cbs->action_deferred_push)
   {
      if (cbs->action_deferred_push(&info) != 0)
         return -1;
   }

   return true;
}

static void menu_displaylist_parse_playlist_history(
      menu_handle_t *menu,
      menu_displaylist_info_t *info,
      const char *playlist_name,
      const char *playlist_path,
      int *ret)
{
   char path_playlist[PATH_MAX_LENGTH];
   playlist_t *playlist = NULL;

   path_playlist[0] = '\0';

   menu_displaylist_set_new_playlist(menu, playlist_path);

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   strlcpy(path_playlist, playlist_name, sizeof(path_playlist));
   *ret = menu_displaylist_parse_playlist(info,
         playlist, path_playlist, true);
}

#ifdef HAVE_NETWORKING
static void wifi_scan_callback(void *task_data,
      void *user_data, const char *error)
{
   unsigned i;
   file_list_t *file_list        = NULL;
   struct string_list *ssid_list = NULL;

   const char *path              = NULL;
   const char *label             = NULL;
   unsigned menu_type            = 0;
   enum msg_hash_enums enum_idx  = MSG_UNKNOWN;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the wifi menu */
   if (!string_is_equal(label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST)))
      return;

   file_list = menu_entries_get_selection_buf_ptr(0);
   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, file_list);

   ssid_list = string_list_new();

   driver_wifi_get_ssids(ssid_list);

   for (i = 0; i < ssid_list->size; i++)
   {
      const char *ssid = ssid_list->elems[i].data;
      menu_entries_append_enum(file_list,
            ssid,
            msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_WIFI),
            MENU_ENUM_LABEL_CONNECT_WIFI,
            MENU_WIFI, 0, 0);
   }

   string_list_free(ssid_list);
}
#endif

bool menu_displaylist_process(menu_displaylist_info_t *info)
{
   size_t idx   = 0;
#if defined(HAVE_NETWORKING)
   settings_t *settings         = config_get_ptr();
#endif

   if (info->need_navigation_clear)
   {
      bool pending_push = true;
      menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }

   if (info->need_entries_refresh)
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   }

   if (info->need_sort)
      file_list_sort_on_alt(info->list);

#if defined(HAVE_NETWORKING)
   if (settings->bools.menu_show_core_updater)
      if (info->download_core)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
               MENU_ENUM_LABEL_CORE_UPDATER_LIST,
               MENU_SETTING_ACTION, 0, 0);
      }
#endif

   if (info->push_builtin_cores)
   {
#if defined(HAVE_VIDEO_PROCESSOR)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR),
            msg_hash_to_str(MENU_ENUM_LABEL_START_VIDEO_PROCESSOR),
            MENU_ENUM_LABEL_START_VIDEO_PROCESSOR,
            MENU_SETTING_ACTION, 0, 0);
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD) && defined(HAVE_NETWORKGAMEPAD_CORE)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD),
            msg_hash_to_str(MENU_ENUM_LABEL_START_NET_RETROPAD),
            MENU_ENUM_LABEL_START_NET_RETROPAD,
            MENU_SETTING_ACTION, 0, 0);
#endif
   }

   if (!string_is_empty(new_entry))
   {
      menu_entries_prepend(info->list,
            new_path_entry,
            new_lbl_entry,
            new_type,
            FILE_TYPE_CORE, 0, 0);
      menu_entries_set_alt_at_offset(info->list, 0,
            new_entry);

      new_type          = MSG_UNKNOWN;
      new_lbl_entry[0]  = '\0';
      new_path_entry[0] = '\0';
      new_entry[0]      = '\0';
   }

   if (info->need_refresh)
      menu_entries_ctl(MENU_ENTRIES_CTL_REFRESH, info->list);

   if (info->need_clear)
      menu_navigation_set_selection(idx);

   if (info->need_push)
   {
      info->label_hash = msg_hash_calculate(info->label);
      menu_driver_populate_entries(info);
      ui_companion_driver_notify_list_loaded(info->list, info->menu_list);
   }
   return true;
}

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type, void *data)
{
   size_t i;
   menu_ctx_displaylist_t disp_list;
   int ret                       = 0;
   core_info_list_t *list        = NULL;
   menu_handle_t       *menu     = NULL;
   bool load_content             = true;
   bool use_filebrowser          = false;
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;
   settings_t      *settings     = config_get_ptr();

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;

   core_info_get_list(&list);

   disp_list.info = info;
   disp_list.type = type;

   if (menu_driver_push_list(&disp_list))
      return true;

   switch (type)
   {
      case DISPLAYLIST_MUSIC_LIST:
         {
            char combined_path[PATH_MAX_LENGTH];
            const char *ext = NULL;

            combined_path[0] = '\0';

            fill_pathname_join(combined_path, menu->scratch2_buf,
                  menu->scratch_buf, sizeof(combined_path));

            ext = path_get_extension(combined_path);

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (audio_driver_mixer_extension_supported(ext))
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION),
                     msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION),
                     MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION,
                     FILE_TYPE_PLAYLIST_ENTRY, 0, 0);

#ifdef HAVE_FFMPEG
            if (settings->bools.multimedia_builtin_mediaplayer_enable)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN_MUSIC),
                     msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC),
                     MENU_ENUM_LABEL_RUN_MUSIC,
                     FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
#endif
         }
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      case DISPLAYLIST_NETPLAY_LAN_SCAN_SETTINGS_LIST:
         /* TODO/FIXME ? */
         break;
      case DISPLAYLIST_OPTIONS_MANAGEMENT:
         /* TODO/FIXME ? */
         break;
      case DISPLAYLIST_NETPLAY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->need_push    = true;
         /* TODO/FIXME ? */
         break;
      case DISPLAYLIST_DATABASE_ENTRY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
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

#ifdef HAVE_LIBRETRODB
         ret = menu_displaylist_parse_database_entry(info);
#else
         ret = 0;
#endif

         info->need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_QUERY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_LIBRETRODB
         ret = menu_database_parse_query(info->list,
               info->path, string_is_empty(info->path_c)
               ? NULL : info->path_c);
#else
         ret = 0;
#endif
         strlcpy(info->path, info->path_b, sizeof(info->path));

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_shader_options(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_CORE_CONTENT:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf, "",
               (int)core_len, FILE_TYPE_DOWNLOAD_CORE_CONTENT, true, false);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_CORE_CONTENT_DIRS_SUBDIR:
         {
#ifdef HAVE_NETWORKING
            char new_label[PATH_MAX_LENGTH];
            struct string_list *str_list = string_split(info->path, ";");

            new_label[0] = '\0';

            if (str_list->elems[0].data)
               strlcpy(new_label, str_list->elems[0].data, sizeof(new_label));
            if (str_list->elems[1].data)
               strlcpy(core_buf, str_list->elems[1].data, core_len);
            print_buf_lines(info->list, core_buf, new_label,
                  (int)core_len, FILE_TYPE_DOWNLOAD_URL, false, false);
            info->need_push    = true;
            info->need_refresh = true;
            info->need_clear   = true;

            string_list_free(str_list);
#endif
         }
         break;
      case DISPLAYLIST_CORE_CONTENT_DIRS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#ifdef HAVE_NETWORKING
            char new_label[PATH_MAX_LENGTH];

            new_label[0] = '\0';

            fill_pathname_join(new_label,
                  settings->paths.network_buildbot_assets_url,
                  "cores", sizeof(new_label));
            print_buf_lines(info->list, core_buf, new_label,
                  (int)core_len, FILE_TYPE_DOWNLOAD_URL, true, false);
            info->need_push    = true;
            info->need_refresh = true;
            info->need_clear   = true;
#endif
         }
         break;
      case DISPLAYLIST_CORES_UPDATER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf, "",
               (int)core_len, FILE_TYPE_DOWNLOAD_CORE, true, true);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_THUMBNAILS_UPDATER:
#ifdef HAVE_NETWORKING
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         print_buf_lines(info->list, core_buf, "",
               (int)core_len, FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT,
               true, false);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_LAKKA:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, core_buf, "",
               (int)core_len, FILE_TYPE_DOWNLOAD_LAKKA,
               true, false);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_PLAYLIST_COLLECTION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (string_is_equal(info->path, file_path_str(FILE_PATH_CONTENT_HISTORY)))
         {
            if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info))
               return menu_displaylist_process(info);
            return false;
         }
         else if (string_is_equal(info->path, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
         {
            if (menu_displaylist_ctl(DISPLAYLIST_FAVORITES, info))
               return menu_displaylist_process(info);
            return false;
         }
         else
         {
            char path_playlist[PATH_MAX_LENGTH];
            playlist_t *playlist                = NULL;

            path_playlist[0] = '\0';


            fill_pathname_join(
                  path_playlist,
                  settings->paths.directory_playlist,
                  info->path,
                  sizeof(path_playlist));

            menu_displaylist_set_new_playlist(menu, path_playlist);

            strlcpy(path_playlist,
                  msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION),
                  sizeof(path_playlist));

            menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

            playlist_qsort(playlist);

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
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (settings->bools.history_list_enable)
            menu_displaylist_parse_playlist_history(menu, info,
                  "history",
                  settings->paths.path_content_history,
                  &ret);
         else
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            ret = 0;
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_FAVORITES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_playlist_history(menu, info,
               "favorites",
               settings->paths.path_content_favorites,
               &ret);
         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_MUSIC_HISTORY:
         if (settings->bools.history_list_enable)
            menu_displaylist_parse_playlist_history(menu, info,
                  "music_history",
                  settings->paths.path_content_music_history,
                  &ret);
         else
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            ret = 0;
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_VIDEO_HISTORY:
#ifdef HAVE_FFMPEG
         if (settings->bools.history_list_enable)
            menu_displaylist_parse_playlist_history(menu, info,
                  "video_history",
                  settings->paths.path_content_video_history,
                  &ret);
         else
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            ret = 0;
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
#endif
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_INDEX),
               MENU_ENUM_LABEL_DISK_INDEX,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS),
               MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND),
               MENU_ENUM_LABEL_DISK_IMAGE_APPEND,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0);

         info->need_push    = true;
         break;
      case DISPLAYLIST_NETWORK_INFO:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#if defined(HAVE_NETWORKING) && !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU)
         menu_displaylist_parse_network_info(info);
#endif
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_system_info(info);
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_ACHIEVEMENT_LIST:
#ifdef HAVE_CHEEVOS
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         cheevos_populate_menu(info, false);
         info->need_push    = true;
         info->need_refresh = true;
#endif
         break;
      case DISPLAYLIST_ACHIEVEMENT_LIST_HARDCORE:
#ifdef HAVE_CHEEVOS
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         cheevos_populate_menu(info, true);
         info->need_push    = true;
         info->need_refresh = true;
#endif
         break;

      case DISPLAYLIST_CORES_SUPPORTED:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;

         {
            unsigned cores_names_len        = 0;
            unsigned cores_paths_len        = 0;
            size_t cores_paths_size         = 0;
            size_t cores_names_size         = 0;
            struct string_list *cores_names =
               string_list_new_special(STRING_LIST_SUPPORTED_CORES_NAMES,
                     (void*)menu->deferred_path,
                     &cores_names_len, &cores_names_size);

            if (cores_names_size == 0)
            {
               rarch_system_info_t *system_info = runloop_get_system_info();
               struct retro_system_info *system = &system_info->info;
               const char *core_name            = system ? system->library_name : NULL;

               if (!path_is_empty(RARCH_PATH_CORE))
               {

                  menu_entries_append_enum(info->list,
                        path_get(RARCH_PATH_CORE),
                        path_get(RARCH_PATH_CORE),
                        MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE,
                        FILE_TYPE_DIRECT_LOAD,
                        0,
                        0);

                  if (!string_is_empty(core_name))
                     menu_entries_set_alt_at_offset(info->list, 0,
                           core_name);
               }
               else
               {
                  if (system)
                  {
                     menu_entries_append_enum(info->list,
                           core_name,
                           core_name,
                           MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE,
                           FILE_TYPE_DIRECT_LOAD,
                           0,
                           0);
                  }
                  else
                  {
                     menu_entries_append_enum(info->list,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE),
                           msg_hash_to_str(MENU_ENUM_LABEL_NO_CORES_AVAILABLE),
                           MENU_ENUM_LABEL_NO_CORES_AVAILABLE,
                           0, 0, 0);
                     info->download_core = true;
                  }
               }
            }

            if (cores_names_size != 0)
            {
               unsigned j = 0;
               struct string_list *cores_paths =
                  string_list_new_special(STRING_LIST_SUPPORTED_CORES_PATHS,
                        (void*)menu->deferred_path,
                        &cores_paths_len, &cores_paths_size);

               for (i = 0; i < cores_names_size; i++)
               {
                  const char *core_path = cores_paths->elems[i].data;
                  const char *core_name = cores_names->elems[i].data;

                  if (     !path_is_empty(RARCH_PATH_CORE) &&
                           string_is_equal(core_path, path_get(RARCH_PATH_CORE)))
                  {
                     strlcpy(new_path_entry, core_path, sizeof(new_path_entry));
                     snprintf(new_entry, sizeof(new_entry), "Current core (%s)", core_name);
                     strlcpy(new_lbl_entry, core_path, sizeof(new_lbl_entry));
                     new_type = MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE;
                  }
                  else if (core_path)
                  {
                     menu_entries_append_enum(info->list, core_path,
                           msg_hash_to_str(MENU_ENUM_LABEL_DETECT_CORE_LIST_OK),
                           MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
                           FILE_TYPE_CORE, 0, 0);

                     menu_entries_set_alt_at_offset(info->list, j, core_name);
                     j++;
                  }
               }

               string_list_free(cores_paths);
            }

            string_list_free(cores_names);

         }
         break;
      case DISPLAYLIST_CORES_COLLECTION_SUPPORTED:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;

         {
            unsigned cores_names_len        = 0;
            unsigned cores_paths_len        = 0;
            size_t cores_paths_size         = 0;
            size_t cores_names_size         = 0;
            struct string_list *cores_names =
               string_list_new_special(STRING_LIST_SUPPORTED_CORES_NAMES,
                     (void*)menu->deferred_path,
                     &cores_names_len, &cores_names_size);

            if (cores_names_size == 0)
            {
               if (!path_is_empty(RARCH_PATH_CORE))
               {

                  menu_entries_append_enum(info->list,
                        path_get(RARCH_PATH_CORE),
                        path_get(RARCH_PATH_CORE),
                        MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
                        FILE_TYPE_DIRECT_LOAD,
                        0,
                        0);

                  {
                     const char *core_name            = NULL;
                     rarch_system_info_t *system_info = runloop_get_system_info();
                     struct retro_system_info *system = &system_info->info;

                     if (system)
                        core_name = system->library_name;

                     if (!string_is_empty(core_name))
                        menu_entries_set_alt_at_offset(info->list, 0,
                              core_name);
                  }
               }
               else
               {
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_CORES_AVAILABLE),
                        MENU_ENUM_LABEL_NO_CORES_AVAILABLE,
                        0, 0, 0);
                  info->download_core = true;
               }
            }

            if (cores_names_size != 0)
            {
               unsigned j = 0;
               struct string_list *cores_paths =
                  string_list_new_special(STRING_LIST_SUPPORTED_CORES_PATHS,
                        (void*)menu->deferred_path,
                        &cores_paths_len, &cores_paths_size);

               for (i = 0; i < cores_names_size; i++)
               {
                  const char *core_path = cores_paths->elems[i].data;
                  const char *core_name = cores_names->elems[i].data;

                  if (     !path_is_empty(RARCH_PATH_CORE) &&
                           string_is_equal(core_path, path_get(RARCH_PATH_CORE)))
                  {
                     strlcpy(new_path_entry, core_path, sizeof(new_path_entry));
                     snprintf(new_entry, sizeof(new_entry), "Current core (%s)", core_name);
                     new_lbl_entry[0] = '\0';
                     new_type         = MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION_CURRENT_CORE;
                  }
                  else if (core_path)
                  {
                     menu_entries_append_enum(info->list, core_path, "",
                           MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
                           FILE_TYPE_CORE, 0, 0);
                     menu_entries_set_alt_at_offset(info->list, j, core_name);
                     j++;
                  }
               }

               string_list_free(cores_paths);
            }

            string_list_free(cores_names);

         }
         break;
      case DISPLAYLIST_CORE_INFO:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_core_info(info);
         info->need_push = true;
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
         {
            size_t opts = 0;

            rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

            if (settings->bools.game_specific_options)
            {
               if (!rarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE),
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_CREATE),
                        MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_CREATE,
                        MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0);
               else
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE),
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_IN_USE),
                        MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_IN_USE,
                        MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0);
            }
            if (opts == 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE),
                     MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE,
                     MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
            }
            else
            {
               core_option_manager_t *coreopts = NULL;

               rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

               for (i = 0; i < opts; i++)
                  menu_entries_append_enum(info->list,
                        core_option_manager_get_desc(coreopts, i), "",
                        MENU_ENUM_LABEL_CORE_OPTION_ENTRY,
                        (unsigned)(MENU_SETTINGS_CORE_OPTION_START + i), 0, 0);
            }
         }
         else
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE),
                  MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE,
                  MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_ARCHIVE_ACTION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_COMPRESSION
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_OPEN_ARCHIVE),
               MENU_ENUM_LABEL_OPEN_ARCHIVE,
               0, 0, 0);
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_ARCHIVE),
               MENU_ENUM_LABEL_LOAD_ARCHIVE,
               0, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_COMPRESSION
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE),
               MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE,
               0, 0, 0);
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE),
               MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE,
               0, 0, 0);
         info->need_push = true;
         break;
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_HISTORY_LIST_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE,
               PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_playlist_associations(info);
         info->need_push    = true;
         break;
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            unsigned i;

            for (i = 0; i < RARCH_BIND_LIST_END; i++)
            {
               ret = menu_displaylist_parse_settings_enum(menu, info,
                    (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i),
                    PARSE_ONLY_BIND, false);
               (void)ret;
            }
         }
         info->need_push    = true;
         break;
      case DISPLAYLIST_DRIVER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_JOYPAD_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CAMERA_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LOCATION_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
#ifdef HAVE_LAKKA
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_WIFI_DRIVER,
               PARSE_ONLY_STRING_OPTIONS, false);
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CONFIGURATION_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_SAVING_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUTOSAVE_INTERVAL,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_LOGGING_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LOG_VERBOSITY,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL,
               PARSE_ONLY_UINT, false);
         if (settings->bools.menu_show_advanced_settings)
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_PERFCNT_ENABLE,
                  PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_REWIND_SETTINGS,   PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_FASTFORWARD_RATIO,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SLOWMOTION_RATIO,
               PARSE_ONLY_FLOAT, false);
         if (settings->bools.menu_show_advanced_settings)
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE,
                  PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_REWIND_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_REWIND_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_REWIND_GRANULARITY,
               PARSE_ONLY_UINT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,   PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,   PARSE_ACTION, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FONT_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FONT_PATH,
               PARSE_ONLY_PATH, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FONT_SIZE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y,
               PARSE_ONLY_FLOAT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE,
               PARSE_ONLY_BOOL, false);
#if 0
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED,
               PARSE_ONLY_BOOL, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_OVERLAY_PRESET,
               PARSE_ONLY_PATH, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_OVERLAY_OPACITY,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_OVERLAY_SCALE,
               PARSE_ONLY_FLOAT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SHOW_HIDDEN_FILES,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USE_BUILTIN_PLAYER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_FILTER_BY_CURRENT_CORE,
               PARSE_ONLY_BOOL, false);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#if defined(HAVE_NETWORKING) && !defined(HAVE_LAKKA)
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER,
               PARSE_ONLY_BOOL, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_SETTINGS,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
            MENU_ENUM_LABEL_XMB_SHOW_FAVORITES,
            PARSE_ONLY_BOOL, false);

#ifdef HAVE_IMAGEVIEWER
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_IMAGES,
               PARSE_ONLY_BOOL, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_MUSIC,
               PARSE_ONLY_BOOL, false);
#ifdef HAVE_FFMPEG
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_VIDEO,
               PARSE_ONLY_BOOL, false);
#endif
#ifdef HAVE_NETWORKING
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_NETPLAY,
               PARSE_ONLY_BOOL, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_HISTORY,
               PARSE_ONLY_BOOL, false);
#ifdef HAVE_LIBRETRODB
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHOW_ADD,
               PARSE_ONLY_BOOL, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_TIMEDATE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CORE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MENU_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_WALLPAPER,
               PARSE_ONLY_PATH, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_LINEAR_FILTER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR,
               PARSE_ONLY_HEX, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ENTRY_HOVER_COLOR,
               PARSE_ONLY_HEX, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_TITLE_COLOR,
               PARSE_ONLY_HEX, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_ALPHA_FACTOR,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SCALE_FACTOR,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_FONT,
               PARSE_ONLY_PATH, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_THEME,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_THUMBNAILS,
               PARSE_ONLY_UINT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS,   PARSE_ACTION, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SETTINGS,   PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PAUSE_LIBRETRO,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MOUSE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_POINTER_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PAUSE_NONACTIVE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_UI_COMPANION_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_UI_MENUBAR_ENABLE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_USERNAME,
               PARSE_ONLY_STRING, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_PASSWORD,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_UPDATER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            unsigned count = 0;
            if (menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL,
                     PARSE_ONLY_STRING, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL,
                  PARSE_ONLY_STRING, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                     MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                     0, 0, 0);
         }

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_WIFI_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (string_is_equal_fast(settings->arrays.wifi_driver, "null", 4))
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_NETWORKS_FOUND),
                  MENU_ENUM_LABEL_NO_NETWORKS_FOUND,
                  0, 0, 0);
#ifdef HAVE_NETWORKING
         else
         {
            struct string_list *ssid_list = string_list_new();
            driver_wifi_get_ssids(ssid_list);

            if (ssid_list->size == 0)
            {
               task_push_wifi_scan(wifi_scan_callback);

               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_NETWORKS_FOUND),
                     MENU_ENUM_LABEL_NO_NETWORKS_FOUND,
                     0, 0, 0);
            }
            else
            {
               unsigned i;
               for (i = 0; i < ssid_list->size; i++)
               {
                  const char *ssid = ssid_list->elems[i].data;
                  menu_entries_append_enum(info->list,
                        ssid,
                        msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_WIFI),
                        MENU_ENUM_LABEL_CONNECT_WIFI,
                        MENU_WIFI, 0, 0);
               }
            }
         }
#else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_NETWORKS_FOUND),
               MENU_ENUM_LABEL_NO_NETWORKS_FOUND,
               0, 0, 0);
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_NETWORK_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            unsigned user;
            unsigned count = 0;

            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS,
                  PARSE_ONLY_STRING, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT,
                  PARSE_ONLY_UINT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_PASSWORD,
                  PARSE_ONLY_STRING, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD,
                  PARSE_ONLY_STRING, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES,
                  PARSE_ONLY_INT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
                  PARSE_ONLY_INT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
                  PARSE_ONLY_INT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETWORK_CMD_ENABLE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETWORK_CMD_PORT,
                  PARSE_ONLY_UINT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETWORK_REMOTE_PORT,
                  PARSE_ONLY_UINT, false) != -1)
               count++;

            {
               unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
               for(user = 0; user < max_users; user++)
               {
                  if (menu_displaylist_parse_settings_enum(menu, info,
                           (enum msg_hash_enums)(MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user),
                           PARSE_ONLY_BOOL, false) != -1)
                     count++;
               }
            }

            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_STDIN_CMD_ENABLE,
                  PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_UPDATER_SETTINGS,   PARSE_ACTION, false) != -1)
               count++;

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                     MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                     0, 0, 0);
         }

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_LAKKA_SERVICES_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SSH_ENABLE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAMBA_ENABLE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_BLUETOOTH_ENABLE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_USER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_LIST,
               PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NETPLAY_NICKNAME,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USER_LANGUAGE,
               PARSE_ONLY_UINT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DIRECTORY_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SYSTEM_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ASSETS_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LIBRETRO_DIR_PATH,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LIBRETRO_INFO_PATH,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CURSOR_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_DATABASE_PATH,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FILTER_DIR,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_FILTER_DIR,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SHADER_DIR,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_OVERLAY_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PLAYLIST_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVEFILE_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVESTATE_DIRECTORY,
               PARSE_ONLY_DIR, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CACHE_DIRECTORY,
               PARSE_ONLY_DIR, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_PRIVACY_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            bool available = false;
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CAMERA_ALLOW,
                  PARSE_ONLY_BOOL, false) == 0)
               available = true;
            if (menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_LOCATION_ALLOW,
                     PARSE_ONLY_BOOL, true) == 0)
               available = true;

            if (!available)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                     MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                     0, 0, 0);

            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_VIDEO_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_FPS_SHOW,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SCREEN_RESOLUTION,
               PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PAL60_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_GAMMA,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SOFT_FILTER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SCALE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VI_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VFILTER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_ROTATION,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_THREADED,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_VSYNC,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SMOOTH,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FILTER,
               PARSE_ONLY_PATH, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CORE_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_AUDIO_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_MUTE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_MIXER_MUTE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_VOLUME,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_SYNC,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_DEVICE,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,
               PARSE_ONLY_PATH, false);
#ifdef HAVE_WASAPI
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
               PARSE_ONLY_INT, false);
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_MAX_USERS,
               PARSE_ONLY_UINT, false);
#if TARGET_OS_IPHONE
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE,
               PARSE_ONLY_BOOL, false);
#endif
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
               PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_ICADE_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
               PARSE_ONLY_UINT, false);
#ifdef VITA
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH,
               PARSE_ONLY_BOOL, false);
#endif
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND, PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD, PARSE_ONLY_FLOAT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_TURBO_PERIOD, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_DUTY_CYCLE, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_BIND_MODE, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS, PARSE_ACTION, false);

         {
            unsigned user;
            unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
            for (user = 0; user < max_users; user++)
            {
               menu_displaylist_parse_settings_enum(menu, info,
                     (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user),
                     PARSE_ACTION, false);
            }
         }

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DRIVER_SETTINGS,  PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CORE_SETTINGS,    PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONFIGURATION_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAVING_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LOGGING_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS,   PARSE_ACTION, false);
         if (string_is_not_equal_fast(settings->arrays.record_driver, "null", 4))
            ret = menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_RECORDING_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,   PARSE_ACTION, false);
#ifdef HAVE_CHEEVOS
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS,  PARSE_ACTION, false);
#endif
#ifdef HAVE_LAKKA
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_WIFI_SETTINGS,   PARSE_ACTION, false);
#endif
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NETWORK_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_LAKKA_SERVICES,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PLAYLIST_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USER_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DIRECTORY_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_PRIVACY_SETTINGS,   PARSE_ACTION, false);
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_list(info);

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_content_actions(info);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CONTENT_SETTINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_load_content_settings(info);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INFORMATION_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_information_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_ADD_CONTENT_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_add_content_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_CONFIGURATIONS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_configurations_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_scan_directory_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_NETPLAY_ROOM_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_netplay_room_list(info);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_LOAD_CONTENT_LIST:
      case DISPLAYLIST_LOAD_CONTENT_SPECIAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
               msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
               MENU_ENUM_LABEL_GOTO_FAVORITES,
               MENU_SETTING_ACTION, 0, 0);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
               msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
               MENU_ENUM_LABEL_GOTO_IMAGES,
               MENU_SETTING_ACTION, 0, 0);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
               msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
               MENU_ENUM_LABEL_GOTO_MUSIC,
               MENU_SETTING_ACTION, 0, 0);

#ifdef HAVE_FFMPEG
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
               msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
               MENU_ENUM_LABEL_GOTO_VIDEO,
               MENU_SETTING_ACTION, 0, 0);
#endif

         if (!string_is_empty(settings->paths.directory_menu_content))
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

         if (core_info_list_num_info_files(list))
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  msg_hash_to_str(
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                  MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                  MENU_SETTING_ACTION, 0, 0);
         }

#ifdef HAVE_LIBRETRODB
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST),
               MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST,
               MENU_SETTING_ACTION, 0, 0);
#endif

         if (frontend_driver_parse_drive_list(info->list, true) != 0)
            menu_entries_append_enum(info->list, "/",
                  msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                  MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                  MENU_SETTING_ACTION, 0, 0);

#if 0
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL_LIST),
               MENU_ENUM_LABEL_BROWSE_URL_LIST,
               MENU_SETTING_ACTION, 0, 0);
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
               MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
               MENU_SETTING_ACTION, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_OPTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_options(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_options_cheats(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_options_remappings(info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_SHADER_MANAGER
         {
            video_shader_ctx_t shader_info;
            video_shader_driver_get_current_shader(&shader_info);

            if (shader_info.data)
               ret = deferred_push_video_shader_parameters_common(
                     info, shader_info.data,
                     (type == DISPLAYLIST_SHADER_PARAMETERS)
                     ? MENU_SETTINGS_SHADER_PARAMETER_0
                     : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
                     );
            else
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_SHADER_PARAMETERS),
                     MENU_ENUM_LABEL_NO_SHADER_PARAMETERS,
                     0, 0, 0);
               ret = 0;
            }

            info->need_push = true;
         }
#endif
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
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
      case DISPLAYLIST_RECORDING_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_CONFIG,
               PARSE_ONLY_PATH, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_PATH,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_USE_OUTPUT_DIRECTORY,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_GPU_RECORD,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,
               PARSE_ONLY_BOOL, false);

         info->need_push    = true;
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            rarch_system_info_t *system    = runloop_get_system_info();

            if (system)
            {
               if ( !string_is_empty(system->info.library_name) &&
                     !string_is_equal(system->info.library_name,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
                  menu_displaylist_parse_settings_enum(menu, info,
                        MENU_ENUM_LABEL_CONTENT_SETTINGS,
                        PARSE_ACTION, false);

               if (system->load_no_content)
                  menu_displaylist_parse_settings_enum(menu, info,
                        MENU_ENUM_LABEL_START_CORE, PARSE_ACTION, false);
            }


#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_CORE_LIST, PARSE_ACTION, false);
            }

            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                  PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                  PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                  PARSE_ACTION, false);
#ifdef HAVE_NETWORKING
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY,
                  PARSE_ACTION, false);
#endif
#if defined(HAVE_NETWORKING)
            if (settings->bools.menu_show_online_updater)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION, false);
#endif
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_SETTINGS, PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_INFORMATION_LIST,
                  PARSE_ACTION, false);
#ifndef HAVE_DYNAMIC
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_RESTART_RETROARCH,
                  PARSE_ACTION, false);
#endif
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                  PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_HELP_LIST,
                  PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_QUIT_RETROARCH,
                  PARSE_ACTION, false);
#if defined(HAVE_LAKKA)
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_REBOOT,
                  PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_SHUTDOWN,
                  PARSE_ACTION, false);
#endif
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_SETTING_ENUM:
         {
            menu_displaylist_ctx_parse_entry_t *entry  =
               (menu_displaylist_ctx_parse_entry_t*)data;

            if (menu_displaylist_parse_settings_enum(entry->data,
                     entry->info,
                     entry->enum_idx,
                     entry->parse_type,
                     entry->add_empty_entry) == -1)
               return false;
         }
         break;
      case DISPLAYLIST_HELP:
         menu_entries_append_enum(info->list, info->path,
               info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0);
         menu_dialog_unset_pending_push();
         break;
      case DISPLAYLIST_HELP_SCREEN_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS),
               MENU_ENUM_LABEL_HELP_CONTROLS,
               0, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE),
               MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE,
               0, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOADING_CONTENT),
               MENU_ENUM_LABEL_HELP_LOADING_CONTENT,
               0, 0, 0);
#ifdef HAVE_LIBRETRODB
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_SCANNING_CONTENT),
               MENU_ENUM_LABEL_HELP_SCANNING_CONTENT,
               0, 0, 0);
#endif
#ifdef HAVE_OVERLAY
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD),
               MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD,
               0, 0, 0);
#endif
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING),
               MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
               0, 0, 0);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_BROWSE_URL_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL),
               MENU_ENUM_LABEL_BROWSE_URL,
               0, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_START),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_START),
               MENU_ENUM_LABEL_BROWSE_START,
               0, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_BROWSE_URL_START:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         {
            char link[1024];
            char name[1024];
            const char *line  = "<a href=\"http://www.test.com/somefile.zip\">Test</a>\n";

            link[0] = name[0] = '\0';

            string_parse_html_anchor(line, link, name, sizeof(link), sizeof(name));

            menu_entries_append_enum(info->list,
                  link,
                  name,
                  MSG_UNKNOWN,
                  0, 0, 0);
         }
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_CHEEVOS
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
               PARSE_ACTION, false);
#else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
               MENU_ENUM_LABEL_NO_ITEMS,
               MENU_SETTING_NO_ITEM, 0, 0);
         ret = 0;
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_CHEEVOS
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_USERNAME,
               PARSE_ONLY_STRING, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_PASSWORD,
               PARSE_ONLY_STRING, false);
#else
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
               MENU_ENUM_LABEL_NO_ITEMS,
               MENU_SETTING_NO_ITEM, 0, 0);
         ret = 0;
#endif
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INFO:
         menu_entries_append_enum(info->list, info->path,
               info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0);
         break;
      case DISPLAYLIST_FILE_BROWSER_SCAN_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_FILE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_CORE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION:
      case DISPLAYLIST_GENERIC:
         {
            menu_ctx_list_t list_info;

            list_info.type    = MENU_LIST_PLAIN;
            list_info.action  = 0;

            menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

            menu_entries_append_enum(info->list, info->path,
                  info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0);

            info->need_navigation_clear = true;
            info->need_entries_refresh  = true;
         }
         break;
      case DISPLAYLIST_PENDING_CLEAR:
         {
            menu_ctx_list_t list_info;

            list_info.type    = MENU_LIST_PLAIN;
            list_info.action  = 0;

            menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

            menu_entries_append_enum(info->list, info->path,
                  info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0);
            info->need_entries_refresh = true;
         }
         break;
      case DISPLAYLIST_USER_BINDS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            char lbl[PATH_MAX_LENGTH];
            unsigned val              = atoi(info->path);
            const char *temp_val      = msg_hash_to_str(
                  (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + (val-1)));

            lbl[0]                    = '\0';

            strlcpy(lbl, temp_val, sizeof(lbl));
            ret = menu_displaylist_parse_settings(menu, info,
                  lbl, PARSE_NONE, true);
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_RDB;
         strlcpy(info->exts,
               file_path_str(FILE_PATH_RDB_EXTENSION),
               sizeof(info->exts));
         info->enum_idx     = MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->path, settings->paths.path_content_database, sizeof(info->path));
         break;
      case DISPLAYLIST_DATABASE_CURSORS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_CURSOR;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "dbc", sizeof(info->exts));
         strlcpy(info->path, settings->paths.directory_cursor, sizeof(info->path));
         break;
      case DISPLAYLIST_CONFIG_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_CONFIG;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         load_content       = false;
         use_filebrowser    = true;
         break;
      case DISPLAYLIST_SHADER_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            struct string_list *str_list;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_VULKAN)
            union string_list_elem_attr attr;
            attr.i = 0;
#endif
            str_list = string_list_new();

            filebrowser_clear_type();
            info->type_default = FILE_TYPE_SHADER_PRESET;

#ifdef HAVE_CG
            string_list_append(str_list, "cgp", attr);
#endif
#ifdef HAVE_GLSL
            string_list_append(str_list, "glslp", attr);
#endif
#ifdef HAVE_VULKAN
            string_list_append(str_list, "slangp", attr);
#endif
            string_list_join_concat(info->exts, sizeof(info->exts), str_list, "|");
            string_list_free(str_list);
            use_filebrowser    = true;
         }
         break;
      case DISPLAYLIST_SHADER_PASS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            struct string_list *str_list;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_VULKAN)
            union string_list_elem_attr attr;
            attr.i = 0;
#endif

            str_list = string_list_new();

            filebrowser_clear_type();
            info->type_default = FILE_TYPE_SHADER;


#ifdef HAVE_CG
            string_list_append(str_list, "cg", attr);
#endif
#ifdef HAVE_GLSL
            string_list_append(str_list, "glsl", attr);
#endif
#ifdef HAVE_VULKAN
            string_list_append(str_list, "slang", attr);
#endif
            string_list_join_concat(info->exts, sizeof(info->exts), str_list, "|");
            string_list_free(str_list);
            use_filebrowser    = true;
         }
         break;
      case DISPLAYLIST_VIDEO_FILTERS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_VIDEOFILTER;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "filt", sizeof(info->exts));
         break;
      case DISPLAYLIST_IMAGES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (     (filebrowser_get_type() != FILEBROWSER_SELECT_FILE)
               && (filebrowser_get_type() != FILEBROWSER_SELECT_IMAGE))
            filebrowser_clear_type();
         info->type_default = FILE_TYPE_IMAGE;
         {
            union string_list_elem_attr attr;
            struct string_list *str_list     = string_list_new();

            attr.i = 0;

#ifdef HAVE_RBMP
            string_list_append(str_list, "bmp", attr);
#endif
#ifdef HAVE_RPNG
            string_list_append(str_list, "png", attr);
#endif
#ifdef HAVE_RJPEG
            string_list_append(str_list, "jpeg", attr);
            string_list_append(str_list, "jpg", attr);
#endif
#ifdef HAVE_RTGA
            string_list_append(str_list, "tga", attr);
#endif
            string_list_join_concat(info->exts, sizeof(info->exts), str_list, "|");
            string_list_free(str_list);
         }
         use_filebrowser    = true;
         break;
      case DISPLAYLIST_IMAGES_HISTORY:
#ifdef HAVE_IMAGEVIEWER
         if (settings->bools.history_list_enable)
            menu_displaylist_parse_playlist_history(menu, info,
                  "images_history",
                  settings->paths.path_content_image_history,
                  &ret);
         else
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            ret = 0;
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
#endif
         break;
      case DISPLAYLIST_AUDIO_FILTERS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_AUDIOFILTER;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "dsp", sizeof(info->exts));
         break;
      case DISPLAYLIST_CHEAT_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_CHEAT;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "cht", sizeof(info->exts));
         break;
      case DISPLAYLIST_CONTENT_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_PLAIN;
         use_filebrowser    = true;
         strlcpy(info->exts, "lpl", sizeof(info->exts));
         break;
      case DISPLAYLIST_FONTS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_FONT;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "ttf", sizeof(info->exts));
         break;
      case DISPLAYLIST_OVERLAYS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_OVERLAY;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         break;
      case DISPLAYLIST_RECORD_CONFIG_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_RECORD_CONFIG;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "cfg", sizeof(info->exts));
         break;
      case DISPLAYLIST_REMAP_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_REMAP;
         load_content       = false;
         use_filebrowser    = true;
         strlcpy(info->exts, "rmp", sizeof(info->exts));
         break;
      case DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_playlists(info, true) == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASE_PLAYLISTS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_playlists(info, false) == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_CORES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            char ext_name[PATH_MAX_LENGTH];

            ext_name[0] = '\0';

            filebrowser_clear_type();
            info->type_default = FILE_TYPE_PLAIN;
            if (frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
               strlcpy(info->exts, ext_name, sizeof(info->exts));
         }
         if (menu_displaylist_parse_cores(menu, info) == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         info->push_builtin_cores = true;
         break;
      case DISPLAYLIST_DEFAULT:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         use_filebrowser = true;
         break;
      case DISPLAYLIST_CORES_DETECTED:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         use_filebrowser = true;
         break;
      case DISPLAYLIST_NONE:
         break;
   }

   if (use_filebrowser)
   {
      if (string_is_empty(info->path))
      {
         if (frontend_driver_parse_drive_list(info->list, load_content) != 0)
            menu_entries_append_enum(info->list, "/", "",
                  MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
         info->need_refresh = true;
         info->need_push    = true;
      }
      else
      {
         filebrowser_parse(info, type);
         info->need_refresh = true;
         info->need_push    = true;
      }
   }

   if (ret != 0)
      return false;

   return true;
}
