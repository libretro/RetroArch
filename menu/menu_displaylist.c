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
#include <streams/file_stream.h>
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

#ifdef HAVE_LAKKA_SWITCH
#include "../../lakka.h"
#endif

#if defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../frontend/drivers/platform_unix.h"
#endif

#include "menu_cbs.h"
#include "menu_content.h"
#include "menu_driver.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_networking.h"
#include "widgets/menu_dialog.h"
#include "widgets/menu_filebrowser.h"

#include "../audio/audio_driver.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../defaults.h"
#include "../verbosity.h"
#include "../managers/cheat_manager.h"
#include "../managers/core_option_manager.h"
#include "../paths.h"
#include "../record/record_driver.h"
#include "../retroarch.h"
#include "../core.h"
#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_driver.h"
#include "../config.features.h"
#include "../version_git.h"
#include "../input/input_driver.h"
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

#if !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU) && !defined(SWITCH)
#include <net/net_ifinfo.h>

static int menu_displaylist_parse_network_info(menu_displaylist_info_t *info)
{
   net_ifinfo_t      list;
   unsigned count          = 0;
   unsigned k              = 0;

   if (!net_ifinfo_new(&list))
      return 0;

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
      count++;
   }

   net_ifinfo_free(&list);

   return count;
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
   settings_t *settings      = config_get_ptr();

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

   if (core_info->core_name)
   {
      fill_pathname_join_concat_noext(tmp,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME),
            ": ",
            core_info->core_name,
            sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }


   if (core_info->display_name)
   {
      fill_pathname_join_concat_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL),
            ": ",
            core_info->display_name,
            sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->systemname)
   {
      fill_pathname_join_concat_noext(tmp,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME),
            ": ",
            core_info->systemname,
            sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
   }

   if (core_info->system_manufacturer)
   {
      fill_pathname_join_concat_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER),
            ": ",
            core_info->system_manufacturer,
            sizeof(tmp));
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
      bool update_missing_firmware   = false;
      bool set_missing_firmware      = false;

      firmware_info.path             = core_info->path;
      firmware_info.directory.system = settings->paths.directory_system;

      rarch_ctl(RARCH_CTL_UNSET_MISSING_BIOS, NULL);

      update_missing_firmware        = core_info_list_update_missing_firmware(&firmware_info, &set_missing_firmware);

      if (set_missing_firmware)
         rarch_ctl(RARCH_CTL_SET_MISSING_BIOS, NULL);

      if (update_missing_firmware)
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

  if (settings->bools.menu_show_core_updater)
     menu_entries_append_enum(info->list,
           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE),
           msg_hash_to_str(MENU_ENUM_LABEL_CORE_DELETE),
           MENU_ENUM_LABEL_CORE_DELETE,
           MENU_SETTING_ACTION_CORE_DELETE, 0, 0);

   return 0;
}

#define BYTES_TO_MB(bytes) ((bytes) / 1024 / 1024)
#define BYTES_TO_GB(bytes) (((bytes) / 1024) / 1024 / 1024)

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
   fill_pathname_join_concat_noext(
         tmp,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION),
         ": ",
         retroarch_git_version,
         sizeof(tmp));
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
            strlcpy(cpu_arch_str, "x64", sizeof(cpu_arch_str));
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
            snprintf(tmp, sizeof(tmp), "Port #%d device display name: %s",
                 controller,
                 input_config_get_device_display_name(controller) ?
                    input_config_get_device_display_name(controller) : "N/A");
            menu_entries_append_enum(info->list, tmp, "",
                 MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                 MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
            snprintf(tmp, sizeof(tmp), "Port #%d device config name: %s",
                 controller,
                 input_config_get_device_display_name(controller) ?
                    input_config_get_device_config_name(controller) : "N/A");
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

      fill_pathname_join_concat_noext(
            tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER),
            ": ",
            frontend->ident,
            sizeof(tmp));
      menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

#ifdef HAVE_LAKKA
      if (frontend->get_lakka_version)
      {
         frontend->get_lakka_version(tmp2, sizeof(tmp2));

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION),
               ": ",
               tmp2,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
#endif

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME),
               ": ",
               tmp2,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s : %s (v%d.%d)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS),
               tmp2,
               major, minor);
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      if (frontend->get_rating)
      {
         snprintf(tmp, sizeof(tmp), "%s : %d",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
               frontend->get_rating());
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }

      {
         char tmp[PATH_MAX_LENGTH];
         char tmp2[PATH_MAX_LENGTH];
         char tmp3[PATH_MAX_LENGTH];
         uint64_t memory_used       = frontend_driver_get_used_memory();
         uint64_t memory_total      = frontend_driver_get_total_memory();

         tmp[0] = tmp2[0] = tmp3[0] = '\0';

         if (memory_used != 0 && memory_total != 0)
         {
            snprintf(tmp, sizeof(tmp),
                  "%s %s: %" PRIu64 "/%" PRIu64 " B",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_BYTES),
                  memory_used,
                  memory_total
                  );
            snprintf(tmp2, sizeof(tmp2),
                  "%s %s: %" PRIu64 "/%" PRIu64 " MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  BYTES_TO_MB(memory_used),
                  BYTES_TO_MB(memory_total)
                  );
            snprintf(tmp3, sizeof(tmp3),
                  "%s %s: %" PRIu64 "/%" PRIu64 " GB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_GIGABYTES),
                  BYTES_TO_GB(memory_used),
                  BYTES_TO_GB(memory_total)
                  );
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

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE),
               ": ",
               tmp2,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   video_context_driver_get_ident(&ident_info);
   tmp_string = ident_info.ident;

   fill_pathname_join_concat_noext(tmp,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER),
         ": ",
         tmp_string ? tmp_string
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

   fill_pathname_join_concat_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT),
         ": ",
         _libretrodb_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   fill_pathname_join_concat_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT),
         ": ",
         _overlay_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
         sizeof(feat_str));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   fill_pathname_join_concat_noext(feat_str,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT),
         ": ",
         _command_supp
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
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT),
         _metal_supp ?
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
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT),
         _ffmpeg_supp ?
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO));
   menu_entries_append_enum(info->list, feat_str, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0);

   snprintf(feat_str, sizeof(feat_str),
         "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT),
         _mpv_supp ?
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
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT),
         _stbfont_supp ?
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
   size_t selection = menu_navigation_get_selection();
   size_t list_size = playlist_size(playlist);

   if (list_size == 0)
      goto error;

   if (!string_is_empty(info->path))
   {
      size_t lpl_basename_size = PATH_MAX_LENGTH * sizeof(char);
      char *lpl_basename       = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      lpl_basename[0] = '\0';

      fill_pathname_base_noext(lpl_basename, info->path, lpl_basename_size);
      menu_driver_set_thumbnail_system(lpl_basename, lpl_basename_size);
      free(lpl_basename);
   }

   /* preallocate the file list */
   file_list_reserve(info->list, list_size);

   for (i = 0; i < list_size; i++)
   {
      char *path_copy                 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      char *fill_buf                  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      size_t path_size                = PATH_MAX_LENGTH * sizeof(char);
      const char *core_name           = NULL;
      const char *path                = NULL;
      const char *label               = NULL;

      fill_buf[0] = path_copy[0]      = '\0';

      if (!string_is_empty(info->path))
         strlcpy(path_copy, info->path, path_size);

      path                            = path_copy;

      playlist_get_index(playlist, i,
            &path, &label, NULL, &core_name, NULL, NULL);

      if (core_name)
         strlcpy(fill_buf, core_name, path_size);

      if (!is_history && i == selection && !string_is_empty(label))
      {
         char *content_basename = strdup(label);

         if (!string_is_empty(content_basename))
         {
            menu_driver_set_thumbnail_content(content_basename, strlen(content_basename) + 1);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH, NULL);
            menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE, NULL);
            free(content_basename);
         }
      }

      if (path)
      {
         char *path_short = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

         path_short[0] = '\0';

         fill_short_pathname_representation(path_short, path,
               path_size);
         strlcpy(fill_buf,
               (!string_is_empty(label)) ? label : path_short,
               path_size);

         if (!string_is_empty(core_name))
         {
            if (!string_is_equal(core_name,
                     file_path_str(FILE_PATH_DETECT)))
            {
               char *tmp       = (char*)
                  malloc(PATH_MAX_LENGTH * sizeof(char));

               tmp[0] = '\0';

               snprintf(tmp, path_size, " (%s)", core_name);
               strlcat(fill_buf, tmp, path_size);

               free(tmp);
            }
         }

         free(path_short);
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

      free(path_copy);
      free(fill_buf);
   }

   return 0;

error:
   info->need_push_no_playlist_entries = true;
   return 0;
}

static int menu_displaylist_parse_shader_options(menu_displaylist_info_t *info)
{
   unsigned i;
   struct video_shader *shader = menu_shader_get();
   unsigned pass_count         = shader ? shader->passes : 0;

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES),
         msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES),
         MENU_ENUM_LABEL_SHADER_APPLY_CHANGES,
         MENU_SETTING_ACTION, 0, 0);
   if (frontend_driver_can_watch_for_changes())
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES),
            msg_hash_to_str(MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES),
            MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES,
            0, 0, 0);
   }
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
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME),
         MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS),
         msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS),
         MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS,
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
   union string_list_elem_attr attr;
   char *tmp                        = NULL;
   char *output_label               = NULL;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();

   if (!str_list)
      return -1;

   attr.i                           = 0;
   tmp                              = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   tmp[0]                           = '\0';

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
      free(tmp);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   fill_pathname_join_concat_noext(tmp, desc, ": ",
         actual_string,
         PATH_MAX_LENGTH * sizeof(char));
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;
   free(tmp);

   return 0;
}

static int create_string_list_rdb_entry_int(
      enum msg_hash_enums enum_idx,
      const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   union string_list_elem_attr attr;
   size_t path_size                 = PATH_MAX_LENGTH * sizeof(char);
   char *tmp                        = NULL;
   char *str                        = NULL;
   char *output_label               = NULL;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();

   if (!str_list)
      goto error;

   attr.i                           = 0;
   tmp                              = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   str                              = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   tmp[0] = str[0] = '\0';

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   snprintf(str, path_size, "%d", actual_int);
   str_len += strlen(str) + 1;
   string_list_append(str_list, str, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
      goto error;

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, path_size, "%s : %d", desc, actual_int);
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   if (output_label)
      free(output_label);

   string_list_free(str_list);
   str_list = NULL;
   free(tmp);
   free(str);
   return 0;

error:
   if (str_list)
      string_list_free(str_list);
   str_list = NULL;
   free(tmp);
   free(str);
   return -1;
}

static enum msg_file_type extension_to_file_hash_type(const char *ext)
{
   if (string_is_equal(ext, "sha1"))
      return FILE_TYPE_SHA1;
   else if (string_is_equal(ext, "crc"))
      return FILE_TYPE_CRC;
   else if (string_is_equal(ext, "md5"))
      return FILE_TYPE_MD5;
   return FILE_TYPE_NONE;
}

static int menu_displaylist_parse_database_entry(menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   unsigned i, j, k;
   char path_playlist[PATH_MAX_LENGTH];
   char path_base[PATH_MAX_LENGTH];
   char query[PATH_MAX_LENGTH];
   playlist_t *playlist                = NULL;
   database_info_list_t *db_info       = NULL;
   settings_t *settings                = config_get_ptr();
   bool show_advanced_settings         = settings->bools.menu_show_advanced_settings;

   path_playlist[0] = path_base[0] = query[0] = '\0';

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
            const char *crc32                = NULL;
            bool match_found                 = false;
            struct string_list *tmp_str_list = NULL;

            playlist_get_index(playlist, j,
                  NULL, NULL, NULL, NULL,
                  NULL, &crc32);

            if (crc32)
                tmp_str_list = string_split(crc32, "|");

            if (!tmp_str_list)
               continue;

            if (tmp_str_list->size > 0)
            {
               if (tmp_str_list->size > 1)
               {
                  const char *elem0 = tmp_str_list->elems[0].data;
                  const char *elem1 = tmp_str_list->elems[1].data;

                  switch (extension_to_file_hash_type(elem1))
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
               }
            }

            string_list_free(tmp_str_list);

            if (!match_found)
               continue;

            menu->scratchpad.unsigned_var = j;
         }
      }

      if (db_info_entry->name)
      {
         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME),
               ": ",
               db_info_entry->name,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_NAME),
               MENU_ENUM_LABEL_RDB_ENTRY_NAME,
               0, 0, 0);
      }
      if (db_info_entry->description)
      {
         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION),
               ": ",
               db_info_entry->description,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION),
               MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION,
               0, 0, 0);
      }
      if (db_info_entry->genre)
      {
         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE),
               ": ",
               db_info_entry->genre,
               sizeof(tmp));
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
      info->need_push_no_playlist_entries = true;

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


static unsigned deferred_push_video_shader_parameters_common(
      menu_displaylist_info_t *info,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i, count = 0;
   size_t list_size  = shader->num_parameters;

   if (list_size == 0)
      return 0;

   for (i = 0; i < list_size; i++)
   {
      menu_entries_append_enum(info->list, shader->parameters[i].desc,
            info->label, MENU_ENUM_LABEL_SHADER_PARAMETERS_ENTRY,
            base_parameter + i, 0, 0);
      count++;
   }

   return count;
}

static int menu_displaylist_parse_settings_internal(void *data,
      menu_displaylist_info_t *info,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry,
      rarch_setting_t *setting
      )
{
   enum setting_type precond;
   size_t             count    = 0;
   uint64_t flags              = 0;
   settings_t *settings        = config_get_ptr();
   bool show_advanced_settings = settings->bools.menu_show_advanced_settings;

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
      case PARSE_ONLY_SIZE:
         precond = ST_SIZE;
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
         case PARSE_ONLY_SIZE:
            if (type == ST_SIZE)
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

      if ((flags & SD_FLAG_ADVANCED) &&
            !show_advanced_settings)
         goto loop;

#ifdef HAVE_LAKKA
      if ((flags & SD_FLAG_LAKKA_ADVANCED) &&
            !show_advanced_settings)
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
         case PARSE_ONLY_SIZE:
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
      case PARSE_ONLY_SIZE:
         precond = ST_SIZE;
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
         case PARSE_ONLY_SIZE:
            if (type == ST_SIZE)
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
         case PARSE_ONLY_SIZE:
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

#define menu_displaylist_parse_settings(data, info, info_label, parse_type, add_empty_entry) menu_displaylist_parse_settings_internal(data, info, parse_type, add_empty_entry, menu_setting_find(info_label))

#define menu_displaylist_parse_settings_enum(data, info, label, parse_type, add_empty_entry) menu_displaylist_parse_settings_internal_enum(data, info, parse_type, add_empty_entry, menu_setting_find_enum(label), label)

static void menu_displaylist_set_new_playlist(
      menu_handle_t *menu, const char *path)
{
   menu->db_playlist_file[0] = '\0';

   if (playlist_get_cached())
      playlist_free_cached();

   if (playlist_init_cached(path, COLLECTION_SIZE))
      strlcpy(
            menu->db_playlist_file,
            path,
            sizeof(menu->db_playlist_file));
}


static int menu_displaylist_parse_horizontal_list(
      menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   menu_ctx_list_t list_info;
   menu_ctx_list_t list_horiz_info;
   bool is_historylist                 = false;
   playlist_t *playlist                = NULL;
   struct item_file *item              = NULL;

   menu_driver_list_get_selection(&list_info);

   list_info.type       = MENU_LIST_TABS;

   menu_driver_list_get_size(&list_info);

   list_horiz_info.type = MENU_LIST_HORIZONTAL;
   list_horiz_info.idx  = list_info.selection - (list_info.size +1);

   menu_driver_list_get_entry(&list_horiz_info);

   item = (struct item_file*)list_horiz_info.entry;

   if (!item)
      return -1;

   if (!string_is_empty(item->path))
   {
      char path_playlist[PATH_MAX_LENGTH];
      char lpl_basename[PATH_MAX_LENGTH];
      settings_t      *settings           = config_get_ptr();

      lpl_basename[0]   = '\0';
      path_playlist[0]  = '\0';

      fill_pathname_base_noext(lpl_basename, item->path, sizeof(lpl_basename));
      menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));
      if (string_is_equal(lpl_basename, "content_history"))
         is_historylist = true;

      fill_pathname_join(
            path_playlist,
            settings->paths.directory_playlist,
            item->path,
            sizeof(path_playlist));
      menu_displaylist_set_new_playlist(menu, path_playlist);
   }

   playlist = playlist_get_cached();

   if (playlist)
   {
      playlist_qsort(playlist);
      menu_displaylist_parse_playlist(info,
            playlist,
            msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION), is_historylist);
   }

   return 0;
}

static int menu_displaylist_parse_load_content_settings(
      menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   settings_t *settings   = config_get_ptr();

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

      if (settings->bools.quick_menu_show_take_screenshot)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
               MENU_ENUM_LABEL_TAKE_SCREENSHOT,
               MENU_SETTING_ACTION_SCREENSHOT, 0, 0);
      }


      if (settings->bools.quick_menu_show_save_load_state
#ifdef HAVE_CHEEVOS
          && !cheevos_hardcore_active
#endif
         )
      {
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
      }

      if (settings->bools.quick_menu_show_save_load_state &&
          settings->bools.quick_menu_show_undo_save_load_state
#ifdef HAVE_CHEEVOS
          && !cheevos_hardcore_active
#endif
         )
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE),
               MENU_ENUM_LABEL_UNDO_LOAD_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE),
               MENU_ENUM_LABEL_UNDO_SAVE_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0);
      }


      if (settings->bools.quick_menu_show_add_to_favorites)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES),
               MENU_ENUM_LABEL_ADD_TO_FAVORITES, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (string_is_not_equal(settings->arrays.record_driver, "null"))
      {
         if (!recording_is_enabled())
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING),
                  msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING),
                  MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING, MENU_SETTING_ACTION, 0, 0);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING),
                  msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING),
                  MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING, MENU_SETTING_ACTION, 0, 0);
         }
         else
         {
            if (streaming_is_enabled())
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING),
                     MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING, MENU_SETTING_ACTION, 0, 0);
            else
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING),
                     MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING, MENU_SETTING_ACTION, 0, 0);
         }
      }

      if (settings->bools.quick_menu_show_options && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS),
               MENU_ENUM_LABEL_CORE_OPTIONS,
               MENU_SETTING_ACTION, 0, 0);
      }

      if (settings->bools.menu_show_overlays && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS),
               MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,
               MENU_SETTING_ACTION, 0, 0);
      }

      if (settings->bools.menu_show_rewind && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS),
               MENU_ENUM_LABEL_REWIND_SETTINGS,
               MENU_SETTING_ACTION, 0, 0);
      }

      if (settings->bools.menu_show_latency && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_LATENCY_SETTINGS),
               MENU_ENUM_LABEL_LATENCY_SETTINGS,
               MENU_SETTING_ACTION, 0, 0);
      }

#if 0
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_SETTINGS),
            MENU_ENUM_LABEL_NETPLAY_SETTINGS,
            MENU_SETTING_ACTION, 0, 0);
#endif

      if (settings->bools.quick_menu_show_controls && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS),
               MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS,
               MENU_SETTING_ACTION, 0, 0);
      }

      if (settings->bools.quick_menu_show_cheats)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS),
               MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS,
               MENU_SETTING_ACTION, 0, 0);
      }

      if ((!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            && system->disk_control_cb.get_num_images)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS),
               MENU_ENUM_LABEL_DISK_OPTIONS,
               MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0);

      if (video_shader_any_supported())
      {
         if (settings->bools.quick_menu_show_shaders && !settings->bools.kiosk_mode_enable)
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS),
                  msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS),
                  MENU_ENUM_LABEL_SHADER_OPTIONS,
                  MENU_SETTING_ACTION, 0, 0);
         }
      }

      if ((settings->bools.quick_menu_show_save_core_overrides ||
         settings->bools.quick_menu_show_save_game_overrides) &&
         !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS),
            MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS,
            MENU_SETTING_ACTION, 0, 0);
      }


#ifdef HAVE_CHEEVOS
      if(settings->bools.cheevos_enable)
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST),
            MENU_ENUM_LABEL_ACHIEVEMENT_LIST,
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
      menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   bool content_loaded             = false;
   const char *label               = NULL;
   const char *entry_path          = NULL;
   const char *core_path           = NULL;
   const char *core_name           = NULL;
   const char *db_name             = NULL;
   playlist_t *playlist            = playlist_get_cached();
   settings_t *settings            = config_get_ptr();
   const char *fullpath            = path_get(RARCH_PATH_CONTENT);
   unsigned idx                    = menu->rpl_entry_selection_ptr;

   if (playlist)
      playlist_get_index(playlist, idx,
            &entry_path, &label, &core_path, &core_name, NULL, &db_name);

   content_loaded = !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, fullpath);

   if (content_loaded)
      menu_displaylist_parse_load_content_settings(menu, info);
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

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY),
               MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY,
               FILE_TYPE_PLAYLIST_ENTRY, 0, idx);
      }

      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN),
            msg_hash_to_str(MENU_ENUM_LABEL_RUN),
            MENU_ENUM_LABEL_RUN, FILE_TYPE_PLAYLIST_ENTRY, 0, idx);

      if (settings->bools.playlist_entry_rename &&
            !settings->bools.kiosk_mode_enable)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RENAME_ENTRY),
               msg_hash_to_str(MENU_ENUM_LABEL_RENAME_ENTRY),
               MENU_ENUM_LABEL_RENAME_ENTRY,
               FILE_TYPE_PLAYLIST_ENTRY, 0, idx);

      if (settings->bools.playlist_entry_remove &&
            !settings->bools.kiosk_mode_enable)
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY),
            msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY),
            MENU_ENUM_LABEL_DELETE_ENTRY,
            MENU_SETTING_ACTION_DELETE_ENTRY, 0, 0);

      if (settings->bools.quick_menu_show_add_to_favorites)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST),
               MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (settings->bools.quick_menu_show_add_to_favorites)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION),
               msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION),
               MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

   }

   if (!string_is_empty(db_name) && (!content_loaded ||
      (content_loaded && settings->bools.quick_menu_show_information)))
   {
      char *db_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      db_path[0]    = '\0';

      fill_pathname_join_noext(db_path,
            settings->paths.path_content_database,
            db_name,
            PATH_MAX_LENGTH * sizeof(char));
      strlcat(db_path,
            file_path_str(FILE_PATH_RDB_EXTENSION),
            PATH_MAX_LENGTH * sizeof(char));

      if (filestream_exists(db_path))
         menu_entries_append_enum(
               info->list,
               label,
               db_path,
               MENU_ENUM_LABEL_INFORMATION,
               FILE_TYPE_RDB_ENTRY, 0, idx);

      free(db_path);
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
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG),
         msg_hash_to_str(MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG),
         MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG,
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

static unsigned menu_displaylist_parse_add_content_list(
      menu_displaylist_info_t *info)
{
   unsigned count = 0;

#ifdef HAVE_LIBRETRODB
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
         MENU_ENUM_LABEL_SCAN_DIRECTORY,
         MENU_SETTING_ACTION, 0, 0);
   count++;
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
         MENU_ENUM_LABEL_SCAN_FILE,
         MENU_SETTING_ACTION, 0, 0);
   count++;
#endif

   return count;
}

static unsigned menu_displaylist_parse_scan_directory_list(
      menu_displaylist_info_t *info)
{
   unsigned count = 0;
#ifdef HAVE_LIBRETRODB
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
         MENU_ENUM_LABEL_SCAN_DIRECTORY,
         MENU_SETTING_ACTION, 0, 0);
   count++;
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
         msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
         MENU_ENUM_LABEL_SCAN_FILE,
         MENU_SETTING_ACTION, 0, 0);
   count++;
#endif

   return count;
}

static unsigned menu_displaylist_parse_options(
      menu_displaylist_info_t *info)
{
   unsigned count = 0;
#ifdef HAVE_NETWORKING
   settings_t *settings         = config_get_ptr();
#endif

#ifdef HAVE_LAKKA
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_LAKKA),
         MENU_ENUM_LABEL_UPDATE_LAKKA,
         MENU_SETTING_ACTION, 0, 0);
   count++;
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
         msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
         MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
         MENU_SETTING_ACTION, 0, 0);
   count++;
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
         MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
         MENU_SETTING_ACTION, 0, 0);
   count++;
#elif defined(HAVE_NETWORKING)
   if (settings->bools.menu_show_core_updater)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
            MENU_ENUM_LABEL_CORE_UPDATER_LIST,
            MENU_SETTING_ACTION, 0, 0);
      count++;
   }

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
         msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
         MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
         MENU_SETTING_ACTION, 0, 0);
   count++;

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
         msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
         MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
         MENU_SETTING_ACTION, 0, 0);
   count++;

   if (settings->bools.menu_show_core_updater)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES),
            msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES),
            MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES,
            MENU_SETTING_ACTION, 0, 0);
      count++;
   }

#ifdef HAVE_UPDATE_ASSETS
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_ASSETS),
         MENU_ENUM_LABEL_UPDATE_ASSETS,
         MENU_SETTING_ACTION, 0, 0);
   count++;
#endif

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES),
         MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES,
         MENU_SETTING_ACTION, 0, 0);
   count++;

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CHEATS),
         MENU_ENUM_LABEL_UPDATE_CHEATS,
         MENU_SETTING_ACTION, 0, 0);
   count++;

#ifdef HAVE_LIBRETRODB
#if !defined(VITA)
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_DATABASES),
         MENU_ENUM_LABEL_UPDATE_DATABASES,
         MENU_SETTING_ACTION, 0, 0);
   count++;
#endif
#endif

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS),
         msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_OVERLAYS),
         MENU_ENUM_LABEL_UPDATE_OVERLAYS,
         MENU_SETTING_ACTION, 0, 0);
   count++;

   if (video_shader_is_supported(RARCH_SHADER_CG))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS),
            msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CG_SHADERS),
            MENU_ENUM_LABEL_UPDATE_CG_SHADERS,
            MENU_SETTING_ACTION, 0, 0);
      count++;
   }

   if (video_shader_is_supported(RARCH_SHADER_GLSL))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS),
            msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS),
            MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS,
            MENU_SETTING_ACTION, 0, 0);
      count++;
   }

   if (video_shader_is_supported(RARCH_SHADER_SLANG))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS),
            msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS),
            MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS,
            MENU_SETTING_ACTION, 0, 0);
      count++;
   }
#endif

   return count;
}

static int menu_displaylist_parse_options_cheats(
      menu_displaylist_info_t *info, menu_handle_t *menu)
{
   unsigned i;

   if (!cheat_manager_alloc_if_empty())
      return -1;

   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_START_OR_CONT),
         MENU_ENUM_LABEL_CHEAT_START_OR_CONT,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD),
         MENU_ENUM_LABEL_CHEAT_FILE_LOAD,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND),
         MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS),
         MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS),
         MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP),
         MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM),
         MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM,
         MENU_SETTING_ACTION, 0, 0);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE_ALL),
         MENU_ENUM_LABEL_CHEAT_DELETE_ALL,
         MENU_SETTING_ACTION, 0, 0);
   menu_displaylist_parse_settings_enum(menu, info,
         MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD,
         PARSE_ONLY_BOOL, false);
   menu_displaylist_parse_settings_enum(menu, info,
         MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE,
         PARSE_ONLY_BOOL, false);
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES),
         msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES),
         MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,
         MENU_SETTING_ACTION, 0, 0);
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
      menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   unsigned p, retro_id;
   rarch_system_info_t *system = NULL;
   unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

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
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR),
         MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR,
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

   if (rarch_ctl(RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE, NULL))
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
            MENU_SETTING_ACTION, 0, 0);
   }

   system    = runloop_get_system_info();

   if (system)
   {
      settings_t *settings = config_get_ptr();
      unsigned device;
      for (p = 0; p < max_users; p++)
      {
         device = settings->uints.input_libretro_device[p];
         device &= RETRO_DEVICE_MASK;

         if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
         {
            for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND + 8; retro_id++)
            {
               char desc_label[64];
               char descriptor[255];
               const struct retro_keybind *auto_bind = NULL;
               const struct retro_keybind *keybind   = NULL;

               keybind   = &input_config_binds[p][retro_id];
               auto_bind = (const struct retro_keybind*)
                  input_config_get_bind_auto(p, retro_id);

               input_config_get_bind_string(descriptor,
                  keybind, auto_bind, sizeof(descriptor));

               if(!strstr(descriptor, "Auto"))
               {
                  const struct retro_keybind *keyptr =
                     &input_config_binds[p][retro_id];

                  snprintf(desc_label, sizeof(desc_label), "%s %s", msg_hash_to_str(keyptr->enum_idx), descriptor);
                  strlcpy(descriptor, desc_label, sizeof(descriptor));
               }

               menu_entries_append_enum(info->list, descriptor, "",
                     MSG_UNKNOWN,
                     MENU_SETTINGS_INPUT_DESC_BEGIN +
                     (p * (RARCH_FIRST_CUSTOM_BIND + 8)) +  retro_id, 0, 0);
            }
         }
         else if (device == RETRO_DEVICE_KEYBOARD)
         {
            for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
            {
               char descriptor[255];
               const struct retro_keybind *auto_bind = NULL;
               const struct retro_keybind *keybind   = NULL;

               keybind   = &input_config_binds[p][retro_id];
               auto_bind = (const struct retro_keybind*)
                  input_config_get_bind_auto(p, retro_id);

               input_config_get_bind_string(descriptor,
                  keybind, auto_bind, sizeof(descriptor));

               if(!strstr(descriptor, "Auto"))
               {
                  const struct retro_keybind *keyptr =
                     &input_config_binds[p][retro_id];

                  strlcpy(descriptor, msg_hash_to_str(keyptr->enum_idx), sizeof(descriptor));
               }

               menu_entries_append_enum(info->list, descriptor, "",
                     MSG_UNKNOWN,
                     MENU_SETTINGS_INPUT_DESC_KBD_BEGIN +
                     (p * RARCH_FIRST_CUSTOM_BIND) + retro_id, 0, 0);
            }
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
   const char *path             = info->path;

   if (string_is_empty(path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
      return 0;
   }

   str_list = dir_list_new(path, NULL, true,
         settings->bools.show_hidden_files, true, false);

   if (!str_list)
      return 0;

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
     if (settings->bools.menu_content_show_favorites)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
            MENU_ENUM_LABEL_GOTO_FAVORITES,
            MENU_SETTING_ACTION, 0, 0);
     if (settings->bools.menu_content_show_images)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
            MENU_ENUM_LABEL_GOTO_IMAGES,
            MENU_SETTING_ACTION, 0, 0);

     if (settings->bools.menu_content_show_music)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
            MENU_ENUM_LABEL_GOTO_MUSIC,
            MENU_SETTING_ACTION, 0, 0);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
     if (settings->bools.menu_content_show_video)
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
      char label[64];
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

      if (horizontal)
      {
         if (!string_is_empty(path))
            path = path_basename(path);
      }

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

static unsigned menu_displaylist_parse_cores(
      menu_handle_t       *menu,
      menu_displaylist_info_t *info)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;
   unsigned items_found         = 0;
   settings_t *settings         = config_get_ptr();
   const char *path             = info->path;

   if (string_is_empty(path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
      items_found++;
      return items_found;
   }

   str_list = dir_list_new(path, info->exts,
         true, settings->bools.show_hidden_files, true, false);

   {
      char *out_dir = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      out_dir[0] = '\0';

      fill_pathname_parent_dir(out_dir, path,
            PATH_MAX_LENGTH * sizeof(char));

      if (string_is_empty(out_dir))
      {
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY),
               path,
               MENU_ENUM_LABEL_PARENT_DIRECTORY,
               FILE_TYPE_PARENT_DIRECTORY, 0, 0);
      }

      free(out_dir);
   }

   if (!str_list)
   {
      const char *str = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND);
      menu_entries_append_enum(info->list, str, "",
            MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND, 0, 0, 0);
      items_found++;
      return items_found;
   }

   info->download_core = true;

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size == 0)
   {
      string_list_free(str_list);
      return 0;
   }

   for (i = 0; i < list_size; i++)
   {
      bool is_dir;
      char label[64];
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

      if (!string_is_empty(path))
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
      return 0;

   {
      enum msg_hash_enums enum_idx   = MSG_UNKNOWN;
      core_info_list_t *list         = NULL;
      const char *dir                = NULL;

      core_info_get_list(&list);

      menu_entries_get_last_stack(&dir, NULL, NULL, &enum_idx, NULL);

      list_size = file_list_get_size(info->list);

      for (i = 0; i < list_size; i++)
      {
         unsigned type                      = 0;
         const char *path                   = NULL;

         menu_entries_get_at_offset(info->list,
               i, &path, NULL, &type, NULL,
               NULL);

         if (type == FILE_TYPE_CORE)
         {
            char *core_path    = (char*)
               malloc(PATH_MAX_LENGTH * sizeof(char));
            char *display_name = (char*)
               malloc(PATH_MAX_LENGTH * sizeof(char));
            core_path[0]       =
            display_name[0]    = '\0';

            fill_pathname_join(core_path, dir, path,
                  PATH_MAX_LENGTH * sizeof(char));

            if (core_info_list_get_display_name(list,
                     core_path, display_name,
                     PATH_MAX_LENGTH * sizeof(char)))
               file_list_set_alt_at_offset(info->list, i, display_name);

            free(core_path);
            free(display_name);
         }
      }
      info->need_sort = true;
   }

   return items_found;
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
            new_playlist_names,
            sizeof(settings->arrays.playlist_names));
      strlcpy(settings->arrays.playlist_cores,
            new_playlist_cores,
            sizeof(settings->arrays.playlist_cores));

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
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS)))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST, info))
         return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup(
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_MUSIC_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup(
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      menu_displaylist_ctl(DISPLAYLIST_VIDEO_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)))
   {
      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts  = strdup(
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#if 0
      if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
               MENU_ENUM_LABEL_TAKE_SCREENSHOT,
               MENU_SETTING_ACTION_SCREENSHOT, 0, 0);
      else
         info->need_push_no_playlist_entries = true;
#endif
      menu_displaylist_ctl(DISPLAYLIST_IMAGES_HISTORY, info);
      return true;
   }
   else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      settings_t *settings  = config_get_ptr();

      filebrowser_clear_type();
      info->type = 42;

      if (!string_is_empty(info->exts))
         free(info->exts);
      if (!string_is_empty(info->label))
         free(info->label);

      info->exts = strdup(
            file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST));

      if (string_is_empty(settings->paths.directory_playlist))
      {
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->need_refresh                  = true;
         info->need_push_no_playlist_entries = true;
         info->need_push                     = true;

         return true;
      }
      else
      {
         if (!string_is_empty(info->path))
            free(info->path);

         info->path = strdup(settings->paths.directory_playlist);

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
   menu_displaylist_info_t info;
   menu_file_list_cbs_t *cbs      = NULL;
   const char *path               = NULL;
   const char *label              = NULL;
   unsigned type                  = 0;
   bool ret                       = false;
   enum msg_hash_enums enum_idx   = MSG_UNKNOWN;

   if (!entry)
      return false;

   menu_displaylist_info_init(&info);

   menu_entries_get_last_stack(&path, &label, &type, &enum_idx, NULL);

   info.list      = entry->list;
   info.menu_list = entry->stack;
   info.type      = type;
   info.enum_idx  = enum_idx;

   if (!string_is_empty(path))
      info.path  = strdup(path);

   if (!string_is_empty(label))
      info.label = strdup(label);

   if (!info.list)
      goto error;

   if (menu_displaylist_push_internal(label, entry, &info))
   {
      ret = menu_displaylist_process(&info);
      goto end;
   }

   cbs = menu_entries_get_last_stack_actiondata();

   if (cbs && cbs->action_deferred_push)
   {
      if (cbs->action_deferred_push(&info) != 0)
         goto error;
   }

   ret = true;

end:
   menu_displaylist_info_free(&info);

   return ret;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static void menu_displaylist_parse_playlist_generic(
      menu_handle_t *menu,
      menu_displaylist_info_t *info,
      const char *playlist_name,
      const char *playlist_path,
      int *ret)
{
   playlist_t *playlist = NULL;
   char *path_playlist  = NULL;

   menu_displaylist_set_new_playlist(menu, playlist_path);

   playlist             = playlist_get_cached();
   path_playlist        = strdup(playlist_name);

   if (playlist)
      *ret              = menu_displaylist_parse_playlist(info,
            playlist, path_playlist, true);

   free(path_playlist);
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
   if (settings->bools.menu_show_core_updater && !settings->bools.kiosk_mode_enable)
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

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
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
      file_list_set_alt_at_offset(info->list, 0,
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
      if (info->need_push_no_playlist_entries)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE),
               msg_hash_to_str(
                  MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE),
               MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE,
               MENU_INFO_MESSAGE, 0, 0);

      menu_driver_populate_entries(info);
      ui_companion_driver_notify_list_loaded(info->list, info->menu_list);
   }
   return true;
}

void menu_displaylist_info_free(menu_displaylist_info_t *info)
{
   if (!info)
      return;
   if (info->exts)
      free(info->exts);
   if (info->path_b)
      free(info->path_b);
   if (info->path_c)
      free(info->path_c);
   if (info->label)
      free(info->label);
   if (info->path)
      free(info->path);
   info->exts   = NULL;
   info->path_b = NULL;
   info->path_c = NULL;
   info->label  = NULL;
   info->path   = NULL;
}

void menu_displaylist_info_init(menu_displaylist_info_t *info)
{
   if (!info)
      return;

   info->enum_idx                 = MSG_UNKNOWN;
   info->need_sort                = false;
   info->need_refresh             = false;
   info->need_entries_refresh     = false;
   info->need_push_no_playlist_entries = false;
   info->need_push                = false;
   info->need_clear               = false;
   info->push_builtin_cores       = false;
   info->download_core            = false;
   info->need_navigation_clear    = false;
   info->type                     = 0;
   info->type_default             = 0;
   info->flags                    = 0;
   info->directory_ptr            = 0;
   info->label                    = NULL;
   info->path                     = NULL;
   info->path_b                   = NULL;
   info->path_c                   = NULL;
   info->exts                     = NULL;
   info->list                     = NULL;
   info->menu_list                = NULL;
   info->setting                  = NULL;
}

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type, void *data)
{
   size_t i;
   menu_ctx_displaylist_t disp_list;
   menu_handle_t       *menu     = NULL;
   bool load_content             = true;
   bool use_filebrowser          = false;
   unsigned count                = 0;
   int ret                       = 0;
   menu_displaylist_info_t *info = (menu_displaylist_info_t*)data;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;

   disp_list.info = info;
   disp_list.type = type;

   if (menu_driver_push_list(&disp_list))
      return true;

   switch (type)
   {
#ifdef HAVE_LAKKA_SWITCH
      case DISPLAYLIST_SWITCH_CPU_PROFILE:
      {
         runloop_msg_queue_push("Warning : extented overclocking can damage the Switch", 1, 90, true);
      
         char current_profile[PATH_MAX_LENGTH];
         
         FILE* profile = popen("cpu-profile get", "r");          
         fgets(current_profile, PATH_MAX_LENGTH, profile);  
         pclose(profile);
         
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         
         char text[PATH_MAX_LENGTH];
         
         snprintf(text, sizeof(text), "Current profile : %s", current_profile);
         
         menu_entries_append_enum(info->list,
            text,
            "",
            0,
            MENU_INFO_MESSAGE, 0, 0);
         
         const size_t profiles_count = sizeof(SWITCH_CPU_PROFILES)/sizeof(SWITCH_CPU_PROFILES[1]);
                  
         
         for (int i = 0; i < profiles_count; i++)
         {
            char* profile = SWITCH_CPU_PROFILES[i];
            char* speed = SWITCH_CPU_SPEEDS[i];
               
            char title[PATH_MAX_LENGTH] = {0};
               
            snprintf(title, sizeof(title), "%s (%s)", profile, speed);
               
         menu_entries_append_enum(info->list, 
            title,
            "",
            0, MENU_SET_SWITCH_CPU_PROFILE, 0, i);
            
         }         
            
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
      
         break;
      }
      case DISPLAYLIST_SWITCH_GPU_PROFILE:
      {
         runloop_msg_queue_push("Warning : extented overclocking can damage the Switch", 1, 90, true);

         char current_profile[PATH_MAX_LENGTH];
         
         FILE* profile = popen("gpu-profile get", "r");          
         fgets(current_profile, PATH_MAX_LENGTH, profile);  
         pclose(profile);
         
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         
         char text[PATH_MAX_LENGTH];
         
         snprintf(text, sizeof(text), "Current profile : %s", current_profile);
         
         menu_entries_append_enum(info->list,
            text,
            "",
            0,
            MENU_INFO_MESSAGE, 0, 0);
         
         const size_t profiles_count = sizeof(SWITCH_GPU_PROFILES)/sizeof(SWITCH_GPU_PROFILES[1]);
                     
         for (int i = 0; i < profiles_count; i++)
         {
            char* profile = SWITCH_GPU_PROFILES[i];
            char* speed = SWITCH_GPU_SPEEDS[i];
            
            char title[PATH_MAX_LENGTH] = {0};
              
            snprintf(title, sizeof(title), "%s (%s)", profile, speed);
               
         menu_entries_append_enum(info->list, 
             title,
            "",
             0, MENU_SET_SWITCH_GPU_PROFILE, 0, i); 
         }         
               
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         
         break;
      }
      case DISPLAYLIST_SWITCH_BACKLIGHT_CONTROL:
      {
        menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
        
        const size_t brightness_count = sizeof(SWITCH_BRIGHTNESS)/sizeof(SWITCH_BRIGHTNESS[1]);
        
        for (int i = 0; i < brightness_count; i++)
        {
        	char title[PATH_MAX_LENGTH] = {0};
        	
        	snprintf(title, sizeof(title), "Set to %d%%", SWITCH_BRIGHTNESS[i]);
        	
        	menu_entries_append_enum(info->list, 
            title,
            "",
            0, MENU_SET_SWITCH_BRIGHTNESS, 0, i);
         }
      
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         
         break;
      }
#endif
      case DISPLAYLIST_MUSIC_LIST:
         {
            char combined_path[PATH_MAX_LENGTH];
            const char *ext  = NULL;

            combined_path[0] = '\0';

            fill_pathname_join(combined_path, menu->scratch2_buf,
                  menu->scratch_buf, sizeof(combined_path));

            ext = path_get_extension(combined_path);

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (audio_driver_mixer_extension_supported(ext))
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION),
                     msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION),
                     MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION,
                     FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
               count++;

               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY),
                     msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY),
                     MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
                     FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
               count++;
            }

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            {
               settings_t      *settings     = config_get_ptr();
               if (settings->bools.multimedia_builtin_mediaplayer_enable)
               {
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN_MUSIC),
                        msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC),
                        MENU_ENUM_LABEL_RUN_MUSIC,
                        FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
                  count++;
               }
            }
#endif
         }

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      case DISPLAYLIST_MIXER_STREAM_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         {
            char lbl_play[128];
            char lbl_play_looped[128];
            char lbl_play_sequential[128];
            char lbl_remove[128];
            char lbl_stop[128];
            char lbl_volume[128];
            unsigned id               = info->type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN;

            lbl_remove[0] = lbl_stop[0] = lbl_play[0] = lbl_play_looped[0] = '\0';
            lbl_volume[0] = lbl_play_sequential[0] = '\0';

            snprintf(lbl_volume, sizeof(lbl_volume), "mixer_stream_%d_action_volume", id);
            snprintf(lbl_stop, sizeof(lbl_stop), "mixer_stream_%d_action_stop", id);
            snprintf(lbl_remove, sizeof(lbl_remove), "mixer_stream_%d_action_remove", id);
            snprintf(lbl_play, sizeof(lbl_play), "mixer_stream_%d_action_play", id);
            snprintf(lbl_play_looped, sizeof(lbl_play_looped), "mixer_stream_%d_action_play_looped", id);
            snprintf(lbl_play_sequential, sizeof(lbl_play_sequential), "mixer_stream_%d_action_play_sequential", id);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY),
                  lbl_play,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN  +  id),
                  0, 0);
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED),
                  lbl_play_looped,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN  +  id),
                  0, 0);
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL),
                  lbl_play_sequential,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN  +  id),
                  0, 0);
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP),
                  lbl_stop,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN  +  id),
                  0, 0);
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE),
                  lbl_remove,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN  +  id),
                  0, 0);
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME),
                  lbl_volume,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN  +  id),
                  0, 0);
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

            if (!string_is_empty(info->path_b))
               free(info->path_b);
            if (!string_is_empty(info->label))
               free(info->label);

            info->path_b = strdup(str_list->elems[1].data);
            info->label  = strdup(str_list->elems[0].data);

            string_list_free(str_list);
         }

#ifdef HAVE_LIBRETRODB
         ret = menu_displaylist_parse_database_entry(menu, info);
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
         if (!string_is_empty(info->path))
            free(info->path);
         info->path         = strdup(info->path_b);

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
         print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len,
               FILE_TYPE_DOWNLOAD_CORE_CONTENT, true, false);
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
               strlcpy(menu->core_buf, str_list->elems[1].data, menu->core_len);
            print_buf_lines(info->list, menu->core_buf, new_label,
                  (int)menu->core_len, FILE_TYPE_DOWNLOAD_URL, false, false);
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
            settings_t      *settings     = config_get_ptr();

            new_label[0] = '\0';

            fill_pathname_join(new_label,
                  settings->paths.network_buildbot_assets_url,
                  "cores", sizeof(new_label));
            print_buf_lines(info->list, menu->core_buf, new_label,
                  (int)menu->core_len, FILE_TYPE_DOWNLOAD_URL, true, false);
            info->need_push    = true;
            info->need_refresh = true;
            info->need_clear   = true;
#endif
         }
         break;
      case DISPLAYLIST_CORES_UPDATER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_CORE, true, true);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_THUMBNAILS_UPDATER:
#ifdef HAVE_NETWORKING
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT,
               true, false);
         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_LAKKA:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_LAKKA,
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
            playlist_t *playlist          = NULL;
            settings_t      *settings     = config_get_ptr();

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

            playlist = playlist_get_cached();

            if (playlist)
            {
               playlist_qsort(playlist);

               ret = menu_displaylist_parse_playlist(info,
                     playlist, path_playlist, false);
            }

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
            settings_t      *settings     = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            if (settings->bools.history_list_enable)
               menu_displaylist_parse_playlist_generic(
                     menu, info,
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
            }
         }

         ret                   = 0;
         info->need_refresh    = true;
         info->need_push       = true;
         break;
      case DISPLAYLIST_FAVORITES:
         {
            settings_t      *settings     = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            menu_displaylist_parse_playlist_generic(menu, info,
                  "favorites",
                  settings->paths.path_content_favorites,
                  &ret);
         }

         ret                   = 0;
         info->need_refresh    = true;
         info->need_push       = true;
         break;
      case DISPLAYLIST_MUSIC_HISTORY:
         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.history_list_enable)
               menu_displaylist_parse_playlist_generic(menu, info,
                     "music_history",
                     settings->paths.path_content_music_history,
                     &ret);
            else
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                     MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                     MENU_INFO_MESSAGE, 0, 0);
               ret = 0;
            }
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_VIDEO_HISTORY:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     "video_history",
                     settings->paths.path_content_video_history,
                     &ret);
               count++;
            }
            else
               ret = 0;
         }
#endif

         if (count == 0)
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
         }

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
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
#if defined(HAVE_NETWORKING) && !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU) && !defined(SWITCH)
         count = menu_displaylist_parse_network_info(info);
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

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
         cheevos_populate_menu(info);
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
                     file_list_set_alt_at_offset(info->list, 0,
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
                     snprintf(new_entry, sizeof(new_entry), "%s (%s)",
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE),
                           core_name);
                     strlcpy(new_lbl_entry, core_path, sizeof(new_lbl_entry));
                     new_type = MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE;
                  }
                  else if (core_path)
                  {
                     menu_entries_append_enum(info->list, core_path,
                           msg_hash_to_str(MENU_ENUM_LABEL_DETECT_CORE_LIST_OK),
                           MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
                           FILE_TYPE_CORE, 0, 0);

                     file_list_set_alt_at_offset(info->list, j, core_name);
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
                        file_list_set_alt_at_offset(info->list, 0,
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
                     snprintf(new_entry, sizeof(new_entry), "%s (%s)",
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE),
                           core_name);
                     new_lbl_entry[0] = '\0';
                     new_type         = MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION_CURRENT_CORE;
                  }
                  else if (core_path)
                  {
                     menu_entries_append_enum(info->list, core_path, "",
                           MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
                           FILE_TYPE_CORE, 0, 0);
                     file_list_set_alt_at_offset(info->list, j, core_name);
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
            size_t                   opts = 0;
            settings_t      *settings     = config_get_ptr();

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

            if (opts != 0)
            {
               core_option_manager_t *coreopts = NULL;

               rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

               for (i = 0; i < opts; i++)
               {
                  menu_entries_append_enum(info->list,
                        core_option_manager_get_desc(coreopts, i), "",
                        MENU_ENUM_LABEL_CORE_OPTION_ENTRY,
                        (unsigned)(MENU_SETTINGS_CORE_OPTION_START + i), 0, 0);
                  count++;
               }
            }
         }

         if (count == 0)
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
               MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME,
               PARSE_ONLY_BOOL, false);
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
                     (enum msg_hash_enums)(
                        MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i),
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
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MIDI_DRIVER,
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

         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.menu_show_advanced_settings)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_PERFCNT_ENABLE,
                     PARSE_ONLY_BOOL, false);
         }

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
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE,
               PARSE_ONLY_BOOL, false);

         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.menu_show_advanced_settings)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE,
                     PARSE_ONLY_BOOL, false);
         }

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
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_REWIND_BUFFER_SIZE,
               PARSE_ONLY_SIZE, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP,
               PARSE_ONLY_UINT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST:
      {
         rarch_setting_t *setting = NULL;
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if ( !cheat_manager_state.memory_initialized)
            cheat_manager_initialize_memory(NULL,true) ;

         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS);
         if ( setting )
            setting->max = cheat_manager_state.total_memory_size==0?0:cheat_manager_state.total_memory_size-1;

         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION);
         if ( setting )
            setting->max = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0 ;

         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY);
         if ( setting )
            setting->max = cheat_manager_state.actual_memory_size>0?cheat_manager_state.actual_memory_size-1:0 ;


         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_IDX,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_STATE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_DESC,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_HANDLER,
               PARSE_ONLY_UINT, false);
         if ( cheat_manager_state.working_cheat.handler == CHEAT_HANDLER_TYPE_EMU)
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_CODE,
                  PARSE_ONLY_STRING, false);
         else
         {
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_MEMORY_SEARCH_SIZE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_TYPE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_VALUE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_ADDRESS,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_TYPE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_PORT,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_STRENGTH,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_DURATION,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_STRENGTH,
                  PARSE_ONLY_UINT, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_DURATION,
                  PARSE_ONLY_UINT, false);
         }

         /* Inspect Memory At this Address */

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER),
               MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER,
               MENU_SETTING_ACTION, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE),
               MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE,
               MENU_SETTING_ACTION, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_AFTER),
               MENU_ENUM_LABEL_CHEAT_COPY_AFTER,
               MENU_SETTING_ACTION, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_BEFORE),
               MENU_ENUM_LABEL_CHEAT_COPY_BEFORE,
               MENU_SETTING_ACTION, 0, 0);
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DELETE),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE),
               MENU_ENUM_LABEL_CHEAT_DELETE,
               MENU_SETTING_ACTION, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      }
      case DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST:
      {
         char cheat_label[64];
         rarch_setting_t *setting;
         unsigned int address = 0;
         unsigned int address_mask = 0;
         unsigned int prev_val = 0;
         unsigned int curr_val = 0 ;

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);


         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_START_OR_RESTART,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_LT,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_LTE,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_GT,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_GTE,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQ,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS,
               PARSE_ONLY_UINT, false);

         cheat_label[0] = '\0';
         snprintf(cheat_label, sizeof(cheat_label),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES), cheat_manager_state.num_matches);

         menu_entries_append_enum(info->list,
               cheat_label,
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_MATCHES),
               MENU_ENUM_LABEL_CHEAT_ADD_MATCHES,
               MENU_SETTING_ACTION, 0, 0);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_DELETE_MATCH,
               PARSE_ONLY_UINT, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_COPY_MATCH,
               PARSE_ONLY_UINT, false);

         cheat_label[0] = '\0';

         cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_VIEW, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val) ;
         snprintf(cheat_label, sizeof(cheat_label),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_MATCH), address, address_mask);

         menu_entries_append_enum(info->list,
               cheat_label,
               "",
               MSG_UNKNOWN,
               MENU_SETTINGS_CHEAT_MATCH, 0, 0);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY,
               PARSE_ONLY_UINT, false);



         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_DELETE_MATCH);
         if (setting)
            setting->max = cheat_manager_state.num_matches-1;
         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_COPY_MATCH);
         if (setting)
            setting->max = cheat_manager_state.num_matches-1;
         setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY);
         if (setting)
            setting->max = cheat_manager_state.actual_memory_size>0?cheat_manager_state.actual_memory_size-1:0 ;

         info->need_refresh = true;
         info->need_push    = true;
         break;
      }
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
               MENU_ENUM_LABEL_FPS_SHOW,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_STATISTICS_SHOW,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_FRAMECOUNT_SHOW,
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
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
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
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
               PARSE_ONLY_BOOL, false);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS,
               PARSE_ACTION, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_INFORMATION,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_HELP,
               PARSE_ONLY_BOOL, false);
#ifdef HAVE_QT
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SHOW_WIMP,
               PARSE_ONLY_UINT, false);
#endif
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_REBOOT,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
               PARSE_ONLY_STRING, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_ADD,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_TIMEDATE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_TIMEDATE_STYLE,
               PARSE_ONLY_UINT, false);
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
      case DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS,
               PARSE_ONLY_BOOL, false);
         if (video_shader_any_supported())
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS,
                  PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_REWIND,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
               PARSE_ONLY_BOOL, false);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MENU_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_WALLPAPER,
                  PARSE_ONLY_PATH, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_LINEAR_FILTER,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR,
                  PARSE_ONLY_HEX, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_ENTRY_HOVER_COLOR,
                  PARSE_ONLY_HEX, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_TITLE_COLOR,
                  PARSE_ONLY_HEX, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_ALPHA_FACTOR,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_SCALE_FACTOR,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_FONT,
                  PARSE_ONLY_PATH, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_LAYOUT,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_THEME,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_THUMBNAILS,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_LEFT_THUMBNAILS,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_SUSTAINED_PERFORMANCE_MODE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

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
               MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD,
               PARSE_ONLY_STRING, false);
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
#ifdef HAVE_QT
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_UI_COMPANION_TOGGLE,
               PARSE_ONLY_BOOL, false);
#endif
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
               MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_UPDATER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
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

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_WIFI_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_NETWORKING
         {
            settings_t      *settings     = config_get_ptr();
            if (!string_is_equal(settings->arrays.wifi_driver, "null"))
            {
               struct string_list *ssid_list = string_list_new();
               driver_wifi_get_ssids(ssid_list);

               if (ssid_list->size == 0)
                  task_push_wifi_scan(wifi_scan_callback);
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
                     count++;
                  }
               }
            }
         }
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_NETWORKS_FOUND),
                  MENU_ENUM_LABEL_NO_NETWORKS_FOUND,
                  0, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_NETWORK_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,
                  PARSE_ONLY_BOOL, false) != -1)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,
                  PARSE_ONLY_BOOL, false) != -1)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,
                  PARSE_ONLY_STRING, false) != -1)
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
                  MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL,
                  PARSE_ONLY_UINT, false) != -1)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG,
                  PARSE_ONLY_UINT, false) != -1)
            count++;

         {
            unsigned user;
            for (user = 0; user < MAX_USERS; user++)
            {
               if (menu_displaylist_parse_settings_enum(menu, info,
                        (enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + user),
                        PARSE_ONLY_BOOL, false) != -1)
                  count++;
            }
         }

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
            unsigned user;
            unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
            for(user = 0; user < max_users; user++)
            {
               if (menu_displaylist_parse_settings_enum(menu, info,
                        (enum msg_hash_enums)(
                           MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user),
                        PARSE_ONLY_BOOL, false) != -1)
                  count++;
            }
         }

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_STDIN_CMD_ENABLE,
                  PARSE_ONLY_BOOL, false) != -1)
            count++;

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_UPDATER_SETTINGS,
                  PARSE_ACTION, false) != -1)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_LAKKA_SERVICES_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SSH_ENABLE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SAMBA_ENABLE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_BLUETOOTH_ENABLE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_USER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_LIST,
               PARSE_ACTION, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_NETPLAY_NICKNAME,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USER_LANGUAGE,
               PARSE_ONLY_UINT, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

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

         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_CAMERA_ALLOW,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_DISCORD_ALLOW,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_LOCATION_ALLOW,
                  PARSE_ONLY_BOOL, true) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MIDI_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MIDI_INPUT,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MIDI_OUTPUT,
               PARSE_ONLY_STRING, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MIDI_VOLUME,
               PARSE_ONLY_UINT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
               PARSE_ONLY_BOOL, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_VIDEO_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,
               PARSE_ACTION, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE,
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
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED,
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
               MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
               PARSE_ONLY_BOOL, false);
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
               MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC,
               PARSE_ONLY_BOOL, false);
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
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
      case DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST:
         {
            unsigned i;
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            for (i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++)
            {
               char msg[128];
               char msg_lbl[128];
               snprintf(msg, sizeof(msg), "Mixer Stream #%d :\n", i+1);
               snprintf(msg_lbl, sizeof(msg_lbl), "audio_mixer_stream_%d\n", i);
               menu_entries_append_enum(info->list, msg, msg_lbl,
                     MSG_UNKNOWN,
                     (MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN  +  i),
                     0, 0);
               count++;
            }

            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_AUDIO_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_MIDI_SETTINGS,   PARSE_ACTION, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,   PARSE_ACTION, false) == 0)
            count++;
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_ENABLE_MENU,
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
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         {
            settings_t *settings      = config_get_ptr();
            if (string_is_not_equal(settings->arrays.audio_resampler, "null"))
            {
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY,
                     PARSE_ONLY_UINT, false);
               count++;
            }
         }
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
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
               PARSE_ONLY_INT, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_MAX_USERS,
               PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS,
               PARSE_ONLY_BOOL, false);
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_ICADE_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
               PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE,
               PARSE_ONLY_BOOL, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH,
               PARSE_ONLY_BOOL, false);
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
               MENU_ENUM_LABEL_INPUT_BIND_HOLD, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_TURBO_PERIOD, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_DUTY_CYCLE, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_BIND_MODE, PARSE_ONLY_UINT, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS, PARSE_ACTION, false);

#ifdef HAVE_LIBNX
         {
            unsigned user;

            for (user = 0; user < 8; user++)
            {
               char key_split_joycon[PATH_MAX_LENGTH];
               unsigned val = user + 1;

               key_split_joycon[0] = '\0';

               snprintf(key_split_joycon, sizeof(key_split_joycon),
                     "%s_%u",
                     msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SPLIT_JOYCON), val);

               menu_displaylist_parse_settings(menu, info,
                     key_split_joycon, PARSE_ONLY_UINT, true);
            }
         }
#endif

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
      case DISPLAYLIST_LATENCY_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RUN_AHEAD_ENABLED,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RUN_AHEAD_FRAMES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

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
               MENU_ENUM_LABEL_LATENCY_SETTINGS, PARSE_ACTION, false);
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
         {
            settings_t      *settings     = config_get_ptr();
            if (string_is_not_equal(settings->arrays.record_driver, "null"))
               ret = menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_RECORDING_SETTINGS,   PARSE_ACTION, false);
         }
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,   PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS,  PARSE_ACTION, false);
         ret = menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_WIFI_SETTINGS,   PARSE_ACTION, false);
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
         ret = menu_displaylist_parse_horizontal_list(menu, info);

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_content_actions(menu, info);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CONTENT_SETTINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_load_content_settings(menu, info);

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
         count = menu_displaylist_parse_add_content_list(info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         ret                = 0;
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
         count = menu_displaylist_parse_scan_directory_list(info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         ret                = 0;
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_NETPLAY_ROOM_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_NETWORKING
         netplay_refresh_rooms_menu(info->list);
#endif

         ret                = 0;
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_LOAD_CONTENT_LIST:
      case DISPLAYLIST_LOAD_CONTENT_SPECIAL:
         {
            settings_t      *settings     = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (!string_is_empty(settings->paths.directory_menu_content))
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                     msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                     MENU_ENUM_LABEL_FAVORITES,
                     MENU_SETTING_ACTION, 0, 0);

            if (settings->bools.menu_content_show_favorites)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
                     MENU_ENUM_LABEL_GOTO_FAVORITES,
                     MENU_SETTING_ACTION, 0, 0);

            if (settings->bools.menu_content_show_images)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
                     MENU_ENUM_LABEL_GOTO_IMAGES,
                     MENU_SETTING_ACTION, 0, 0);

            if (settings->bools.menu_content_show_music)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
                     MENU_ENUM_LABEL_GOTO_MUSIC,
                     MENU_SETTING_ACTION, 0, 0);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            if (settings->bools.menu_content_show_video)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
                     MENU_ENUM_LABEL_GOTO_VIDEO,
                     MENU_SETTING_ACTION, 0, 0);
#endif
         }

         {
            core_info_list_t *list        = NULL;
            core_info_get_list(&list);
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
         count              = menu_displaylist_parse_options(info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_options_cheats(info, menu);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_options_remappings(menu, info);

         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_OVERRIDES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.quick_menu_show_save_core_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
                     MENU_SETTING_ACTION, 0, 0);
               count++;
            }

            if (settings->bools.quick_menu_show_save_content_dir_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
                     MENU_SETTING_ACTION, 0, 0);
               count++;
            }

            if (settings->bools.quick_menu_show_save_game_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
                     MENU_SETTING_ACTION, 0, 0);
               count++;
            }

         }

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

         info->need_push = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            video_shader_ctx_t shader_info;
            video_shader_driver_get_current_shader(&shader_info);

            if (shader_info.data)
               count = deferred_push_video_shader_parameters_common(
                     info, shader_info.data,
                     (type == DISPLAYLIST_SHADER_PARAMETERS)
                     ? MENU_SETTINGS_SHADER_PARAMETER_0
                     : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
                     );

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_SHADER_PARAMETERS),
                     MENU_ENUM_LABEL_NO_SHADER_PARAMETERS,
                     0, 0, 0);

            ret             = 0;
            info->need_push = true;
         }
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
      {
         settings_t      *settings      = config_get_ptr();
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_RECORD_CONFIG,
               PARSE_ONLY_PATH, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_STREAM_CONFIG,
               PARSE_ONLY_PATH, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_STREAMING_MODE,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_STREAMING_TITLE,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         if (settings->uints.streaming_mode == STREAMING_MODE_LOCAL)
         {
            /* To-Do: Refresh on settings->uints.streaming_mode change to show this parameter */
            if (menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_UDP_STREAM_PORT,
                  PARSE_ONLY_UINT, false) == 0)
               count++;
         }
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_STREAMING_URL,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_GPU_RECORD,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                  MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                  0, 0, 0);

         info->need_push    = true;
      }
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            settings_t      *settings      = config_get_ptr();
            rarch_system_info_t *system    = runloop_get_system_info();

            if (system)
            {
               if (!string_is_empty(system->info.library_name) &&
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
            if (settings->bools.menu_show_load_core)
                  menu_displaylist_parse_settings_enum(menu, info,
                        MENU_ENUM_LABEL_CORE_LIST, PARSE_ACTION, false);
            }

            if (settings->bools.menu_show_load_content)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION, false);
            if (settings->bools.menu_content_show_history)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                     PARSE_ACTION, false);
            if (settings->bools.menu_content_show_add)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                     PARSE_ACTION, false);
            if (settings->bools.menu_content_show_netplay)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_NETPLAY,
                     PARSE_ACTION, false);
            if (settings->bools.menu_show_online_updater)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_SETTINGS, PARSE_ACTION, false);
            if (settings->bools.menu_show_information)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION, false);
            menu_displaylist_parse_settings_enum(menu, info,
                  MENU_ENUM_LABEL_RESTART_RETROARCH,
                  PARSE_ACTION, false);
            if (settings->bools.menu_show_configurations)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION, false);
            if (settings->bools.menu_show_help)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION, false);
            if (settings->bools.menu_show_quit_retroarch)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION, false);
#ifdef HAVE_LAKKA_SWITCH
            menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
               PARSE_ACTION, false);

            menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL,
               PARSE_ACTION, false);
#endif
            if (settings->bools.menu_show_reboot)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION, false);
            if (settings->bools.menu_show_shutdown)
               menu_displaylist_parse_settings_enum(menu, info,
                     MENU_ENUM_LABEL_SHUTDOWN,
                     PARSE_ACTION, false);
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

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
               PARSE_ACTION, false) == 0)
            count++;

#ifdef HAVE_NETWORKING
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE,
               PARSE_ACTION, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_ACCOUNTS_TWITCH,
               PARSE_ACTION, false) == 0)
            count++;
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_USERNAME,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_CHEEVOS_PASSWORD,
               PARSE_ONLY_STRING, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_YOUTUBE_STREAM_KEY,
               PARSE_ONLY_STRING, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_ACCOUNTS_TWITCH_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (menu_displaylist_parse_settings_enum(menu, info,
               MENU_ENUM_LABEL_TWITCH_STREAM_KEY,
               PARSE_ONLY_STRING, false) == 0)
            count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
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

            menu_driver_list_cache(&list_info);

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

            menu_driver_list_cache(&list_info);

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
         {
            settings_t *settings      = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            filebrowser_clear_type();
            info->type_default = FILE_TYPE_RDB;
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(
                  file_path_str(FILE_PATH_RDB_EXTENSION));
            info->enum_idx     = MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST;
            load_content       = false;
            use_filebrowser    = true;
            if (info->path)
               free(info->path);
            info->path        = strdup(settings->paths.path_content_database);
         }
         break;
      case DISPLAYLIST_DATABASE_CURSORS:
         {
            settings_t *settings      = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            filebrowser_clear_type();
            info->type_default = FILE_TYPE_CURSOR;
            load_content       = false;
            use_filebrowser    = true;
            if (!string_is_empty(info->exts))
               free(info->exts);
            if (info->path)
               free(info->path);
            info->exts         = strdup("dbc");
            info->path         = strdup(settings->paths.directory_cursor);
         }
         break;
      case DISPLAYLIST_CONFIG_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();

         info->type_default = FILE_TYPE_CONFIG;

         if (!string_is_empty(info->exts))
            free(info->exts);

         info->exts         = strdup("cfg");
         load_content       = false;
         use_filebrowser    = true;
         break;
      case DISPLAYLIST_SHADER_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            char new_exts[PATH_MAX_LENGTH];
            union string_list_elem_attr attr;
            struct string_list *str_list    = NULL;
            bool is_preset                  = false;
            attr.i = 0;

            new_exts[0] = '\0';
            str_list    = string_list_new();

            filebrowser_clear_type();
            info->type_default = FILE_TYPE_SHADER_PRESET;

            if (video_shader_is_supported(RARCH_SHADER_CG) &&
                  video_shader_get_type_from_ext("cgp", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "cgp", attr);

            if (video_shader_is_supported(RARCH_SHADER_GLSL) &&
                  video_shader_get_type_from_ext("glslp", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "glslp", attr);

            if (video_shader_is_supported(RARCH_SHADER_SLANG) &&
                  video_shader_get_type_from_ext("slangp", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "slangp", attr);
            string_list_join_concat(new_exts, sizeof(new_exts), str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
            string_list_free(str_list);
            use_filebrowser    = true;
         }
         break;
      case DISPLAYLIST_SHADER_PASS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            char new_exts[PATH_MAX_LENGTH];
            union string_list_elem_attr attr;
            struct string_list *str_list    = NULL;
            bool is_preset                  = false;

            attr.i = 0;

            new_exts[0] = '\0';
            str_list    = string_list_new();

            filebrowser_clear_type();
            info->type_default = FILE_TYPE_SHADER;


            if (video_shader_is_supported(RARCH_SHADER_CG) &&
                  video_shader_get_type_from_ext("cg", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "cg", attr);

            if (video_shader_is_supported(RARCH_SHADER_GLSL) &&
                  video_shader_get_type_from_ext("glsl", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "glsl", attr);

            if (video_shader_is_supported(RARCH_SHADER_SLANG) &&
                  video_shader_get_type_from_ext("slang", &is_preset)
                  != RARCH_SHADER_NONE)
               string_list_append(str_list, "slang", attr);

            string_list_join_concat(new_exts, sizeof(new_exts), str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
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
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("filt");
         break;
      case DISPLAYLIST_IMAGES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (     (filebrowser_get_type() != FILEBROWSER_SELECT_FILE)
               && (filebrowser_get_type() != FILEBROWSER_SELECT_IMAGE))
            filebrowser_clear_type();
         info->type_default = FILE_TYPE_IMAGE;
         {
            char new_exts[PATH_MAX_LENGTH];
            union string_list_elem_attr attr;
            struct string_list *str_list     = string_list_new();

            attr.i = 0;
            new_exts[0] = '\0';

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
            string_list_join_concat(new_exts, sizeof(new_exts), str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
            string_list_free(str_list);
         }
         use_filebrowser    = true;
         break;
      case DISPLAYLIST_PLAYLIST:
         menu_displaylist_parse_playlist_generic(menu, info,
               path_basename(info->path),
               info->path,
               &ret);
         ret = 0;

         if (ret == 0)
         {
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_IMAGES_HISTORY:
#ifdef HAVE_IMAGEVIEWER
         {
            settings_t *settings      = config_get_ptr();
            if (settings->bools.history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     "images_history",
                     settings->paths.path_content_image_history,
                     &ret);
               count++;
            }
         }
#endif
         if (count == 0)
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                  MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
         }

         ret                   = 0;
         info->need_refresh    = true;
         info->need_push       = true;
         break;
      case DISPLAYLIST_AUDIO_FILTERS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_AUDIOFILTER;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("dsp");
         break;
      case DISPLAYLIST_CHEAT_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_CHEAT;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("cht");
         break;
      case DISPLAYLIST_CONTENT_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_PLAIN;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("lpl");
         break;
      case DISPLAYLIST_FONTS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_FONT;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("ttf");
         break;
      case DISPLAYLIST_OVERLAYS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_OVERLAY;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("cfg");
         break;
      case DISPLAYLIST_STREAM_CONFIG_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_STREAM_CONFIG;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("cfg");
         break;
      case DISPLAYLIST_RECORD_CONFIG_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_RECORD_CONFIG;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("cfg");
         break;
      case DISPLAYLIST_REMAP_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default    = FILE_TYPE_REMAP;
         load_content          = false;
         use_filebrowser       = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts            = strdup("rmp");
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
            {
               if (!string_is_empty(info->exts))
                  free(info->exts);
               info->exts = strdup(ext_name);
            }
         }

         count = menu_displaylist_parse_cores(menu, info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         info->need_refresh       = true;
         info->need_push          = true;
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
      case DISPLAYLIST_DROPDOWN_LIST:
         {
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (strstr(info->path, "core_option_"))
            {
               struct string_list *tmp_str_list = string_split(info->path, "_");

               if (tmp_str_list && tmp_str_list->size > 0)
               {
                  core_option_manager_t *coreopts = NULL;

                  rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

                  if (coreopts)
                  {
                     unsigned size                   = (unsigned)tmp_str_list->size;
                     unsigned i                      = atoi(tmp_str_list->elems[size-1].data);
                     struct core_option *option      = NULL;

                     i--;

                     option                          = (struct core_option*)&coreopts->opts[i];

                     if (option)
                     {
                        unsigned k;
                        for (k = 0; k < option->vals->size; k++)
                        {
                           const char *str = option->vals->elems[k].data;

                           if (!string_is_empty(str))
                           {
                              char val_d[256];
                              snprintf(val_d, sizeof(val_d), "%d", i);
                              menu_entries_append_enum(info->list,
                                    str,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM, k, 0);
                              count++;
                           }
                        }
                     }
                  }
               }

            }
            else
            {
               enum msg_hash_enums enum_idx = (enum msg_hash_enums)atoi(info->path);
               rarch_setting_t     *setting = menu_setting_find_enum(enum_idx);

               if (setting)
               {
                  switch (setting->type)
                  {
                     case ST_STRING_OPTIONS:
                        {
                           struct string_list *tmp_str_list = string_split(setting->values, "|");

                           if (tmp_str_list && tmp_str_list->size > 0)
                           {
                              unsigned i;
                              unsigned size = (unsigned)tmp_str_list->size;

                              for (i = 0; i < size; i++)
                              {
                                 char val_d[256];
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 menu_entries_append_enum(info->list,
                                       tmp_str_list->elems[i].data,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM, i, 0);
                              }
                           }
                        }
                        break;
                     case ST_INT:
                        {
                           float i;
                           int32_t orig_value     = *setting->value.target.integer;
                           unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_INT_ITEM;
                           float step             = setting->step;
                           double min             = setting->enforce_minrange ? setting->min : 0.00;
                           double max             = setting->enforce_maxrange ? setting->max : 999.00;

                           if (setting->get_string_representation)
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[256], val_d[256];
                                 int val = (int)i;

                                 *setting->value.target.integer = val;

                                 setting->get_string_representation(setting,
                                       val_s, sizeof(val_s));
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0);
                              }

                              *setting->value.target.integer = orig_value;
                           }
                           else
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[16], val_d[16];
                                 int val = (int)i;

                                 snprintf(val_s, sizeof(val_s), "%d", val);
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);

                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0);
                              }
                           }
                        }
                        break;
                     case ST_FLOAT:
                        {
                           float i;
                           float orig_value       = *setting->value.target.fraction;
                           unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM;
                           float step             = setting->step;
                           double min             = setting->enforce_minrange ? setting->min : 0.00;
                           double max             = setting->enforce_maxrange ? setting->max : 999.00;

                           if (setting->get_string_representation)
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[256], val_d[256];

                                 *setting->value.target.fraction = i;

                                 setting->get_string_representation(setting,
                                       val_s, sizeof(val_s));
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, 0, 0);
                              }

                              *setting->value.target.fraction = orig_value;
                           }
                           else
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[16], val_d[16];

                                 snprintf(val_s, sizeof(val_s), "%.2f", i);
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);

                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, 0, 0);
                              }
                           }
                        }
                        break;
                     case ST_UINT:
                        {
                           float i;
                           unsigned orig_value    = *setting->value.target.unsigned_integer;
                           unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM;
                           float step             = setting->step;
                           double min             = setting->enforce_minrange ? setting->min : 0.00;
                           double max             = setting->enforce_maxrange ? setting->max : 999.00;

                           if (setting->get_string_representation)
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[256], val_d[256];
                                 int val = (int)i;

                                 *setting->value.target.unsigned_integer = val;

                                 setting->get_string_representation(setting,
                                       val_s, sizeof(val_s));
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0);
                              }

                              *setting->value.target.unsigned_integer = orig_value;
                           }
                           else
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[16], val_d[16];
                                 int val = (int)i;

                                 snprintf(val_s, sizeof(val_s), "%d", val);
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);

                                 menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0);
                              }
                           }
                        }
                        break;
                     default:
                        break;
                  }
               }
            }

            info->need_refresh       = true;
            info->need_push          = true;
         }
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
      }
      else
         filebrowser_parse(info, type);

      info->need_refresh = true;
      info->need_push    = true;
   }

   if (ret != 0)
      return false;

   return true;
}
