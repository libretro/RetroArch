/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Andrés Suárez
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <playlists/label_sanitization.h>
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
#include "../cheevos-new/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_http_parse.h>
#include "../../network/netplay/netplay.h"
#include "../network/netplay/netplay_discovery.h"
#endif

#ifdef HAVE_LAKKA_SWITCH
#include "../../lakka.h"
#endif

#ifdef HAVE_LIBNX
#include <switch.h>
#include "../../switch_performance_profiles.h"
#endif

#if defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../frontend/drivers/platform_unix.h"
#endif

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#include <media/media_detect_cd.h>
#endif

#include "menu_cbs.h"
#include "menu_content.h"
#include "menu_driver.h"
#include "menu_entries.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu_shader.h"
#endif
#include "menu_networking.h"
#include "widgets/menu_dialog.h"
#include "widgets/menu_filebrowser.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../defaults.h"
#include "../verbosity.h"
#include "../managers/cheat_manager.h"
#include "../managers/core_option_manager.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../core.h"
#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_display_server.h"
#include "../config.features.h"
#include "../version_git.h"
#include "../list_special.h"
#include "../performance_counters.h"
#include "../core_info.h"
#include "../wifi/wifi_driver.h"
#include "../tasks/tasks_internal.h"
#include "../dynamic.h"
#include "../runtime_file.h"

static char new_path_entry[4096]        = {0};
static char new_lbl_entry[4096]         = {0};
static char new_entry[4096]             = {0};
static enum msg_hash_enums new_type     = MSG_UNKNOWN;

#define menu_displaylist_parse_settings_enum(list, label, parse_type, add_empty_entry) menu_displaylist_parse_settings_internal_enum(list, parse_type, add_empty_entry, menu_setting_find_enum(label), label, true)

#define menu_displaylist_parse_settings(list, label, parse_type, add_empty_entry, entry_type) menu_displaylist_parse_settings_internal_enum(list, parse_type, add_empty_entry, menu_setting_find(label), entry_type, false)

/* Spacers used for '<content> - <core name>' labels
 * in playlists */
#define PL_LABEL_SPACER_DEFAULT "   |   "
#define PL_LABEL_SPACER_RGUI    " | "
#define PL_LABEL_SPACER_MAXLEN  8

#ifdef HAVE_NETWORKING
#if !defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(SWITCH) && defined(HAVE_LIBNX))
#include <net/net_ifinfo.h>
#endif
#endif

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

   {
      unsigned i;
      typedef struct menu_features_info
      {
         const char *name;
         enum msg_hash_enums msg;
      } menu_features_info_t;

      menu_features_info_t info_list[] = {
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER},
      };
      info_list[0].name = core_info->core_name;
      info_list[1].name = core_info->display_name;
      info_list[2].name = core_info->systemname;
      info_list[3].name = core_info->system_manufacturer;

      for (i = 0; i < ARRAY_SIZE(info_list); i++)
      {
         if (!info_list[i].name)
            continue;

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(info_list[i].msg),
               ": ",
               info_list[i].name,
               sizeof(tmp));
         menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0);
      }
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

   if (core_info->required_hw_api)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->required_hw_api_list, ", ");
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
            if (!core_info->firmware[i].desc)
               continue;

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

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#else
  if (settings->bools.menu_show_core_updater)
     menu_entries_append_enum(info->list,
           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE),
           msg_hash_to_str(MENU_ENUM_LABEL_CORE_DELETE),
           MENU_ENUM_LABEL_CORE_DELETE,
           MENU_SETTING_ACTION_CORE_DELETE, 0, 0);
#endif

   return 0;
}

#define BYTES_TO_MB(bytes) ((bytes) / 1024 / 1024)
#define BYTES_TO_GB(bytes) (((bytes) / 1024) / 1024 / 1024)

static unsigned menu_displaylist_parse_system_info(menu_displaylist_info_t *info)
{
   int controller;
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGLES) || defined(HAVE_OPENGL_CORE)
   gfx_ctx_ident_t ident_info;
#endif
   char tmp[8192];
#ifdef ANDROID
   bool perms                            = false;
#endif
   unsigned count                        = 0;
   const char *tmp_string                = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();
   settings_t *settings                  = config_get_ptr();

   tmp[0] = '\0';

   snprintf(tmp, sizeof(tmp), "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE), __DATE__);

   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

   (void)tmp_string;

#ifdef HAVE_GIT_VERSION
   fill_pathname_join_concat_noext(
         tmp,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION),
         ": ",
         retroarch_git_version,
         sizeof(tmp));
   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;
#endif

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

#ifdef ANDROID
   perms = test_permissions(internal_storage_path);

   snprintf(tmp, sizeof(tmp), "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS),
         perms ? "read-write" : "read-only");
   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

#endif
   {
      char cpu_str[255];
      const char *model = frontend_driver_get_cpu_model_name();

      cpu_str[0] = '\0';

      fill_pathname_noext(cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL),
            ": ",
            sizeof(cpu_str));

      if (string_is_empty(model))
         strlcat(cpu_str, "N/A", sizeof(cpu_str));
      else
         strlcat(cpu_str, model, sizeof(cpu_str));

      if (menu_entries_append_enum(info->list, cpu_str, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   {
      char cpu_str[255];

      cpu_str[0] = '\0';

      fill_pathname_noext(cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES),
            ": ",
            sizeof(cpu_str));

      retroarch_get_capabilities(RARCH_CAPABILITIES_CPU,
            cpu_str, sizeof(cpu_str));
      if (menu_entries_append_enum(info->list, cpu_str, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   {
      char cpu_str[8192];
      char cpu_arch_str[PATH_MAX_LENGTH];
      char cpu_text_str[PATH_MAX_LENGTH];

      cpu_str[0] = cpu_arch_str[0] = cpu_text_str[0] = '\0';

      strlcpy(cpu_text_str,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE),
            sizeof(cpu_text_str));

      rarch_get_cpu_architecture_string(cpu_arch_str, sizeof(cpu_arch_str));

      snprintf(cpu_str, sizeof(cpu_str), "%s %s", cpu_text_str, cpu_arch_str);

      if (menu_entries_append_enum(info->list, cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_CPU_ARCHITECTURE),
            MENU_ENUM_LABEL_CPU_ARCHITECTURE, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   {
      char cpu_str[PATH_MAX_LENGTH];
      unsigned         amount_cores = cpu_features_get_core_amount();

      cpu_str[0] = '\0';

      snprintf(cpu_str, sizeof(cpu_str),
            "%s %d\n", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_CORES), amount_cores);
      if (menu_entries_append_enum(info->list, cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_CPU_CORES),
            MENU_ENUM_LABEL_CPU_CORES, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   for (controller = 0; controller < MAX_USERS; controller++)
   {
      if (input_is_autoconfigured(controller))
      {
         snprintf(tmp, sizeof(tmp), "Port #%d device name: %s (#%d)",
            controller,
            input_config_get_device_name(controller),
            input_autoconfigure_get_device_name_index(controller));
         if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;

         if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         {
            snprintf(tmp, sizeof(tmp), " Device display name: %s",
               input_config_get_device_display_name(controller) ?
               input_config_get_device_display_name(controller) : "N/A");
            if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
            snprintf(tmp, sizeof(tmp), " Device config name: %s",
               input_config_get_device_display_name(controller) ?
               input_config_get_device_config_name(controller) : "N/A");
            if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
            snprintf(tmp, sizeof(tmp), " Device VID/PID: %d/%d",
               input_config_get_vid(controller),
               input_config_get_pid(controller));
            if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
         }
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
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;

#ifdef HAVE_LAKKA
      if (frontend->get_lakka_version)
      {
         frontend->get_lakka_version(tmp2, sizeof(tmp2));

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION),
               ": ",
               tmp2,
               sizeof(tmp));
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
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
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      if (frontend->get_os)
      {
         frontend->get_os(tmp2, sizeof(tmp2), &major, &minor);
         snprintf(tmp, sizeof(tmp), "%s : %s (v%d.%d)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS),
               tmp2,
               major, minor);
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      if (frontend->get_rating)
      {
         snprintf(tmp, sizeof(tmp), "%s : %d",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
               frontend->get_rating());
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      {
         char tmp[PATH_MAX_LENGTH];
         char tmp2[PATH_MAX_LENGTH];
         char tmp3[PATH_MAX_LENGTH];
         uint64_t memory_free       = frontend_driver_get_free_memory();
         uint64_t memory_total      = frontend_driver_get_total_memory();

         tmp[0] = tmp2[0] = tmp3[0] = '\0';

         if (memory_free != 0 && memory_total != 0)
         {
            snprintf(tmp, sizeof(tmp),
                  "%s %s: %" PRIu64 "/%" PRIu64 " MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  BYTES_TO_MB(memory_free),
                  BYTES_TO_MB(memory_total)
                  );

            if (menu_entries_append_enum(info->list, tmp, "",
                  MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
         }
      }

      if (frontend->get_powerstate)
      {
         int seconds    = 0, percent = 0;
         enum frontend_powerstate state =
            frontend->get_powerstate(&seconds, &percent);

         tmp2[0] = '\0';

         if (percent != 0)
            snprintf(tmp2, sizeof(tmp2), "%d%%", percent);

         switch (state)
         {
            case FRONTEND_POWERSTATE_NONE:
               strlcat(tmp2, " ", sizeof(tmp2));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), sizeof(tmp2));
               break;
            case FRONTEND_POWERSTATE_NO_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp2));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE),
                     sizeof(tmp2));
               strlcat(tmp2, ")", sizeof(tmp2));
               break;
            case FRONTEND_POWERSTATE_CHARGING:
               strlcat(tmp2, " (", sizeof(tmp2));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING),
                     sizeof(tmp2));
               strlcat(tmp2, ")", sizeof(tmp2));
               break;
            case FRONTEND_POWERSTATE_CHARGED:
               strlcat(tmp2, " (", sizeof(tmp2));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED),
                     sizeof(tmp2));
               strlcat(tmp2, ")", sizeof(tmp2));
               break;
            case FRONTEND_POWERSTATE_ON_POWER_SOURCE:
               strlcat(tmp2, " (", sizeof(tmp2));
               strlcat(tmp2,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING),
                     sizeof(tmp2));
               strlcat(tmp2, ")", sizeof(tmp2));
               break;
         }

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE),
               ": ",
               tmp2,
               sizeof(tmp));
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }
   }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGLES) || defined(HAVE_OPENGL_CORE)
   video_context_driver_get_ident(&ident_info);
   tmp_string = ident_info.ident;

   fill_pathname_join_concat_noext(tmp,
         msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER),
         ": ",
         tmp_string ? tmp_string
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
         sizeof(tmp));
   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

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
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      metrics.type  = DISPLAY_METRIC_MM_HEIGHT;

      if (video_context_driver_get_metrics(&metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT),
               val);
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      metrics.type  = DISPLAY_METRIC_DPI;

      if (video_context_driver_get_metrics(&metrics))
      {
         snprintf(tmp, sizeof(tmp), "%s: %.2f",
               msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI),
               val);
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }
   }
#endif

   {
      unsigned i;
      char feat_str[255];
      typedef struct menu_features_info
      {
         bool enabled;
         enum msg_hash_enums msg;
      } menu_features_info_t;

      menu_features_info_t info_list[] = {
         {SUPPORTS_LIBRETRODB, MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT},
         {SUPPORTS_OVERLAY,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT},
         {SUPPORTS_COMMAND,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT},
         {SUPPORTS_NETWORK_COMMAND,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT},
         {SUPPORTS_NETWORK_GAMEPAD,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT},
         {SUPPORTS_COCOA          ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT},
         {SUPPORTS_RPNG        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT},
         {SUPPORTS_RJPEG       ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT},
         {SUPPORTS_RBMP        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT},
         {SUPPORTS_RTGA        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT},
         {SUPPORTS_SDL         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT},
         {SUPPORTS_SDL2        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT},
         {SUPPORTS_VULKAN      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT},
         {SUPPORTS_METAL       ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT},
         {SUPPORTS_OPENGL      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT},
         {SUPPORTS_OPENGLES    ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT},
         {SUPPORTS_THREAD      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT},
         {SUPPORTS_KMS         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT},
         {SUPPORTS_UDEV        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT},
         {SUPPORTS_VG          ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT},
         {SUPPORTS_EGL         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT},
         {SUPPORTS_X11         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT},
         {SUPPORTS_WAYLAND     ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT},
         {SUPPORTS_XVIDEO      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT},
         {SUPPORTS_ALSA        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT},
         {SUPPORTS_OSS         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT},
         {SUPPORTS_AL          ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT},
         {SUPPORTS_SL          ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT},
         {SUPPORTS_RSOUND      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT},
         {SUPPORTS_ROAR        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT},
         {SUPPORTS_JACK        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT},
         {SUPPORTS_PULSE       ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT},
         {SUPPORTS_COREAUDIO   ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT},
         {SUPPORTS_COREAUDIO3  ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT},
         {SUPPORTS_DSOUND      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT},
         {SUPPORTS_WASAPI      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT},
         {SUPPORTS_XAUDIO      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT},
         {SUPPORTS_ZLIB        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT},
         {SUPPORTS_7ZIP        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT},
         {SUPPORTS_DYLIB       ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT},
         {SUPPORTS_DYNAMIC     ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT},
         {SUPPORTS_CG          ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT},
         {SUPPORTS_GLSL        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT},
         {SUPPORTS_HLSL        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT},
         {SUPPORTS_SDL_IMAGE   ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT},
         {SUPPORTS_FFMPEG      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT},
         {SUPPORTS_MPV         ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT},
         {SUPPORTS_CORETEXT    ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT},
         {SUPPORTS_FREETYPE    ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT},
         {SUPPORTS_STBFONT     ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT},
         {SUPPORTS_NETPLAY     ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT},
         {SUPPORTS_PYTHON      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT},
         {SUPPORTS_V4L2        ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT},
         {SUPPORTS_LIBUSB      ,    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT},
      };

      feat_str[0] = '\0';

      for (i = 0; i < ARRAY_SIZE(info_list); i++)
      {
         fill_pathname_join_concat_noext(feat_str,
               msg_hash_to_str(
                  info_list[i].msg),
               ": ",
               info_list[i].enabled ?
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_YES) :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO),
               sizeof(feat_str));
         if (menu_entries_append_enum(info->list, feat_str, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }
   }

   return count;
}

static int menu_displaylist_parse_playlist(menu_displaylist_info_t *info,
      playlist_t *playlist, const char *path_playlist, bool is_collection)
{
   unsigned i;
   char label_spacer[PL_LABEL_SPACER_MAXLEN];
   size_t           list_size        = playlist_size(playlist);
   settings_t       *settings        = config_get_ptr();
   bool show_inline_core_name        = false;
   void (*sanitization)(char*);

   label_spacer[0] = '\0';

   if (list_size == 0)
      goto error;

   /* Check whether core name should be added to playlist entries */
   if (!string_is_equal(settings->arrays.menu_driver, "ozone") &&
       !settings->bools.playlist_show_sublabels &&
       ((settings->uints.playlist_show_inline_core_name == PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS) ||
        (!is_collection && !(settings->uints.playlist_show_inline_core_name == PLAYLIST_INLINE_CORE_DISPLAY_NEVER))))
   {
      show_inline_core_name = true;

      /* Get spacer for menu entry labels (<content><spacer><core>)
       * > Note: Only required when showing inline core names */
      if (string_is_equal(settings->arrays.menu_driver, "rgui"))
         strlcpy(label_spacer, PL_LABEL_SPACER_RGUI, sizeof(label_spacer));
      else
         strlcpy(label_spacer, PL_LABEL_SPACER_DEFAULT, sizeof(label_spacer));
   }

   /* Inform menu driver of current system name
    * > Note: history, favorites and images_history
    *   require special treatment here, since info->path
    *   is nonsensical in these cases (and we *do* need
    *   to call set_thumbnail_system() in these cases,
    *   since all three playlist types have thumbnail
    *   support)
    * EDIT: For correct operation of the quick menu
    * 'download thumbnails' option, we must also extend
    * this to music_history and video_history */
   if (string_is_equal(path_playlist, "history") ||
       string_is_equal(path_playlist, "favorites") ||
       string_is_equal(path_playlist, "images_history") ||
       string_is_equal(path_playlist, "music_history") ||
       string_is_equal(path_playlist, "video_history"))
   {
      char system_name[15];
      system_name[0] = '\0';

      strlcpy(system_name, path_playlist, sizeof(system_name));
      menu_driver_set_thumbnail_system(system_name, sizeof(system_name));
   }
   else if (!string_is_empty(info->path))
   {
      char lpl_basename[PATH_MAX_LENGTH];
      lpl_basename[0] = '\0';

      fill_pathname_base_noext(lpl_basename, info->path, sizeof(lpl_basename));
      menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));
   }

   /* Preallocate the file list */
   file_list_reserve(info->list, list_size);

   switch (playlist_get_label_display_mode(playlist))
   {
      case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES :
         sanitization = &label_remove_parens;
         break;
      case LABEL_DISPLAY_MODE_REMOVE_BRACKETS :
         sanitization = &label_remove_brackets;
         break;
      case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES_AND_BRACKETS :
         sanitization = &label_remove_parens_and_brackets;
         break;
      case LABEL_DISPLAY_MODE_KEEP_DISC_INDEX :
         sanitization = &label_keep_disc;
         break;
      case LABEL_DISPLAY_MODE_KEEP_REGION :
         sanitization = &label_keep_region;
         break;
      case LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX :
         sanitization = &label_keep_region_and_disc;
         break;
      default :
         sanitization = NULL;
   }

   for (i = 0; i < list_size; i++)
   {
      char menu_entry_label[PATH_MAX_LENGTH];
      const struct playlist_entry *entry  = NULL;

      menu_entry_label[0] = '\0';

      /* Read playlist entry */
      playlist_get_index(playlist, i, &entry);

      if (!string_is_empty(entry->path))
      {
         /* Standard playlist entry
          * > Base menu entry label is always playlist label
          *   > If playlist label is NULL, fallback to playlist entry file name
          * > If required, add currently associated core (if any), otherwise
          *   no further action is necessary */

         if (string_is_empty(entry->label))
            fill_short_pathname_representation(menu_entry_label, entry->path, sizeof(menu_entry_label));
         else
            strlcpy(menu_entry_label, entry->label, sizeof(menu_entry_label));

         if (sanitization)
            (*sanitization)(menu_entry_label);

         if (show_inline_core_name)
         {
            if (!string_is_empty(entry->core_name) && !string_is_equal(entry->core_name, "DETECT"))
            {
               strlcat(menu_entry_label, label_spacer, sizeof(menu_entry_label));
               strlcat(menu_entry_label, entry->core_name, sizeof(menu_entry_label));
            }
         }

         menu_entries_append_enum(info->list, menu_entry_label, entry->path,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_RPL_ENTRY, 0, i);
      }
      else
      {
         if (entry->core_name)
            strlcpy(menu_entry_label, entry->core_name, sizeof(menu_entry_label));

         menu_entries_append_enum(info->list, menu_entry_label, path_playlist,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_PLAYLIST_ENTRY, 0, i);
      }

      info->count++;
   }

   return 0;

error:
   info->need_push_no_playlist_entries = true;
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
   size_t path_size                 = PATH_MAX_LENGTH * sizeof(char);

   if (!str_list)
      return -1;

   attr.i                           = 0;
   tmp                              = (char*)malloc(path_size);
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
   string_list_free(str_list);

   fill_pathname_join_concat_noext(tmp, desc, ": ",
         actual_string, path_size);
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   free(output_label);
   free(tmp);

   return 0;
}

static int create_string_list_rdb_entry_int(
      enum msg_hash_enums enum_idx,
      const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   union string_list_elem_attr attr;
   int str_len                      = 0;
   struct string_list *str_list     = string_list_new();
   char tmp[PATH_MAX_LENGTH];
   char str[PATH_MAX_LENGTH];
   char output_label[PATH_MAX_LENGTH];

   tmp[0]          = '\0';
   str[0]          = '\0';
   output_label[0] = '\0';

   if (!str_list)
      return -1;

   attr.i                           = 0;

   str_len                         += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   snprintf(str, sizeof(str), "%d", actual_int);
   str_len                         += strlen(str) + 1;
   string_list_append(str_list, str, attr);

   str_len                         += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   string_list_join_concat(output_label, str_len, str_list, "|");
   string_list_free(str_list);
   str_list = NULL;

   snprintf(tmp, sizeof(tmp), "%s : %d", desc, actual_int);
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   return 0;
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

   strlcat(path_base, ".lpl", sizeof(path_base));

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

      /* This allows thumbnails to be shown while viewing database
       * entries...
       * It only makes sense to do this for the first info entry,
       * since menu drivers cannot handle multiple successive
       * calls of menu_driver_set_thumbnail_content()...
       * Note that thumbnail updates must be disabled when using
       * RGUI, since this functionality is handled elsewhere
       * (and doing it here creates harmful conflicts) */
      if ((i == 0) && !string_is_equal(settings->arrays.menu_driver, "rgui"))
      {
         if (!string_is_empty(db_info_entry->name))
            strlcpy(thumbnail_content, db_info_entry->name,
                  sizeof(thumbnail_content));

         if (!string_is_empty(thumbnail_content))
            menu_driver_set_thumbnail_content(thumbnail_content,
                  sizeof(thumbnail_content));

         menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH, NULL);
         menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE, NULL);
      }

      if (playlist)
      {
         for (j = 0; j < playlist_size(playlist); j++)
         {
            const struct playlist_entry *entry  = NULL;
            bool match_found                 = false;
            struct string_list *tmp_str_list = NULL;

            playlist_get_index(playlist, j, &entry);

            if (entry->crc32)
                tmp_str_list = string_split(entry->crc32, "|");

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
#endif

static int menu_displaylist_parse_settings_internal_enum(
      file_list_t *info_list,
      enum menu_displaylist_parse_type parse_type,
      bool add_empty_entry,
      rarch_setting_t *setting,
      unsigned entry_type,
      bool is_enum
      )
{
   static enum setting_type precond_lut[] =
   {
      ST_END_GROUP,              /* PARSE_NONE                */
      ST_NONE,                   /* PARSE_GROUP               */
      ST_ACTION,                 /* PARSE_ACTION              */
      ST_INT,                    /* PARSE_ONLY_INT            */
      ST_UINT,                   /* PARSE_ONLY_UINT           */
      ST_BOOL,                   /* PARSE_ONLY_BOOL           */
      ST_FLOAT,                  /* PARSE_ONLY_FLOAT          */
      ST_BIND,                   /* PARSE_ONLY_BIND           */
      ST_END_GROUP,              /* PARSE_ONLY_GROUP          */
      ST_STRING,                 /* PARSE_ONLY_STRING         */
      ST_PATH,                   /* PARSE_ONLY_PATH           */
      ST_STRING_OPTIONS,         /* PARSE_ONLY_STRING_OPTIONS */
      ST_HEX,                    /* PARSE_ONLY_HEX            */
      ST_DIR,                    /* PARSE_ONLY_DIR            */
      ST_NONE,                   /* PARSE_SUB_GROUP           */
      ST_SIZE,                   /* PARSE_ONLY_SIZE           */
   };
   enum setting_type precond   = precond_lut[parse_type];
   size_t             count    = 0;
   settings_t *settings        = config_get_ptr();
   bool show_advanced_settings = settings->bools.menu_show_advanced_settings;

   if (!setting)
      return -1;

   if (!show_advanced_settings)
   {
      uint64_t flags = setting->flags;
      if (flags & SD_FLAG_ADVANCED)
         goto end;
#ifdef HAVE_LAKKA
      if (flags & SD_FLAG_LAKKA_ADVANCED)
         goto end;
#endif
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
         case PARSE_ONLY_INT:
         case PARSE_ONLY_UINT:
         case PARSE_ONLY_SIZE:
         case PARSE_ONLY_BIND:
         case PARSE_ONLY_BOOL:
         case PARSE_ONLY_FLOAT:
         case PARSE_ONLY_HEX:
         case PARSE_ONLY_STRING:
         case PARSE_ONLY_PATH:
         case PARSE_ONLY_DIR:
         case PARSE_ONLY_STRING_OPTIONS:
            if (type == precond)
               break;
            goto loop;
      }

      if (is_enum)
         menu_entries_append_enum(info_list,
               short_description, name,
               (enum msg_hash_enums)entry_type,
               menu_setting_set_flags(setting), 0, 0);
      else
      {
         if (
               (entry_type >= MENU_SETTINGS_INPUT_BEGIN) &&
               (entry_type < MENU_SETTINGS_INPUT_END)
            )
            entry_type = (unsigned)(MENU_SETTINGS_INPUT_BEGIN + count);
         if (entry_type == 0)
            entry_type = menu_setting_set_flags(setting);

         menu_entries_append(info_list, short_description,
               name, entry_type, 0, 0);
      }
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

end:
   if (count == 0)
   {
      if (add_empty_entry)
         menu_entries_append_enum(info_list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
               MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
               0, 0, 0);
      return -1;
   }

   return 0;
}

static void menu_displaylist_set_new_playlist(
      menu_handle_t *menu, const char *path)
{
   unsigned playlist_size         = COLLECTION_SIZE;
   const char *playlist_file_name = path_basename(path);
   settings_t *settings           = config_get_ptr();

   menu->db_playlist_file[0]      = '\0';

   if (playlist_get_cached())
      playlist_free_cached();

   /* Get proper playlist capacity */
   if (settings && !string_is_empty(playlist_file_name))
   {
      if (string_is_equal(playlist_file_name, file_path_str(FILE_PATH_CONTENT_HISTORY)) ||
          string_is_equal(playlist_file_name, file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY)) ||
          string_is_equal(playlist_file_name, file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY)) ||
          string_is_equal(playlist_file_name, file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY)))
         playlist_size = settings->uints.content_history_size;
      else if (string_is_equal(playlist_file_name, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
         if (settings->ints.content_favorites_size >= 0)
            playlist_size = (unsigned)settings->ints.content_favorites_size;
   }

   if (playlist_init_cached(path, playlist_size))
      strlcpy(
            menu->db_playlist_file,
            path,
            sizeof(menu->db_playlist_file));
}

static int menu_displaylist_parse_horizontal_list(
      menu_handle_t *menu,
      menu_displaylist_info_t *info,
      bool sort)
{
   menu_ctx_list_t list_info;
   menu_ctx_list_t list_horiz_info;
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

      fill_pathname_join(
            path_playlist,
            settings->paths.directory_playlist,
            item->path,
            sizeof(path_playlist));
      menu_displaylist_set_new_playlist(menu, path_playlist);

      /* Thumbnail system must be set *after* playlist
       * is loaded/cached */
      fill_pathname_base_noext(lpl_basename, item->path, sizeof(lpl_basename));
      menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));
   }

   playlist = playlist_get_cached();

   if (playlist)
   {
      if (sort)
         playlist_qsort(playlist);

      menu_displaylist_parse_playlist(info,
            playlist,
            msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION), true);
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

      if (settings->bools.quick_menu_show_resume_content)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT),
               MENU_ENUM_LABEL_RESUME_CONTENT,
               MENU_SETTING_ACTION_RUN, 0, 0);

      if (settings->bools.quick_menu_show_restart_content)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT),
               MENU_ENUM_LABEL_RESTART_CONTENT,
               MENU_SETTING_ACTION_RUN, 0, 0);

      if (settings->bools.quick_menu_show_close_content)
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

      if (settings->bools.quick_menu_show_save_load_state)
      {
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_STATE_SLOT, PARSE_ONLY_INT, true);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE),
               MENU_ENUM_LABEL_SAVE_STATE,
               MENU_SETTING_ACTION_SAVESTATE, 0, 0);
#ifdef HAVE_CHEEVOS
         if (!rcheevos_hardcore_active)
#endif
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_STATE),
                  msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE),
                  MENU_ENUM_LABEL_LOAD_STATE,
                  MENU_SETTING_ACTION_LOADSTATE, 0, 0);
         }
      }

      if (settings->bools.quick_menu_show_save_load_state &&
          settings->bools.quick_menu_show_undo_save_load_state)
      {
#ifdef HAVE_CHEEVOS
         if (!rcheevos_hardcore_active)
#endif
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE),
                  msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE),
                  MENU_ENUM_LABEL_UNDO_LOAD_STATE,
                  MENU_SETTING_ACTION_LOADSTATE, 0, 0);
         }

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
            if (settings->bools.quick_menu_show_start_recording && !settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING),
                     MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING, MENU_SETTING_ACTION, 0, 0);
            }

            if (settings->bools.quick_menu_show_start_streaming && !settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING),
                     MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING, MENU_SETTING_ACTION, 0, 0);
            }
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

#ifdef HAVE_VIDEO_LAYOUT
      if (settings->bools.menu_show_video_layout && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS),
               MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
               MENU_SETTING_ACTION, 0, 0);
      }
#endif

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

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
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
#endif

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
      if (settings->bools.cheevos_enable)
      {
         menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST),
            MENU_ENUM_LABEL_ACHIEVEMENT_LIST,
            MENU_SETTING_ACTION, 0, 0);
      }
#endif

      if (settings->bools.quick_menu_show_information)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INFORMATION),
               msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION),
               MENU_ENUM_LABEL_INFORMATION,
               MENU_SETTING_ACTION, 0, 0);
      }
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
   playlist_t *playlist            = playlist_get_cached();
   settings_t *settings            = config_get_ptr();
   const char *fullpath            = path_get(RARCH_PATH_CONTENT);
   unsigned idx                    = menu->rpl_entry_selection_ptr;
   const struct playlist_entry *entry  = NULL;

   if (playlist)
      playlist_get_index(playlist, idx, &entry);

   content_loaded = !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, fullpath);

   if (content_loaded)
      menu_displaylist_parse_load_content_settings(menu, info);
   else
   {
#ifdef HAVE_AUDIOMIXER
      const char *ext = NULL;

      if (entry && !string_is_empty(entry->path))
         ext = path_get_extension(entry->path);

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
#endif

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

      if (!settings->bools.kiosk_mode_enable)
      {
         bool remove_entry_enabled = false;

         if (settings->uints.playlist_entry_remove_enable == PLAYLIST_ENTRY_REMOVE_ENABLE_ALL)
            remove_entry_enabled = true;
         else if (settings->uints.playlist_entry_remove_enable == PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV)
         {
            char system[PATH_MAX_LENGTH];
            system[0] = '\0';

            menu_driver_get_thumbnail_system(system, sizeof(system));

            if (!string_is_empty(system))
               remove_entry_enabled = string_is_equal(system, "history") ||
                                      string_is_equal(system, "favorites") ||
                                      string_is_equal(system, "images_history") ||
                                      string_is_equal(system, "music_history") ||
                                      string_is_equal(system, "video_history");

            /* An annoyance: if the user navigates to the information menu,
             * then to the database entry, the thumbnail system will be changed.
             * This breaks the above 'remove_entry_enabled' check for the
             * history and favorites playlists. We therefore have to check
             * the playlist file name as well... */
            if (!remove_entry_enabled && settings->bools.quick_menu_show_information)
            {
               const char *playlist_path = playlist_get_conf_path(playlist);

               if (!string_is_empty(playlist_path))
               {
                  const char *playlist_file = path_basename(playlist_path);

                  if (!string_is_empty(playlist_file))
                     remove_entry_enabled = string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)) ||
                                            string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES));
               }
            }
         }

         if (remove_entry_enabled)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY),
                  msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY),
                  MENU_ENUM_LABEL_DELETE_ENTRY,
                  MENU_SETTING_ACTION_DELETE_ENTRY, 0, 0);
      }

      if (settings->bools.quick_menu_show_add_to_favorites)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST),
               MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (settings->bools.quick_menu_show_set_core_association && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION),
               msg_hash_to_str(MENU_ENUM_LABEL_SET_CORE_ASSOCIATION),
               MENU_ENUM_LABEL_SET_CORE_ASSOCIATION, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (settings->bools.quick_menu_show_reset_core_association && !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION),
               msg_hash_to_str(MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION),
               MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (settings->bools.quick_menu_show_information)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INFORMATION),
               msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION),
               MENU_ENUM_LABEL_INFORMATION, MENU_SETTING_ACTION, 0, 0);
      }
   }

#ifdef HAVE_NETWORKING
   if (settings->bools.quick_menu_show_download_thumbnails && !settings->bools.kiosk_mode_enable)
   {
      bool download_enabled = true;

      /* If content is currently running, have to make sure
       * we have a valid playlist to work with */
      if (content_loaded)
      {
         const char *core_path = path_get(RARCH_PATH_CORE);

         download_enabled = false;
         if (!string_is_empty(fullpath) && !string_is_empty(core_path))
            download_enabled = playlist_index_is_valid(
                  playlist, idx, fullpath, core_path);
      }

      if (download_enabled)
      {
         char system[PATH_MAX_LENGTH];

         system[0] = '\0';

         /* Only show 'download thumbnails' on supported playlists */
         download_enabled = false;
         menu_driver_get_thumbnail_system(system, sizeof(system));

         if (!string_is_empty(system))
            download_enabled = !string_is_equal(system, "images_history") &&
                               !string_is_equal(system, "music_history") &&
                               !string_is_equal(system, "video_history");
      }

      if (download_enabled)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS),
               msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS),
               MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
   }
#endif

   return 0;
}

static unsigned menu_displaylist_parse_information_list(
      menu_displaylist_info_t *info)
{
   unsigned count                   = 0;
   core_info_t   *core_info         = NULL;
   struct retro_system_info *system = runloop_get_libretro_system_info();

   core_info_get_current_core(&core_info);

   if (  system &&
         (!string_is_empty(system->library_name) &&
          !string_is_equal(system->library_name,
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
         )
         && core_info && core_info->config_data
      )
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_INFORMATION),
            MENU_ENUM_LABEL_CORE_INFORMATION,
            MENU_SETTING_ACTION, 0, 0))
         count++;

#ifdef HAVE_CDROM
   {
      struct string_list *drive_list = cdrom_get_available_drives();

      if (drive_list)
      {
         if (drive_list->size)
         {
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISC_INFORMATION),
                  msg_hash_to_str(MENU_ENUM_LABEL_DISC_INFORMATION),
                  MENU_ENUM_LABEL_DISC_INFORMATION,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
         }

         string_list_free(drive_list);
      }
   }
#endif

#ifdef HAVE_NETWORKING
#ifndef HAVE_SOCKET_LEGACY
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION),
         MENU_ENUM_LABEL_NETWORK_INFORMATION,
         MENU_SETTING_ACTION, 0, 0))
      count++;
#endif
#endif

   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_INFORMATION),
         MENU_ENUM_LABEL_SYSTEM_INFORMATION,
         MENU_SETTING_ACTION, 0, 0))
      count++;

#ifdef HAVE_LIBRETRODB
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST),
         MENU_ENUM_LABEL_DATABASE_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0))
      count++;
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST),
         MENU_ENUM_LABEL_CURSOR_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0))
      count++;
#endif

   if (rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
   {
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_FRONTEND_COUNTERS),
            MENU_ENUM_LABEL_FRONTEND_COUNTERS,
            MENU_SETTING_ACTION, 0, 0))
         count++;

      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_COUNTERS),
            MENU_ENUM_LABEL_CORE_COUNTERS,
            MENU_SETTING_ACTION, 0, 0))
         count++;
   }

   return count;
}

static unsigned menu_displaylist_parse_playlists(
      menu_displaylist_info_t *info, bool horizontal)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;
   unsigned count               = 0;
   settings_t *settings         = config_get_ptr();
   const char *path             = info->path;

   if (string_is_empty(path))
   {
      int ret = frontend_driver_parse_drive_list(info->list, true);
      /* TODO/FIXME - we need to know the actual count number here */
      if (ret == 0)
         count++;
      else
         if (menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0))
            count++;
      return count;
   }

   if (!horizontal)
   {
#ifdef HAVE_LIBRETRODB
      if (settings->bools.menu_content_show_add)
      {
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
               MENU_ENUM_LABEL_SCAN_DIRECTORY,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
               MENU_ENUM_LABEL_SCAN_FILE,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }
#endif
     if (settings->bools.menu_content_show_favorites)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
            MENU_ENUM_LABEL_GOTO_FAVORITES,
            MENU_SETTING_ACTION, 0, 0))
         count++;
     if (settings->bools.menu_content_show_images)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
            MENU_ENUM_LABEL_GOTO_IMAGES,
            MENU_SETTING_ACTION, 0, 0))
         count++;

     if (settings->bools.menu_content_show_music)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
            MENU_ENUM_LABEL_GOTO_MUSIC,
            MENU_SETTING_ACTION, 0, 0))
         count++;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
     if (settings->bools.menu_content_show_video)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
            msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
            MENU_ENUM_LABEL_GOTO_VIDEO,
            MENU_SETTING_ACTION, 0, 0))
         count++;
#endif
   }

   str_list = dir_list_new(path, NULL, true,
         settings->bools.show_hidden_files, true, false);

   if (!str_list)
      return count;

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   for (i = 0; i < list_size; i++)
   {
      const char *path             = str_list->elems[i].data;
      const char *playlist_file    = NULL;
      enum msg_file_type file_type = FILE_TYPE_NONE;

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

      if (string_is_empty(path))
         continue;

      playlist_file = path_basename(path);

      if (string_is_empty(playlist_file))
         continue;

      /* Ignore non-playlist files */
      if (!string_is_equal_noncase(path_get_extension(playlist_file),
               "lpl"))
         continue;

      /* Ignore history/favourites */
      if (string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)) ||
          string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY)) ||
          string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY)) ||
          string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY)) ||
          string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
         continue;

      file_type = FILE_TYPE_PLAYLIST_COLLECTION;

      if (horizontal)
         path = playlist_file;

      if (menu_entries_append_enum(info->list, path, "",
            MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY,
            file_type, 0, 0))
         count++;
   }

   string_list_free(str_list);

   return count;
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
   bool ok;

   if (string_is_empty(path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MSG_UNKNOWN, FILE_TYPE_DIRECTORY, 0, 0);
      items_found++;
      return items_found;
   }

   str_list = string_list_new();
   ok = dir_list_append(str_list, path, info->exts,
         true, settings->bools.show_hidden_files, false, false);

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   /* UWP: browse the optional packages for additional cores */
   struct string_list *core_packages = string_list_new();
   uwp_fill_installed_core_packages(core_packages);
   for (i = 0; i < core_packages->size; i++)
      dir_list_append(str_list, core_packages->elems[i].data, info->exts,
            true, settings->bools.show_hidden_files, true, false);

   string_list_free(core_packages);
#else
   /* Keep the old 'directory not found' behavior */
   if (!ok)
   {
      string_list_free(str_list);
      str_list = NULL;
   }
#endif

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

   if (string_is_equal(info->label, msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
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
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            /* Compressed cores are unsupported */
            continue;
         case RARCH_PLAIN_FILE:
         default:
            file_type = FILE_TYPE_CORE;
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

      if (is_dir)
      {
         file_type = FILE_TYPE_DIRECTORY;
         enum_idx  = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
      }
      else
      {
         file_type = FILE_TYPE_CORE;
         if (string_is_equal(info->label, msg_hash_to_str(MENU_ENUM_LABEL_SIDELOAD_CORE_LIST)))
            enum_idx  = MENU_ENUM_LABEL_FILE_BROWSER_SIDELOAD_CORE;
         else
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
      core_info_list_t *list         = NULL;
      const char *dir                = NULL;

      core_info_get_list(&list);

      menu_entries_get_last_stack(&dir, NULL, NULL, NULL, NULL);

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
            size_t path_size   = PATH_MAX_LENGTH * sizeof(char);
            char *core_path    = (char*)malloc(path_size);
            char *display_name = (char*)malloc(path_size);
            core_path[0]       =
            display_name[0]    = '\0';

            fill_pathname_join(core_path, dir, path, path_size);

            if (core_info_list_get_display_name(list,
                     core_path, display_name, path_size))
               file_list_set_alt_at_offset(info->list, i, display_name);

            free(core_path);
            free(display_name);
         }
      }
      info->need_sort = true;
   }

   return items_found;
}

static unsigned menu_displaylist_parse_playlist_manager_list(
      menu_displaylist_info_t *info)
{
   settings_t      *settings    = config_get_ptr();
   unsigned count               = 0;
   struct string_list *str_list = NULL;

   if (!settings)
      return count;

   /* Add collection playlists */
   str_list = dir_list_new_special(
         settings->paths.directory_playlist,
         DIR_LIST_COLLECTIONS, NULL);

   if (str_list && str_list->size)
   {
      unsigned i;

      dir_list_sort(str_list, true);

      for (i = 0; i < str_list->size; i++)
      {
         const char *path          = str_list->elems[i].data;
         const char *playlist_file = NULL;

         if (str_list->elems[i].attr.i == FILE_TYPE_DIRECTORY)
            continue;

         if (string_is_empty(path))
            continue;

         playlist_file = path_basename(path);

         if (string_is_empty(playlist_file))
            continue;

         /* Ignore non-playlist files */
         if (!string_is_equal_noncase(path_get_extension(playlist_file),
                  "lpl"))
            continue;

         /* Ignore history/favourites
          * > content_history + favorites are handled separately
          * > music/video/image_history are ignored */
         if (string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)) ||
             string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY)) ||
             string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY)) ||
             string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY)) ||
             string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
            continue;

         menu_entries_append_enum(info->list,
               path,
               "",
               MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS,
               MENU_SETTING_ACTION,
               0, 0);
         count++;
      }
   }

   /* Not necessary to check for NULL here */
   string_list_free(str_list);

   /* Add content history */
   if (settings->bools.history_list_enable)
      if (g_defaults.content_history)
         if (playlist_size(g_defaults.content_history) > 0)
            if (menu_entries_append_enum(info->list,
                  playlist_get_conf_path(g_defaults.content_history),
                  "",
                  MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS,
                  MENU_SETTING_ACTION,
                  0, 0))
               count++;

   /* Add favourites */
   if (g_defaults.content_favorites)
      if (playlist_size(g_defaults.content_favorites) > 0)
         if (menu_entries_append_enum(info->list,
               playlist_get_conf_path(g_defaults.content_favorites),
               "",
               MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS,
               MENU_SETTING_ACTION,
               0, 0))
            count++;

   return count;
}

static bool menu_displaylist_parse_playlist_manager_settings(
      menu_handle_t *menu,
      menu_displaylist_info_t *info,
      const char *playlist_path)
{
   enum msg_hash_enums right_thumbnail_label_value;
   enum msg_hash_enums left_thumbnail_label_value;
   settings_t *settings      = config_get_ptr();
   const char *playlist_file = NULL;
   playlist_t *playlist      = NULL;

   if (!settings)
      return false;

   if (string_is_empty(playlist_path))
      return false;

   playlist_file = path_basename(playlist_path);

   if (string_is_empty(playlist_file))
      return false;

   menu_displaylist_set_new_playlist(menu, playlist_path);

   playlist = playlist_get_cached();

   if (!playlist)
      return false;

   /* Default core association
    * > This is only shown for collection playlists
    *   (i.e. it is not relevant for history/favourites) */
   if (!string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_HISTORY)) &&
       !string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY)) &&
       !string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY)) &&
       !string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY)) &&
       !string_is_equal(playlist_file, file_path_str(FILE_PATH_CONTENT_FAVORITES)))
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE),
            MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
            MENU_SETTING_PLAYLIST_MANAGER_DEFAULT_CORE, 0, 0);

   /* Reset core associations */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES,
         FILE_TYPE_PLAYLIST_ENTRY, 0, 0);

   /* Label display mode */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
         MENU_SETTING_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE, 0, 0);

   /* Thumbnail modes */

   /* > Get label values */
   if (string_is_equal(settings->arrays.menu_driver, "rgui"))
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI;
      left_thumbnail_label_value =  MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI;
   }
   else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
      left_thumbnail_label_value =  MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE;
   }
   else
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
      left_thumbnail_label_value =  MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS;
   }

   /* > Right thumbnail mode */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(right_thumbnail_label_value),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE,
         MENU_SETTING_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE, 0, 0);

   /* > Left thumbnail mode */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(left_thumbnail_label_value),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE,
         MENU_SETTING_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE, 0, 0);

   /* TODO - Add:
    * - Remove invalid entries */

   return true;
}

#ifdef HAVE_NETWORKING
static unsigned menu_displaylist_parse_pl_thumbnail_download_list(
      menu_displaylist_info_t *info)
{
   settings_t      *settings    = config_get_ptr();
   unsigned count               = 0;
   struct string_list *str_list = NULL;

   if (!settings)
      return count;

   str_list = dir_list_new_special(
         settings->paths.directory_playlist,
         DIR_LIST_COLLECTIONS, NULL);

   if (str_list && str_list->size)
   {
      unsigned i;

      dir_list_sort(str_list, true);

      for (i = 0; i < str_list->size; i++)
      {
         char path_base[PATH_MAX_LENGTH];
         const char *path                 =
            path_basename(str_list->elems[i].data);

         path_base[0] = '\0';

         if (str_list->elems[i].attr.i == FILE_TYPE_DIRECTORY)
            continue;

         if (string_is_empty(path))
            continue;

         if (!string_is_equal_noncase(path_get_extension(path),
                  "lpl"))
            continue;

         strlcpy(path_base, path, sizeof(path_base));
         path_remove_extension(path_base);

         menu_entries_append_enum(info->list,
               path_base,
               path,
               MENU_ENUM_LABEL_PLAYLIST_ENTRY,
               FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT,
               0, 0);
         count++;
      }
   }

   /* Not necessary to check for NULL here */
   string_list_free(str_list);

   return count;
}
#endif

static unsigned menu_displaylist_parse_content_information(
      menu_handle_t *menu,
      menu_displaylist_info_t *info)
{
   settings_t *settings                = config_get_ptr();
   playlist_t *playlist                = playlist_get_cached();
   unsigned idx                        = menu->rpl_entry_selection_ptr;
   const struct playlist_entry *entry  = NULL;
   const char *loaded_content_path     = path_get(RARCH_PATH_CONTENT);
   const char *loaded_core_path        = path_get(RARCH_PATH_CORE);
   const char *content_label           = NULL;
   const char *content_path            = NULL;
   const char *core_path               = NULL;
   const char *db_name                 = NULL;
   bool content_loaded                 = false;
   bool playlist_valid                 = false;
   unsigned count                      = 0;
   int n                               = 0;
   char core_name[PATH_MAX_LENGTH];
   char tmp[8192];

   core_name[0] = '\0';

   if (!settings)
      return count;

   content_loaded = !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, loaded_content_path);

   /* If content is currently running, have to make sure
    * we have a valid playlist to work with
    * (if content is not running, than playlist will always
    * be valid provided that playlist_get_cached() does not
    * return NULL) */
   if (content_loaded)
   {
      if (!string_is_empty(loaded_content_path) && !string_is_empty(loaded_core_path))
         playlist_valid = playlist_index_is_valid(
               playlist, idx, loaded_content_path, loaded_core_path);
   }
   else if (playlist)
      playlist_valid = true;

   if (playlist_valid)
   {
      /* If playlist is valid, all information is readily available */
      playlist_get_index(playlist, idx, &entry);

      if (entry)
      {
         content_label = entry->label;
         content_path  = entry->path;
         core_path     = entry->core_path;
         db_name       = entry->db_name;

         strlcpy(core_name, entry->core_name, sizeof(core_name));
      }
   }
   else
   {
      core_info_ctx_find_t core_info;

      /* No playlist - just extract what we can... */
      content_path   = loaded_content_path;
      core_path      = loaded_core_path;

      core_info.inf  = NULL;
      core_info.path = core_path;

      if (core_info_find(&core_info, core_path))
         if (!string_is_empty(core_info.inf->display_name))
            strlcpy(core_name, core_info.inf->display_name, sizeof(core_name));
   }

   /* Content label */
   if (!string_is_empty(content_label))
   {
      tmp[0]   = '\0';

      n        = strlcpy(tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL), sizeof(tmp));
      strlcat(tmp, ": ", sizeof(tmp));
      n        = strlcat(tmp, content_label, sizeof(tmp));

      /* Silence gcc compiler warning
       * (getting so sick of these...) */
      if ((n < 0) || (n >= PATH_MAX_LENGTH))
         n = 0;

      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_LABEL),
            MENU_ENUM_LABEL_CONTENT_INFO_LABEL,
            0, 0, 0))
         count++;
   }

   /* Content path */
   if (!string_is_empty(content_path))
   {
      tmp[0]   = '\0';

      n        = strlcpy(tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH), sizeof(tmp));
      n        = strlcat(tmp, ": ", sizeof(tmp));
      n        = strlcat(tmp, content_path, sizeof(tmp));

      /* Silence gcc compiler warning
       * (getting so sick of these...) */
      if ((n < 0) || (n >= PATH_MAX_LENGTH))
         n = 0;

      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_PATH),
            MENU_ENUM_LABEL_CONTENT_INFO_PATH,
            0, 0, 0))
         count++;
   }

   /* Core name */
   if (!string_is_empty(core_name) &&
       !string_is_equal(core_name, "DETECT"))
   {
      tmp[0]   = '\0';

      n        = strlcpy(tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME), sizeof(tmp));
      n        = strlcat(tmp, ": ", sizeof(tmp));
      n        = strlcat(tmp, core_name, sizeof(tmp));

      /* Silence gcc compiler warning
       * (getting so sick of these...) */
      if ((n < 0) || (n >= PATH_MAX_LENGTH))
         n = 0;

      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_CORE_NAME),
            MENU_ENUM_LABEL_CONTENT_INFO_CORE_NAME,
            0, 0, 0))
         count++;
   }

   /* Database */
   if (!string_is_empty(db_name))
   {
      char *db_name_no_ext = NULL;
      char db_name_no_ext_buff[PATH_MAX_LENGTH];

      db_name_no_ext_buff[0] = '\0';

      /* Remove .lpl extension
       * > path_remove_extension() requires a char * (not const)
       *   so have to use a temporary buffer... */
      strlcpy(db_name_no_ext_buff, db_name, sizeof(db_name_no_ext_buff));
      db_name_no_ext = path_remove_extension(db_name_no_ext_buff);

      if (!string_is_empty(db_name_no_ext))
      {
         tmp[0]   = '\0';

         n        = strlcpy(tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE), sizeof(tmp));
         n        = strlcat(tmp, ": ", sizeof(tmp));
         n        = strlcat(tmp, db_name_no_ext, sizeof(tmp));

         /* Silence gcc compiler warning
          * (getting so sick of these...) */
         if ((n < 0) || (n >= PATH_MAX_LENGTH))
            n = 0;

         if (menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_DATABASE),
               MENU_ENUM_LABEL_CONTENT_INFO_DATABASE,
               0, 0, 0))
            count++;
      }
   }

   /* Runtime */
   if (((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE) &&
         settings->bools.content_runtime_log) ||
       ((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_AGGREGATE) &&
         !settings->bools.content_runtime_log_aggregate))
   {
      runtime_log_t *runtime_log = runtime_log_init(
            content_path, core_path,
            (settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE));

      if (runtime_log)
      {
         if (runtime_log_has_runtime(runtime_log))
         {
            /* Play time */
            tmp[0] = '\0';
            runtime_log_get_runtime_str(runtime_log, tmp, sizeof(tmp));

            if (!string_is_empty(tmp))
               if (menu_entries_append_enum(info->list, tmp,
                     msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_RUNTIME),
                     MENU_ENUM_LABEL_CONTENT_INFO_RUNTIME,
                     0, 0, 0))
                  count++;

            /* Last Played */
            tmp[0] = '\0';
            runtime_log_get_last_played_str(runtime_log, tmp, sizeof(tmp),
                  (enum playlist_sublabel_last_played_style_type)settings->uints.playlist_sublabel_last_played_style);

            if (!string_is_empty(tmp))
               if (menu_entries_append_enum(info->list, tmp,
                     msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_LAST_PLAYED),
                     MENU_ENUM_LABEL_CONTENT_INFO_LAST_PLAYED,
                     0, 0, 0))
                  count++;
         }

         free(runtime_log);
      }
   }

#ifdef HAVE_LIBRETRODB

   /* Database entry */
   if (!string_is_empty(content_label) && !string_is_empty(db_name))
   {
      char db_path[PATH_MAX_LENGTH];

      db_path[0] = '\0';

      fill_pathname_join_noext(db_path,
            settings->paths.path_content_database,
            db_name,
            sizeof(db_path));
      strlcat(db_path, ".rdb", sizeof(db_path));

      if (path_is_valid(db_path))
         if (menu_entries_append_enum(info->list,
               content_label,
               db_path,
               MENU_ENUM_LABEL_RDB_ENTRY_DETAIL,
               FILE_TYPE_RDB_ENTRY, 0, 0))
            count++;
   }

#endif

   return count;
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

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

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

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

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

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

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

      info->exts  = strdup("lpl");
      info->label = strdup(
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));

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
      bool is_collection,
      bool sort,
      int *ret)
{
   playlist_t *playlist = NULL;

   menu_displaylist_set_new_playlist(menu, playlist_path);

   playlist             = playlist_get_cached();

   if (!playlist)
      return;

   if (sort)
      playlist_qsort(playlist);

   *ret              = menu_displaylist_parse_playlist(info,
         playlist, playlist_name, is_collection);
}

#ifdef HAVE_NETWORKING
static void wifi_scan_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   unsigned i;
   file_list_t *file_list        = NULL;
   struct string_list *ssid_list = NULL;

   const char *path              = NULL;
   const char *label             = NULL;
   unsigned menu_type            = 0;

   menu_entries_get_last_stack(&path, &label, &menu_type, NULL, NULL);

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
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#else
   if (settings->bools.menu_show_core_updater && !settings->bools.kiosk_mode_enable)
   {
      if (info->download_core)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
               MENU_ENUM_LABEL_CORE_UPDATER_LIST,
               MENU_SETTING_ACTION, 0, 0);

         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_SIDELOAD_CORE_LIST),
               MENU_ENUM_LABEL_SIDELOAD_CORE_LIST,
               MENU_SETTING_ACTION, 0, 0);
      }
   }
#endif
#endif

   if (info->push_builtin_cores)
   {
#if defined(HAVE_VIDEOPROCESSOR)
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

bool menu_displaylist_setting(menu_displaylist_ctx_parse_entry_t *entry)
{
   if (menu_displaylist_parse_settings_enum(
            entry->info->list,
            entry->enum_idx,
            entry->parse_type,
            entry->add_empty_entry) == -1)
      return false;
   return true;
}

typedef struct menu_displaylist_build_info {
   enum msg_hash_enums enum_idx;
   enum menu_displaylist_parse_type parse_type;
} menu_displaylist_build_info_t;

typedef struct menu_displaylist_build_info_selective {
   enum msg_hash_enums enum_idx;
   enum menu_displaylist_parse_type parse_type;
   bool checked;
} menu_displaylist_build_info_selective_t;

static unsigned populate_playlist_thumbnail_mode_dropdown_list(
      file_list_t *list, enum playlist_thumbnail_id thumbnail_id)
{
   unsigned count       = 0;
   playlist_t *playlist = playlist_get_cached();

   if (list && playlist)
   {
      size_t i;
      /* Get currently selected thumbnail mode */
      enum playlist_thumbnail_mode current_thumbnail_mode =
            playlist_get_thumbnail_mode(playlist, thumbnail_id);
      /* Get appropriate menu_settings_type (right/left) */
      enum menu_settings_type settings_type =
            (thumbnail_id == PLAYLIST_THUMBNAIL_RIGHT) ?
                  MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_RIGHT_THUMBNAIL_MODE :
                  MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LEFT_THUMBNAIL_MODE;

      /* Loop over all thumbnail modes */
      for (i = 0; i <= (unsigned)PLAYLIST_THUMBNAIL_MODE_BOXARTS; i++)
      {
         enum msg_hash_enums label_value;
         enum playlist_thumbnail_mode thumbnail_mode =
               (enum playlist_thumbnail_mode)i;

         /* Get appropriate entry label */
         switch (thumbnail_mode)
         {
            case PLAYLIST_THUMBNAIL_MODE_OFF:
               label_value = MENU_ENUM_LABEL_VALUE_OFF;
               break;
            case PLAYLIST_THUMBNAIL_MODE_SCREENSHOTS:
               label_value = MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS;
               break;
            case PLAYLIST_THUMBNAIL_MODE_TITLE_SCREENS:
               label_value = MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS;
               break;
            case PLAYLIST_THUMBNAIL_MODE_BOXARTS:
               label_value = MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS;
               break;
            default:
               /* PLAYLIST_THUMBNAIL_MODE_DEFAULT */
               label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT;
               break;
         }

         /* Add entry */
         if (menu_entries_append_enum(list,
               msg_hash_to_str(label_value),
               "",
               MENU_ENUM_LABEL_NO_ITEMS,
               settings_type,
               0, 0))
            count++;

         /* Add checkmark if item is currently selected */
         if (current_thumbnail_mode == thumbnail_mode)
            menu_entries_set_checked(list, i, true);
      }
   }

   return count;
}

unsigned menu_displaylist_build_list(file_list_t *list, enum menu_displaylist_ctl_state type)
{
   unsigned i;
   unsigned count = 0;

   switch (type)
   {
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_MAX_USERS,
                  PARSE_ONLY_UINT, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_QUIT_PRESS_TWICE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_VIBRATE_ON_KEYPRESS,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_ENABLE_DEVICE_VIBRATION,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_ICADE_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
#if defined(GEKKO)
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_MOUSE_SCALE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
#endif
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_ANALOG_DEADZONE,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_ANALOG_SENSITIVITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_BIND_HOLD,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_TURBO_PERIOD,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_DUTY_CYCLE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_BIND_MODE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,
                  PARSE_ACTION, false) == 0)
            count++;

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

               if (menu_displaylist_parse_settings(list,
                        key_split_joycon, PARSE_ONLY_UINT, true, 0) != -1)
                  count++;
            }
         }
#endif

         {
            unsigned user;
            unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
            for (user = 0; user < max_users; user++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user),
                        PARSE_ACTION, false) != -1)
                  count++;
            }

         }
         break;
      case DISPLAYLIST_AI_SERVICE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_AI_SERVICE_MODE,                               PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_AI_SERVICE_URL,                               PARSE_ONLY_STRING, true },
               {MENU_ENUM_LABEL_AI_SERVICE_ENABLE,                                   PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG,                                   PARSE_ONLY_UINT, true},
               {MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG,                                   PARSE_ONLY_UINT, true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ADD_CONTENT_LIST:
#ifdef HAVE_LIBRETRODB
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
               MENU_ENUM_LABEL_SCAN_DIRECTORY,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
               MENU_ENUM_LABEL_SCAN_FILE,
               MENU_SETTING_ACTION, 0, 0))
            count++;
#endif
         break;
      case DISPLAYLIST_NETWORK_INFO:
#if defined(HAVE_NETWORKING) && !defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(SWITCH) && defined(HAVE_LIBNX))
         network_init();
         {
            net_ifinfo_t      netlist;

            if (net_ifinfo_new(&netlist))
            {
               unsigned k;
               for (k = 0; k < netlist.size; k++)
               {
                  char tmp[255];

                  tmp[0] = '\0';

                  snprintf(tmp, sizeof(tmp), "%s (%s) : %s\n",
                        msg_hash_to_str(MSG_INTERFACE),
                        netlist.entries[k].name, netlist.entries[k].host);
                  if (menu_entries_append_enum(list, tmp, "",
                           MENU_ENUM_LABEL_NETWORK_INFO_ENTRY,
                           MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
                     count++;
               }

               net_ifinfo_free(&netlist);
            }
         }
#endif
         break;
      case DISPLAYLIST_OPTIONS_CHEATS:
         if (cheat_manager_alloc_if_empty())
         {
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_START_OR_CONT),
                  MENU_ENUM_LABEL_CHEAT_START_OR_CONT,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD),
                  MENU_ENUM_LABEL_CHEAT_FILE_LOAD,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND),
                  MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS),
                  MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS),
                  MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP),
                  MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM),
                  MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE_ALL),
                  MENU_ENUM_LABEL_CHEAT_DELETE_ALL,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD,
                  PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE,
                  PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES),
                  MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,
                  MENU_SETTING_ACTION, 0, 0))
               count++;

            {
               unsigned i;
               for (i = 0; i < cheat_manager_get_size(); i++)
               {
                  char cheat_label[64];

                  cheat_label[0] = '\0';

                  snprintf(cheat_label, sizeof(cheat_label),
                        "%s #%u: ", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT), i);
                  if (cheat_manager_get_desc(i))
                     strlcat(cheat_label, cheat_manager_get_desc(i), sizeof(cheat_label));
                  if (menu_entries_append_enum(list,
                        cheat_label, "", MSG_UNKNOWN,
                        MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0))
                     count++;
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_RESOLUTION:
         {
            unsigned i, size                  = 0;
            struct video_display_config *video_list = (struct video_display_config*)
               video_display_server_get_resolution_list(&size);

            if (video_list)
            {
               for (i = 0; i < size; i++)
               {
                  char val_d[256], str[256];
                  snprintf(str, sizeof(str), "%dx%d (%d Hz)",
                        video_list[i].width,
                        video_list[i].height,
                        video_list[i].refreshrate);
                  snprintf(val_d, sizeof(val_d), "%d", i);
                  if (menu_entries_append_enum(list,
                        str,
                        val_d,
                        MENU_ENUM_LABEL_NO_ITEMS,
                        MENU_SETTING_DROPDOWN_ITEM_RESOLUTION, video_list[i].idx, 0))
                     count++;

                  if (video_list[i].current)
                     menu_entries_set_checked(list, i, true);
               }

               free(video_list);
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE:
         {
            core_info_list_t *core_info_list = NULL;
            playlist_t *playlist             = playlist_get_cached();

            /* Get core list */
            core_info_get_list(&core_info_list);

            if (core_info_list && playlist)
            {
               const char *current_core_name = playlist_get_default_core_name(playlist);
               core_info_t *core_info        = NULL;
               size_t i;

               /* Sort cores alphabetically */
               core_info_qsort(core_info_list, CORE_INFO_LIST_SORT_DISPLAY_NAME);

               /* Add N/A entry */
               if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
                     "",
                     MENU_ENUM_LABEL_NO_ITEMS,
                     MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_DEFAULT_CORE,
                     0, 0))
                  count++;

               if (string_is_empty(current_core_name) ||
                   string_is_equal(current_core_name, "DETECT"))
                  menu_entries_set_checked(list, 0, true);

               /* Loop through cores */
               for (i = 0; i < core_info_list->count; i++)
               {
                  core_info = NULL;
                  core_info = core_info_get(core_info_list, i);

                  if (core_info)
                  {
                     if (menu_entries_append_enum(list,
                           core_info->display_name,
                           "",
                           MENU_ENUM_LABEL_NO_ITEMS,
                           MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_DEFAULT_CORE,
                           i + 1, 0))
                        count++;

                     if (string_is_equal(current_core_name, core_info->display_name))
                        menu_entries_set_checked(list, i + 1, true);
                  }
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
         {
            playlist_t *playlist = playlist_get_cached();

            if (playlist)
            {
               size_t i;
               enum playlist_label_display_mode current_display_mode =
                     playlist_get_label_display_mode(playlist);

               for (i = 0; i <= (unsigned)LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX; i++)
               {
                  enum msg_hash_enums label_value;
                  enum playlist_label_display_mode display_mode =
                        (enum playlist_label_display_mode)i;

                  switch (display_mode)
                  {
                     case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS;
                        break;
                     case LABEL_DISPLAY_MODE_REMOVE_BRACKETS:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS;
                        break;
                     case LABEL_DISPLAY_MODE_REMOVE_PARENTHESES_AND_BRACKETS:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS;
                        break;
                     case LABEL_DISPLAY_MODE_KEEP_REGION:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION;
                        break;
                     case LABEL_DISPLAY_MODE_KEEP_DISC_INDEX:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX;
                        break;
                     case LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX;
                        break;
                     default:
                        /* LABEL_DISPLAY_MODE_DEFAULT */
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT;
                        break;
                  }

                  if (menu_entries_append_enum(list,
                        msg_hash_to_str(label_value),
                        "",
                        MENU_ENUM_LABEL_NO_ITEMS,
                        MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LABEL_DISPLAY_MODE,
                        0, 0))
                     count++;

                  if (current_display_mode == display_mode)
                     menu_entries_set_checked(list, i, true);
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
         count = populate_playlist_thumbnail_mode_dropdown_list(list, PLAYLIST_THUMBNAIL_RIGHT);
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
         count = populate_playlist_thumbnail_mode_dropdown_list(list, PLAYLIST_THUMBNAIL_LEFT);
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         {
            unsigned i;
            struct retro_perf_counter **counters =
               (type == DISPLAYLIST_PERFCOUNTERS_CORE)
               ? retro_get_perf_counter_libretro()
               : retro_get_perf_counter_rarch();
            unsigned num                         =
               (type == DISPLAYLIST_PERFCOUNTERS_CORE)
               ?   retro_get_perf_count_libretro()
               : retro_get_perf_count_rarch();
            unsigned id                          =
               (type == DISPLAYLIST_PERFCOUNTERS_CORE)
               ? MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
               : MENU_SETTINGS_PERF_COUNTERS_BEGIN;

            if (counters && num != 0)
            {
               for (i = 0; i < num; i++)
                  if (counters[i] && counters[i]->ident)
                     if (menu_entries_append_enum(list,
                           counters[i]->ident, "",
                           (enum msg_hash_enums)(id + i),
                           id + i , 0, 0))
                        count++;
            }
         }
         break;
      case DISPLAYLIST_NETWORK_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,                               PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,                               PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,                                   PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS,                                    PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT,                                  PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_NETPLAY_PASSWORD,                                      PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD,                             PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR,                            PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES,                                  PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES,                                PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE,                                PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES,                                  PARSE_ONLY_INT   },
               {MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,                      PARSE_ONLY_INT   },
               {MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,                    PARSE_ONLY_INT   },
               {MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL,                                 PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL,                                 PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG,                                  PARSE_ONLY_UINT  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }

            {
               unsigned user;
               for (user = 0; user < MAX_USERS; user++)
               {
                  if (menu_displaylist_parse_settings_enum(list,
                           (enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + user),
                           PARSE_ONLY_BOOL, false) != -1)
                     count++;
               }
            }

            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_NETWORK_CMD_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_NETWORK_CMD_PORT,
                     PARSE_ONLY_UINT, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_NETWORK_REMOTE_PORT,
                     PARSE_ONLY_UINT, false) != -1)
               count++;

            {
               unsigned user;
               unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
               for(user = 0; user < max_users; user++)
               {
                  if (menu_displaylist_parse_settings_enum(list,
                           (enum msg_hash_enums)(
                              MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user),
                           PARSE_ONLY_BOOL, false) != -1)
                     count++;
               }
            }

            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_STDIN_CMD_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_UPDATER_SETTINGS,
                     PARSE_ACTION, false) != -1)
               count++;
         }
         break;
      case DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST:
         {
            char cheat_label[64];
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CHEAT_START_OR_RESTART,                                PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN,                                      PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT,                                    PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_LT,                                       PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_LTE,                                      PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_GT,                                       PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_GTE,                                      PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_EQ,                                       PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ,                                      PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS,                                   PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS,                                  PARSE_ONLY_UINT  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }

            {
               cheat_label[0] = '\0';
               snprintf(cheat_label, sizeof(cheat_label),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES),
                     cheat_manager_state.num_matches);

               if (menu_entries_append_enum(list,
                     cheat_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_MATCHES),
                     MENU_ENUM_LABEL_CHEAT_ADD_MATCHES,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_CHEAT_DELETE_MATCH,
                  PARSE_ONLY_UINT, false) != -1)
               count++;

            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_CHEAT_COPY_MATCH,
                  PARSE_ONLY_UINT, false) != -1)
               count++;

            {
               unsigned int address_mask = 0;
               unsigned int address      = 0;
               unsigned int prev_val     = 0;
               unsigned int curr_val     = 0;

               cheat_label[0] = '\0';

               cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_VIEW, cheat_manager_state.match_idx, &address, &address_mask, &prev_val, &curr_val) ;
               snprintf(cheat_label, sizeof(cheat_label),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_MATCH), address, address_mask);

               if (menu_entries_append_enum(list,
                     cheat_label,
                     "",
                     MSG_UNKNOWN,
                     MENU_SETTINGS_CHEAT_MATCH, 0, 0))
                  count++;
            }

            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY,
                  PARSE_ONLY_UINT, false) != -1)
               count++;

            {
               rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_DELETE_MATCH);
               if (setting)
                  setting->max = cheat_manager_state.num_matches-1;
               setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_COPY_MATCH);
               if (setting)
                  setting->max = cheat_manager_state.num_matches-1;
               setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY);
               if (setting)
                  setting->max = cheat_manager_state.total_memory_size>0?cheat_manager_state.total_memory_size-1:0 ;
            }
         }
         break;
      case DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST:
         {
            if (!cheat_manager_state.memory_initialized)
               cheat_manager_initialize_memory(NULL,true) ;

            {
               rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS);
               if (setting )
                  setting->max = cheat_manager_state.total_memory_size==0?0:cheat_manager_state.total_memory_size-1;

               setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION);
               if (setting )
                  setting->max = cheat_manager_state.working_cheat.memory_search_size<3 ? 255 : 0 ;

               setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY);
               if (setting )
                  setting->max = cheat_manager_state.total_memory_size>0?cheat_manager_state.total_memory_size-1:0 ;
            }

            {
               menu_displaylist_build_info_t build_list[] = {
                  {MENU_ENUM_LABEL_CHEAT_IDX,                                             PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_STATE,                                           PARSE_ONLY_BOOL  },
                  {MENU_ENUM_LABEL_CHEAT_DESC,                                            PARSE_ONLY_STRING},
                  {MENU_ENUM_LABEL_CHEAT_HANDLER,                                         PARSE_ONLY_UINT  },
               };

               for (i = 0; i < ARRAY_SIZE(build_list); i++)
               {
                  if (menu_displaylist_parse_settings_enum(list,
                           build_list[i].enum_idx,  build_list[i].parse_type,
                           false) == 0)
                     count++;
               }
            }

            if (cheat_manager_state.working_cheat.handler == CHEAT_HANDLER_TYPE_EMU)
               menu_displaylist_parse_settings_enum(list,
                     MENU_ENUM_LABEL_CHEAT_CODE,
                     PARSE_ONLY_STRING, false);
            else
            {
               menu_displaylist_build_info_t build_list[] = {
                  {MENU_ENUM_LABEL_CHEAT_MEMORY_SEARCH_SIZE,                              PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_TYPE,                                            PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_VALUE,                                           PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_ADDRESS,                                         PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_BROWSE_MEMORY,                                   PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION,                            PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT,                                    PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,                           PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE,                             PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_TYPE,                                     PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE,                                    PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_PORT,                                     PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_STRENGTH,                         PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_PRIMARY_DURATION,                         PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_STRENGTH,                       PARSE_ONLY_UINT  },
                  {MENU_ENUM_LABEL_CHEAT_RUMBLE_SECONDARY_DURATION,                       PARSE_ONLY_UINT  },
               };

               for (i = 0; i < ARRAY_SIZE(build_list); i++)
               {
                  if (menu_displaylist_parse_settings_enum(list,
                           build_list[i].enum_idx,  build_list[i].parse_type,
                           false) == 0)
                     count++;
               }
            }

            /* Inspect Memory At this Address */

            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER),
                  MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE),
                  MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_AFTER),
                  MENU_ENUM_LABEL_CHEAT_COPY_AFTER,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_COPY_BEFORE),
                  MENU_ENUM_LABEL_CHEAT_COPY_BEFORE,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DELETE),
                  msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DELETE),
                  MENU_ENUM_LABEL_CHEAT_DELETE,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
         }
         break;
      case DISPLAYLIST_RECORDING_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY,                                  PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_RECORD_CONFIG,                                         PARSE_ONLY_PATH  },
               {MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY,                                  PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_STREAM_CONFIG,                                         PARSE_ONLY_PATH  },
               {MENU_ENUM_LABEL_STREAMING_MODE,                                        PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_VIDEO_RECORD_THREADS,                                  PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_STREAMING_TITLE,                                       PARSE_ONLY_STRING},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }

         {
            settings_t      *settings      = config_get_ptr();
            if (settings->uints.streaming_mode == STREAMING_MODE_LOCAL)
            {
               /* TODO: Refresh on settings->uints.streaming_mode change to show this parameter */
               if (menu_displaylist_parse_settings_enum(list,
                        MENU_ENUM_LABEL_UDP_STREAM_PORT,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
            }
         }

         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_STREAMING_URL,                                         PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_VIDEO_GPU_RECORD,                                      PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,                              PARSE_ONLY_BOOL  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_YOUTUBE_STREAM_KEY,                                    PARSE_ONLY_STRING},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CHEEVOS_ENABLE,                                        PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_USERNAME,                                      PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_CHEEVOS_PASSWORD,                                      PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,                          PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE,                           PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE,                                 PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,                               PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,                                PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT,                               PARSE_ONLY_BOOL  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ACCOUNTS_TWITCH_LIST:
         if (menu_displaylist_parse_settings_enum(list,
               MENU_ENUM_LABEL_TWITCH_STREAM_KEY,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         break;
      case DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS,                                   PARSE_ACTION     },
               {MENU_ENUM_LABEL_MENU_SETTINGS,                                         PARSE_ACTION     },
               {MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS,                                PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE,                                   PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,                                PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD,                              PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND,                                 PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_PAUSE_LIBRETRO,                                        PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_MENU_SAVESTATE_RESUME,                                 PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_MOUSE_ENABLE,                                          PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_POINTER_ENABLE,                                        PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE,                          PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_PAUSE_NONACTIVE,                                       PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION,                             PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_UI_COMPANION_ENABLE,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT,                            PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_UI_MENUBAR_ENABLE,                                     PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_UI_COMPANION_TOGGLE,                                   PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_VIDEO_3DS_DISPLAY_MODE,                                PARSE_ONLY_UINT  },
               {MENU_ENUM_LABEL_VIDEO_3DS_LCD_BOTTOM,                                  PARSE_ONLY_BOOL  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               bool parse_setting = true;
               if (build_list[i].checked &&
                     string_is_equal(ui_companion_driver_get_ident(), "null"))
                  parse_setting = false;
               if (parse_setting &&
                     menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_INDEX),
               MENU_ENUM_LABEL_DISK_INDEX,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS),
               MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND),
               MENU_ENUM_LABEL_DISK_IMAGE_APPEND,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0))
            count++;
         break;
      case DISPLAYLIST_MIDI_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_MIDI_INPUT,                                            PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_MIDI_OUTPUT,                                           PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_MIDI_VOLUME,                                           PARSE_ONLY_UINT  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION,                                 PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER,                           PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING,                           PARSE_ONLY_INT },
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,         PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_LAKKA_SERVICES_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SSH_ENABLE,                                            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAMBA_ENABLE,                                          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_BLUETOOTH_ENABLE,                                      PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS,                             PARSE_ACTION, true     },
               {MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS,                             PARSE_ACTION, true     },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_DISC,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_DUMP_DISC,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,                              PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_INFORMATION,                                 PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS,                              PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_HELP,                                        PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_SHOW_WIMP,                                             PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH,                              PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_RESTART_RETROARCH,                           PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_REBOOT,                                      PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN,                                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS,                                 PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD,                        PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC,                                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO,                                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_ADD,                                      PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_TIMEDATE_ENABLE,                                       PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_TIMEDATE_STYLE,                                        PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CORE_ENABLE,                                           PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN,                                PARSE_ONLY_BOOL, true  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SHOW_HIDDEN_FILES,                                     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_USE_BUILTIN_PLAYER,                                    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_FILTER_BY_CURRENT_CORE,                                PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,                 PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ACCOUNTS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,            PARSE_ACTION},
               {MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE,                       PARSE_ACTION},
               {MENU_ENUM_LABEL_ACCOUNTS_TWITCH,                        PARSE_ACTION},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CHEEVOS_USERNAME,                       PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_CHEEVOS_PASSWORD,                       PARSE_ONLY_STRING},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE,                   PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU,             PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,     PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,        PARSE_ONLY_BOOL  },
               {MENU_ENUM_LABEL_OVERLAY_PRESET,                         PARSE_ONLY_PATH  },
               {MENU_ENUM_LABEL_OVERLAY_OPACITY,                        PARSE_ONLY_FLOAT },
               {MENU_ENUM_LABEL_OVERLAY_SCALE,                          PARSE_ONLY_FLOAT },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#ifdef HAVE_VIDEO_LAYOUT
      case DISPLAYLIST_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE,                   PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH,                     PARSE_ONLY_PATH },
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW,            PARSE_ONLY_UINT },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#endif
      case DISPLAYLIST_LATENCY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,            PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_VIDEO_HARD_SYNC,                       PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,                PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,                     PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_AUDIO_LATENCY,                         PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,              PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_INPUT_BLOCK_TIMEOUT,                   PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_RUN_AHEAD_ENABLED,                     PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_RUN_AHEAD_FRAMES,                      PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE,          PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS,               PARSE_ONLY_BOOL },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_FONT_ENABLE,            PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_FPS_SHOW,                     PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL,          PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_FRAMECOUNT_SHOW,              PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_STATISTICS_SHOW,              PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MEMORY_SHOW,                  PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_VIDEO_FONT_PATH,              PARSE_ONLY_PATH },
               {MENU_ENUM_LABEL_VIDEO_FONT_SIZE,              PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X,          PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y,          PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,      PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,    PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE,     PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,  PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,   PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,PARSE_ONLY_FLOAT},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_CONFIGURATIONS_LIST:
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS),
               MENU_ENUM_LABEL_CONFIGURATIONS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG),
               msg_hash_to_str(MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG),
               MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG),
               MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_NEW_CONFIG),
               MENU_ENUM_LABEL_SAVE_NEW_CONFIG,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         break;
      case DISPLAYLIST_PRIVACY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CAMERA_ALLOW, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_DISCORD_ALLOW,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_LOCATION_ALLOW,  PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SAVING_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_AUTOSAVE_INTERVAL,  PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE,   PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         {
            settings_t      *settings     = config_get_ptr();
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_DRIVER_SETTINGS, PARSE_ACTION, true},
               {MENU_ENUM_LABEL_VIDEO_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_AUDIO_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_INPUT_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_LATENCY_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_CORE_SETTINGS,   PARSE_ACTION, true},
               {MENU_ENUM_LABEL_CONFIGURATION_SETTINGS, PARSE_ACTION, true},
               {MENU_ENUM_LABEL_SAVING_SETTINGS, PARSE_ACTION, true},
               {MENU_ENUM_LABEL_LOGGING_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS, PARSE_ACTION, true},
               {MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS, PARSE_ACTION, true},
               {MENU_ENUM_LABEL_RECORDING_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_AI_SERVICE_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_WIFI_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_NETWORK_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_LAKKA_SERVICES,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_PLAYLIST_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_USER_SETTINGS,PARSE_ACTION, true},
               {MENU_ENUM_LABEL_DIRECTORY_SETTINGS,PARSE_ACTION, true},
            };


            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_AI_SERVICE_SETTINGS:
#ifdef HAVE_TRANSLATE
                     build_list[i].checked = settings->bools.settings_show_ai_service;
#else
                     build_list[i].checked = false;
#endif
                     break;
                  case MENU_ENUM_LABEL_DRIVER_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_drivers;
                     break;
                  case MENU_ENUM_LABEL_VIDEO_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_video;
                     break;
                  case MENU_ENUM_LABEL_AUDIO_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_audio;
                     break;
                  case MENU_ENUM_LABEL_INPUT_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_input;
                     break;
                  case MENU_ENUM_LABEL_LATENCY_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_latency;
                     break;
                  case MENU_ENUM_LABEL_CORE_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_core;
                     break;
                  case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_configuration;
                     break;
                  case MENU_ENUM_LABEL_SAVING_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_saving;
                     break;
                  case MENU_ENUM_LABEL_LOGGING_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_logging;
                     break;
                  case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_frame_throttle;
                     break;
                  case MENU_ENUM_LABEL_RECORDING_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_recording;
                     break;
                  case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_onscreen_display;
                     break;
                  case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_user_interface;
                     break;
                  case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_power_management;
                     break;
                  case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_achievements;
                     break;
                  case MENU_ENUM_LABEL_NETWORK_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_network;
                     break;
                  case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_playlists;
                     break;
                  case MENU_ENUM_LABEL_USER_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_user;
                     break;
                  case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_directory;
                     break;
                     /* MISSING:
                      * MENU_ENUM_LABEL_WIFI_SETTINGS
                      * MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS
                      * MENU_ENUM_LABEL_LAKKA_SERVICES
                      */
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (build_list[i].checked &&
                     menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SUSTAINED_PERFORMANCE_MODE, PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS, PARSE_ACTION},
#ifdef HAVE_VIDEO_LAYOUT
               {MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS, PARSE_ACTION},
#endif
               {MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,  PARSE_ACTION},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_USER_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_PRIVACY_SETTINGS,  PARSE_ACTION},
               {MENU_ENUM_LABEL_ACCOUNTS_LIST,     PARSE_ACTION},
               {MENU_ENUM_LABEL_NETPLAY_NICKNAME,  PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_USER_LANGUAGE,     PARSE_ONLY_UINT},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_UPDATER_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL,             PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL,                   PARSE_ONLY_STRING},
               {MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,     PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_MENU_SOUNDS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_AUDIO_ENABLE_MENU, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_SOUND_OK,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_SOUND_CANCEL, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_SOUND_NOTICE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_SOUND_BGM,    PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SETTINGS_SHOW_DRIVERS,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_VIDEO,            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_AUDIO,            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_INPUT,            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_LATENCY,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_CORE,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_CONFIGURATION,    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_SAVING,           PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_LOGGING,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE,       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_USER,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY,        PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS,                PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS,               PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS,                 PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         if (video_shader_any_supported())
         {
            if (menu_displaylist_parse_settings_enum(list,
                  MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS,
                  PARSE_ONLY_BOOL, false) == 0)
               count++;
         }
#endif

         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CONTENT_SHOW_REWIND,                    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY,                   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS,                  PARSE_ONLY_BOOL},
#ifdef HAVE_VIDEO_LAYOUT
               {MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO_LAYOUT,              PARSE_ONLY_BOOL},
#endif
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION,            PARSE_ONLY_BOOL},
#ifdef HAVE_NETWORKING
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,    PARSE_ONLY_BOOL},
#endif
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_CORE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE,    PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_CONFIGURATION_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE,    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS,   PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_DIRECTORY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SYSTEM_DIRECTORY,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY,        PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_ASSETS_DIRECTORY,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY, PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY,         PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,       PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY,        PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LIBRETRO_DIR_PATH,            PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LIBRETRO_INFO_PATH,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY,   PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CURSOR_DIRECTORY,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CHEAT_DATABASE_PATH,          PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_VIDEO_FILTER_DIR,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_AUDIO_FILTER_DIR,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_VIDEO_SHADER_DIR,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,   PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY,   PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_OVERLAY_DIRECTORY,            PARSE_ONLY_DIR},
#ifdef HAVE_VIDEO_LAYOUT
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY,       PARSE_ONLY_DIR},
#endif
               {MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY,         PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR,        PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY,    PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_PLAYLIST_DIRECTORY,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY,        PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_SAVEFILE_DIRECTORY,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_SAVESTATE_DIRECTORY,          PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CACHE_DIRECTORY,              PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LOG_DIR,                      PARSE_ONLY_DIR},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_DRIVER_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_INPUT_DRIVER,          PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_JOYPAD_DRIVER,         PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_VIDEO_DRIVER,          PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_AUDIO_DRIVER,          PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER,PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_CAMERA_DRIVER,         PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_LOCATION_DRIVER,       PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_MENU_DRIVER,           PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_RECORD_DRIVER,         PARSE_ONLY_STRING_OPTIONS},
               {MENU_ENUM_LABEL_MIDI_DRIVER,           PARSE_ONLY_STRING_OPTIONS},
#ifdef HAVE_LAKKA
               {MENU_ENUM_LABEL_WIFI_DRIVER,           PARSE_ONLY_STRING_OPTIONS},
#endif
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_LOGGING_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_LOG_VERBOSITY,         PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL,    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL,    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_LOG_TO_FILE,           PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_PERFCNT_ENABLE,        PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_FRAME_TIME_COUNTER_SETTINGS_LIST:
         {
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO, PARSE_ONLY_FLOAT, true},
               {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING, PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE, PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE, PARSE_ONLY_BOOL, true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_REWIND_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_REWIND_ENABLE,           PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_REWIND_GRANULARITY,      PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_REWIND_BUFFER_SIZE,      PARSE_ONLY_SIZE},
               {MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP, PARSE_ONLY_UINT},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_REWIND_SETTINGS,         PARSE_ACTION    },
               {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS, PARSE_ACTION},
               {MENU_ENUM_LABEL_FASTFORWARD_RATIO,       PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_SLOWMOTION_RATIO,        PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE,      PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE, PARSE_ONLY_BOOL },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_MENU_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_MENU_WALLPAPER,                               PARSE_ONLY_PATH },
               {MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,                            PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY,                       PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY,                     PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE,               PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,                  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_LINEAR_FILTER,                           PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,             PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO,                       PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK,                  PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,                    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,                    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,                    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,                    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME,                        PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET,                       PARSE_ONLY_PATH},
               {MENU_ENUM_LABEL_MENU_RGUI_SHADOWS,                            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT,                    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,              PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE,                          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE,                           PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_ALPHA_FACTOR,                             PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_SCALE_FACTOR,                             PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_FONT,                                     PARSE_ONLY_PATH},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,                          PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,                        PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE,                         PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_LAYOUT,                                   PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_THEME,                                    PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,                           PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,                            PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME,                         PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME,                       PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_OZONE_COLLAPSE_SIDEBAR,                       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME,                 PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE,                      PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,                  PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY,               PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY,               PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,        PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_RGUI_INLINE_THUMBNAILS,                  PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_THUMBNAILS,                                   PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_LEFT_THUMBNAILS,                              PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS,                      PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,              PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,             PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_MENU_RGUI_SWAP_THUMBNAILS,                    PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,               PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DELAY,                    PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_MENU_TICKER_TYPE,                             PARSE_ONLY_UINT },
               {MENU_ENUM_LABEL_MENU_TICKER_SPEED,                            PARSE_ONLY_FLOAT},
               {MENU_ENUM_LABEL_MENU_TICKER_SMOOTH,                           PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_OZONE_SCROLL_CONTENT_METADATA,                PARSE_ONLY_BOOL },
               {MENU_ENUM_LABEL_MENU_RGUI_EXTENDED_ASCII,                     PARSE_ONLY_BOOL },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      default:
         break;
   }

   return count;
}

/* Returns true if selection pointer should be reset
 * to zero when viewing specified history playlist */
#ifndef IS_SALAMANDER
static bool history_needs_navigation_clear(menu_handle_t *menu, playlist_t *playlist)
{
   if (!menu)
      return false;

   /* If content is running, compare last selected path
    * with current content path */
   if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return string_is_equal(menu->deferred_path, path_get(RARCH_PATH_CONTENT));

   /* If content is not running, have to examine the
    * playlist... */
   if (!playlist)
      return false;

   if (menu->rpl_entry_selection_ptr < playlist_size(playlist))
   {
      const struct playlist_entry *entry = NULL;

      playlist_get_index(playlist, menu->rpl_entry_selection_ptr, &entry);
      return !string_is_equal(menu->deferred_path, entry->path);
   }

   return false;
}
#endif

#ifdef HAVE_CDROM
static int menu_displaylist_parse_disc_info(menu_displaylist_info_t *info,
      unsigned type)
{
   unsigned i;
   unsigned           count = 0;
   struct string_list *list = cdrom_get_available_drives();

   for (i = 0; list && i < list->size; i++)
   {
      char drive_string[256] = {0};
      char drive[2]          = {0};
      size_t pos             = 0;

      drive[0]               = list->elems[i].attr.i;

      pos += snprintf(drive_string + pos, sizeof(drive_string) - pos, msg_hash_to_str(MSG_DRIVE_NUMBER), i + 1);
      pos += snprintf(drive_string + pos, sizeof(drive_string) - pos, ": %s", list->elems[i].data);

      if (menu_entries_append_enum(info->list,
               drive_string,
               drive,
               MSG_UNKNOWN,
               type,
               0, i))
         count++;
   }

   if (list)
      string_list_free(list);

   return count;
}
#endif

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type,
      menu_displaylist_info_t *info)
{
   size_t i;
   menu_ctx_displaylist_t disp_list;
   bool load_content             = true;
   bool use_filebrowser          = false;
   static bool core_selected     = false;
   unsigned count                = 0;
   int ret                       = 0;
   menu_handle_t *menu           = menu_driver_get_ptr();

   /* TODO/FIXME
    * We cannot perform a:
    *    if (!menu)
    *       return false;
    * sanity check here, because menu_displaylist_ctl() can be
    * called by menu driver init functions - i.e. we can legally
    * reach this point before the menu has been created... */

   disp_list.info = info;
   disp_list.type = type;

   if (menu_driver_push_list(&disp_list))
      return true;

   switch (type)
   {
#ifdef HAVE_CDROM
      case DISPLAYLIST_CDROM_DETAIL_INFO:
      {
         media_detect_cd_info_t cd_info = {{0}};
         char file_path[PATH_MAX_LENGTH] = {0};
         RFILE *file;
         char drive = info->path[0];
         bool atip = false;

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = 0;

         if (cdrom_drive_has_media(drive))
         {
            cdrom_device_fillpath(file_path, sizeof(file_path), drive, 0, true);

            /* opening the cue triggers storing of TOC info internally */
            file = filestream_open(file_path, RETRO_VFS_FILE_ACCESS_READ, 0);

            if (file)
            {
               const cdrom_toc_t *toc = retro_vfs_file_get_cdrom_toc();
               unsigned first_data_track = 1;

               atip = cdrom_has_atip(filestream_get_vfs_handle(file));

               filestream_close(file);

               {
                  unsigned i;

                  for (i = 0; i < toc->num_tracks; i++)
                  {
                     if (!toc->track[i].audio)
                     {
                        first_data_track = i + 1;
                        break;
                     }
                  }
               }

               /* open first data track */
               memset(file_path, 0, sizeof(file_path));
               cdrom_device_fillpath(file_path, sizeof(file_path), drive, first_data_track, false);

               if (media_detect_cd_info(file_path, 0, &cd_info))
               {
                  if (!string_is_empty(cd_info.title))
                  {
                     char title[256];

                     count++;

                     title[0] = '\0';

                     strlcpy(title, "Title: ", sizeof(title));
                     strlcat(title, cd_info.title, sizeof(title));

                     menu_entries_append_enum(info->list,
                           title,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  if (!string_is_empty(cd_info.system))
                  {
                     char system[256];

                     count++;

                     system[0] = '\0';

                     strlcpy(system, "System: ", sizeof(system));
                     strlcat(system, cd_info.system, sizeof(system));

                     menu_entries_append_enum(info->list,
                           system,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  if (!string_is_empty(cd_info.serial))
                  {
                     char serial[256];

                     count++;

                     serial[0] = '\0';

                     strlcpy(serial, "Serial#: ", sizeof(serial));
                     strlcat(serial, cd_info.serial, sizeof(serial));

                     menu_entries_append_enum(info->list,
                           serial,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  if (!string_is_empty(cd_info.version))
                  {
                     char version[256];

                     count++;

                     version[0] = '\0';

                     strlcpy(version, "Version: ", sizeof(version));
                     strlcat(version, cd_info.version, sizeof(version));

                     menu_entries_append_enum(info->list,
                           version,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  if (!string_is_empty(cd_info.release_date))
                  {
                     char release_date[256];

                     count++;

                     release_date[0] = '\0';

                     strlcpy(release_date, "Release Date: ", sizeof(release_date));
                     strlcat(release_date, cd_info.release_date, sizeof(release_date));

                     menu_entries_append_enum(info->list,
                           release_date,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  {
                     char atip_string[32] = {"Genuine Disc: "};

                     count++;

                     if (atip)
                        strlcat(atip_string, "No", sizeof(atip_string));
                     else
                        strlcat(atip_string, "Yes", sizeof(atip_string));

                     menu_entries_append_enum(info->list,
                           atip_string,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  {
                     char tracks_string[32] = {"Number of tracks: "};

                     count++;

                     snprintf(tracks_string + strlen(tracks_string), sizeof(tracks_string) - strlen(tracks_string), "%d", toc->num_tracks);

                     menu_entries_append_enum(info->list,
                           tracks_string,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0);
                  }

                  {
                     unsigned i;

                     for (i = 0; i < toc->num_tracks; i++)
                     {
                        char track_string[16]  = {"Track "};
                        char mode_string[16]   = {" - Mode: "};
                        char size_string[32]   = {" - Size: "};
                        char length_string[32] = {" - Length: "};

                        snprintf(track_string + strlen(track_string), sizeof(track_string) - strlen(track_string), "%d:", i + 1);

                        count++;

                        menu_entries_append_enum(info->list,
                              track_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0);

                        if (toc->track[i].audio)
                           snprintf(mode_string + strlen(mode_string), sizeof(mode_string) - strlen(mode_string), "Audio");
                        else
                           snprintf(mode_string + strlen(mode_string), sizeof(mode_string) - strlen(mode_string), "Mode %d", toc->track[i].mode);

                        count++;

                        menu_entries_append_enum(info->list,
                              mode_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0);

                        snprintf(size_string + strlen(size_string), sizeof(size_string) - strlen(size_string), "%.1f MB", toc->track[i].track_bytes / 1000.0 / 1000.0);

                        count++;

                        menu_entries_append_enum(info->list,
                              size_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0);

                        {
                           unsigned char min = 0;
                           unsigned char sec = 0;
                           unsigned char frame = 0;

                           cdrom_lba_to_msf(toc->track[i].track_size, &min, &sec, &frame);

                           snprintf(length_string + strlen(length_string), sizeof(length_string) - strlen(length_string), "%02d:%02d.%02d", min, sec, frame);

                           count++;

                           menu_entries_append_enum(info->list,
                                 length_string,
                                 "",
                                 MSG_UNKNOWN,
                                 FILE_TYPE_NONE, 0, 0);
                        }
                     }
                  }
               }
               else
                  RARCH_ERR("[CDROM]: Could not detect any disc info.\n");
            }
            else
               RARCH_ERR("[CDROM]: Error opening file for reading: %s\n", file_path);
         }
         else
         {
            RARCH_LOG("[CDROM]: No media is inserted or drive is not ready.\n");

            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_NO_DISC_INSERTED),
                  1, 100, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      }
      case DISPLAYLIST_DISC_INFO:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_disc_info(info,
               MENU_SET_CDROM_INFO);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      case DISPLAYLIST_DUMP_DISC:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_disc_info(info,
               MENU_SET_CDROM_LIST);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      case DISPLAYLIST_LOAD_DISC:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_disc_info(info,
               MENU_SET_LOAD_CDROM_LIST);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
#else
      case DISPLAYLIST_CDROM_DETAIL_INFO:
      case DISPLAYLIST_DISC_INFO:
      case DISPLAYLIST_LOAD_DISC:
      case DISPLAYLIST_DUMP_DISC:
         /* No-op */
         break;
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case DISPLAYLIST_SWITCH_CPU_PROFILE:
      {
         unsigned i;
         char text[PATH_MAX_LENGTH];
         char current_profile[PATH_MAX_LENGTH];
         FILE               *profile = NULL;
         const size_t profiles_count = sizeof(SWITCH_CPU_PROFILES)/sizeof(SWITCH_CPU_PROFILES[1]);

         runloop_msg_queue_push("Warning : extended overclocking can damage the Switch", 1, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_LAKKA_SWITCH
         profile = popen("cpu-profile get", "r");
         fgets(current_profile, PATH_MAX_LENGTH, profile);
         pclose(profile);

         snprintf(text, sizeof(text), "Current profile : %s", current_profile);
#else
         u32 currentClock = 0;
         if(hosversionBefore(8, 0, 0)) {
            pcvGetClockRate(PcvModule_CpuBus, &currentClock);
         } else {
            ClkrstSession session = {0};
            clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
            clkrstGetClockRate(&session, &currentClock);
            clkrstCloseSession(&session);
         }
         snprintf(text, sizeof(text), "Current Clock : %i", currentClock);
#endif
         menu_entries_append_enum(info->list,
            text,
            "",
            0,
            MENU_INFO_MESSAGE, 0, 0);

         for (i = 0; i < profiles_count; i++)
         {
            char* profile               = SWITCH_CPU_PROFILES[i];
            char* speed                 = SWITCH_CPU_SPEEDS[i];

            char title[PATH_MAX_LENGTH] = {0};

            snprintf(title, sizeof(title), "%s (%s)", profile, speed);

            if (menu_entries_append_enum(info->list,
                  title,
                  "",
                  0, MENU_SET_SWITCH_CPU_PROFILE, 0, i))
               count++;

         }

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;

         break;
      }
#if defined(HAVE_LAKKA_SWITCH)
      case DISPLAYLIST_SWITCH_GPU_PROFILE:
      {
         unsigned i;
         char text[PATH_MAX_LENGTH];
         char current_profile[PATH_MAX_LENGTH];
         FILE               *profile = NULL;
         const size_t profiles_count = sizeof(SWITCH_GPU_PROFILES)/sizeof(SWITCH_GPU_PROFILES[1]);

         runloop_msg_queue_push("Warning : extented overclocking can damage the Switch", 1, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         profile = popen("gpu-profile get", "r");
         fgets(current_profile, PATH_MAX_LENGTH, profile);
         pclose(profile);

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         snprintf(text, sizeof(text), "Current profile : %s", current_profile);

         menu_entries_append_enum(info->list, text, "", 0, MENU_INFO_MESSAGE, 0, 0);

         for (i = 0; i < profiles_count; i++)
         {
            char* profile               = SWITCH_GPU_PROFILES[i];
            char* speed                 = SWITCH_GPU_SPEEDS[i];
            char title[PATH_MAX_LENGTH] = {0};

            snprintf(title, sizeof(title), "%s (%s)", profile, speed);

            if (menu_entries_append_enum(info->list, title, "", 0, MENU_SET_SWITCH_GPU_PROFILE, 0, i))
               count++;
         }

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;

         break;
      }
      case DISPLAYLIST_SWITCH_BACKLIGHT_CONTROL:
      {
         unsigned i;
         const size_t brightness_count = sizeof(SWITCH_BRIGHTNESS)/sizeof(SWITCH_BRIGHTNESS[1]);

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         for (i = 0; i < brightness_count; i++)
         {
            char title[PATH_MAX_LENGTH] = {0};

            snprintf(title, sizeof(title), "Set to %d%%", SWITCH_BRIGHTNESS[i]);

            if (menu_entries_append_enum(info->list, title, "", 0, MENU_SET_SWITCH_BRIGHTNESS, 0, i))
               count++;
         }

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;

         break;
      }
#endif /* HAVE_LAKKA_SWITCH */
#endif /* HAVE_LAKKA_SWITCH || HAVE_LIBNX */
      case DISPLAYLIST_MUSIC_LIST:
         {
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_AUDIOMIXER
            {
               char combined_path[PATH_MAX_LENGTH];
               const char *ext  = NULL;

               combined_path[0] = '\0';

               fill_pathname_join(combined_path, menu->scratch2_buf,
                     menu->scratch_buf, sizeof(combined_path));

               ext = path_get_extension(combined_path);


               if (audio_driver_mixer_extension_supported(ext))
               {
                  if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION),
                           msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION),
                           MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION,
                           FILE_TYPE_PLAYLIST_ENTRY, 0, 0))
                     count++;

                  if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY),
                           msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY),
                           MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
                           FILE_TYPE_PLAYLIST_ENTRY, 0, 0))
                     count++;
               }
            }
#endif

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            {
               settings_t      *settings     = config_get_ptr();
               if (settings->bools.multimedia_builtin_mediaplayer_enable)
               {
                  if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN_MUSIC),
                        msg_hash_to_str(MENU_ENUM_LABEL_RUN_MUSIC),
                        MENU_ENUM_LABEL_RUN_MUSIC,
                        FILE_TYPE_PLAYLIST_ENTRY, 0, 0))
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

#ifdef HAVE_AUDIOMIXER
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

            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY),
                  lbl_play,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN  +  id),
                  0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED),
                     lbl_play_looped,
                     MSG_UNKNOWN,
                     (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN  +  id),
                     0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL),
                  lbl_play_sequential,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN  +  id),
                  0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP),
                  lbl_stop,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN  +  id),
                  0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE),
                  lbl_remove,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN  +  id),
                  0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME),
                  lbl_volume,
                  MSG_UNKNOWN,
                  (MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN  +  id),
                  0, 0))
               count++;
         }
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

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
      case DISPLAYLIST_INFORMATION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_content_information(menu, info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_DATABASE_ENTRY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#ifdef HAVE_LIBRETRODB
            bool parse_database          = false;
#endif
            struct string_list *str_list = NULL;

            if (!string_is_empty(info->label))
            {
               str_list     = string_split(info->label, "|");
               free(info->label);
               info->label  = NULL;
            }
            if (!string_is_empty(info->path_b))
            {
               free(info->path_b);
               info->path_b = NULL;
            }

            if (str_list)
            {
               if (str_list->size > 1)
               {
                  if (!string_is_empty(str_list->elems[0].data) &&
                      !string_is_empty(str_list->elems[1].data))
                  {
                     info->path_b   = strdup(str_list->elems[1].data);
                     info->label    = strdup(str_list->elems[0].data);
#ifdef HAVE_LIBRETRODB
                     parse_database = true;
#endif
                  }
               }

               string_list_free(str_list);
            }

#ifdef HAVE_LIBRETRODB
            if (parse_database)
               ret = menu_displaylist_parse_database_entry(menu, info);
            else
               info->need_push_no_playlist_entries = true;
#else
            ret = 0;
            info->need_push_no_playlist_entries = true;
#endif
         }

         info->need_push    = true;
         break;
      case DISPLAYLIST_DATABASE_QUERY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_LIBRETRODB
         {
            unsigned i;
            const char *query             = string_is_empty(info->path_c) ? NULL : info->path_c;
            database_info_list_t *db_list = database_info_list_new(info->path, query);

            if (db_list)
            {
               for (i = 0; i < db_list->count; i++)
               {
                  if (!string_is_empty(db_list->list[i].name))
                     if (menu_entries_append_enum(info->list, db_list->list[i].name,
                              info->path, MENU_ENUM_LABEL_RDB_ENTRY, FILE_TYPE_RDB_ENTRY, 0, 0))
                        count++;
               }
            }

            database_info_list_free(db_list);
            free(db_list);
         }
#endif
         if (!string_is_empty(info->path))
            free(info->path);
         info->path         = strdup(info->path_b);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_sort    = true;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_SHADERS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            unsigned i;
            struct video_shader *shader = menu_shader_get();
            unsigned pass_count         = shader ? shader->passes : 0;
            settings_t      *settings   = config_get_ptr();

            if (menu_displaylist_parse_settings_enum(info->list,
                  MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
               count++;

            if (settings->bools.video_shader_enable)
            {
               if (frontend_driver_can_watch_for_changes())
               {
                  if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES),
                           msg_hash_to_str(MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES),
                           MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES,
                           0, 0, 0))
                     count++;
               }
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET,
                        FILE_TYPE_PATH, 0, 0))
                  count++;

               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES),
                        msg_hash_to_str(MENU_ENUM_LABEL_SHADER_APPLY_CHANGES),
                        MENU_ENUM_LABEL_SHADER_APPLY_CHANGES,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES),
                        MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES,
                        0, 0, 0))
                  count++;

               for (i = 0; i < pass_count; i++)
               {
                  char buf_tmp[64];
                  char buf[128];

                  buf[0] = buf_tmp[0] = '\0';

                  snprintf(buf_tmp, sizeof(buf_tmp),
                        "%s #%u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER), i);

                  if (menu_entries_append_enum(info->list, buf_tmp,
                           msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PASS),
                           MENU_ENUM_LABEL_VIDEO_SHADER_PASS,
                           MENU_SETTINGS_SHADER_PASS_0 + i, 0, 0))
                     count++;

                  snprintf(buf, sizeof(buf), "%s %s", buf_tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILTER));
                  if (menu_entries_append_enum(info->list, buf,
                           msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS),
                           MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS,
                           MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0, 0))
                     count++;

                  snprintf(buf, sizeof(buf), "%s %s", buf_tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCALE));
                  if (menu_entries_append_enum(info->list, buf,
                           msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS),
                           MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS,
                           MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0, 0))
                     count++;
               }
            }
         }
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         break;
      case DISPLAYLIST_CORE_CONTENT:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         count = print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len,
               FILE_TYPE_DOWNLOAD_CORE_CONTENT, true, false);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

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

            count = print_buf_lines(info->list, menu->core_buf, new_label,
                  (int)menu->core_len, FILE_TYPE_DOWNLOAD_URL, false, false);

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                     MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                     FILE_TYPE_NONE, 0, 0);

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

            count = print_buf_lines(info->list, menu->core_buf, new_label,
                  (int)menu->core_len, FILE_TYPE_DOWNLOAD_URL, true, false);

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                     MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                     FILE_TYPE_NONE, 0, 0);

            info->need_push    = true;
            info->need_refresh = true;
            info->need_clear   = true;
#endif
         }
         break;
      case DISPLAYLIST_CORES_UPDATER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         count = print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_CORE, true, true);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_THUMBNAILS_UPDATER:
#ifdef HAVE_NETWORKING
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT,
               true, false);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_PL_THUMBNAILS_UPDATER:
#ifdef HAVE_NETWORKING
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_pl_thumbnail_download_list(info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_LAKKA:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         count = print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_LAKKA,
               true, false);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
#endif
         break;
      case DISPLAYLIST_PLAYLIST_COLLECTION:
         /* Note: This would appear to be legacy code. Cannot find
          * a single instance where this case is met... */
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
               if (settings->bools.playlist_sort_alphabetical)
                  playlist_qsort(playlist);

               ret = menu_displaylist_parse_playlist(info,
                     playlist, path_playlist, true);
            }

            if (ret == 0)
            {
               info->need_sort    = settings->bools.playlist_sort_alphabetical;
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
                     false, /* Not a collection */
                     false, /* Do not sort */
                     &ret);
            else
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE),
                     MENU_ENUM_LABEL_NO_HISTORY_AVAILABLE,
                     MENU_INFO_MESSAGE, 0, 0);
         }

         ret                         = 0;
         info->need_refresh          = true;
         info->need_push             = true;
#ifndef IS_SALAMANDER
         info->need_navigation_clear =
               history_needs_navigation_clear(menu, g_defaults.content_history);
#endif

         break;
      case DISPLAYLIST_FAVORITES:
         {
            settings_t      *settings     = config_get_ptr();
            info->count                   = 0;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            menu_displaylist_parse_playlist_generic(menu, info,
                  "favorites",
                  settings->paths.path_content_favorites,
                  false, /* Not a conventional collection */
                  settings->bools.playlist_sort_alphabetical,
                  &ret);

            if (info->count == 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_FAVORITES_AVAILABLE),
                     MENU_ENUM_LABEL_NO_FAVORITES_AVAILABLE,
                     MENU_INFO_MESSAGE, 0, 0);
               info->need_push_no_playlist_entries = false;
               ret = 0;
            }

            ret                   = 0;
            info->need_sort       = settings->bools.playlist_sort_alphabetical;
            info->need_refresh    = true;
            info->need_push       = true;
         }
         break;
      case DISPLAYLIST_MUSIC_HISTORY:
         {
            settings_t      *settings     = config_get_ptr();
            info->count                   = 0;

            if (settings->bools.history_list_enable)
               menu_displaylist_parse_playlist_generic(menu, info,
                     "music_history",
                     settings->paths.path_content_music_history,
                     false, /* Not a collection */
                     false, /* Do not sort */
                     &ret);

            if (info->count == 0)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_MUSIC_AVAILABLE),
                     MENU_ENUM_LABEL_NO_MUSIC_AVAILABLE,
                     MENU_INFO_MESSAGE, 0, 0);
               info->need_push_no_playlist_entries = false;
               ret = 0;
            }
         }

         if (ret == 0)
         {
            info->need_refresh          = true;
            info->need_push             = true;
#ifndef IS_SALAMANDER
            info->need_navigation_clear =
                  history_needs_navigation_clear(menu, g_defaults.music_history);
#endif
         }
         break;
      case DISPLAYLIST_VIDEO_HISTORY:
         info->count           = 0;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     "video_history",
                     settings->paths.path_content_video_history,
                     false, /* Not a collection */
                     false, /* Do not sort */
                     &ret);
               count++;
            }
            else
               ret = 0;
         }
#endif

         if (info->count == 0)
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_VIDEOS_AVAILABLE),
                  MENU_ENUM_LABEL_NO_VIDEOS_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            info->need_push_no_playlist_entries = false;
            ret = 0;
         }

         if (ret == 0)
         {
            info->need_refresh          = true;
            info->need_push             = true;
#if !defined(IS_SALAMANDER) && (defined(HAVE_FFMPEG) || defined(HAVE_MPV))
            info->need_navigation_clear =
                  history_needs_navigation_clear(menu, g_defaults.video_history);
#endif
         }
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_system_info(info);
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_ACHIEVEMENT_LIST:
#ifdef HAVE_CHEEVOS
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         rcheevos_populate_menu(info);
#endif
         info->need_push    = true;
         info->need_refresh = true;
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
               struct retro_system_info *system = runloop_get_libretro_system_info();
               const char *core_name            = system ? system->library_name : NULL;

               if (!path_is_empty(RARCH_PATH_CORE))
               {

                  if (menu_entries_append_enum(info->list,
                        path_get(RARCH_PATH_CORE),
                        path_get(RARCH_PATH_CORE),
                        MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE,
                        FILE_TYPE_DIRECT_LOAD,
                        0,
                        0))
                     count++;

                  if (!string_is_empty(core_name))
                     file_list_set_alt_at_offset(info->list, 0,
                           core_name);
               }
               else
               {
                  if (system)
                  {
                     if (menu_entries_append_enum(info->list,
                           core_name,
                           core_name,
                           MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE,
                           FILE_TYPE_DIRECT_LOAD,
                           0,
                           0))
                        count++;
                  }
                  else
                  {
                     if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE),
                           msg_hash_to_str(MENU_ENUM_LABEL_NO_CORES_AVAILABLE),
                           MENU_ENUM_LABEL_NO_CORES_AVAILABLE,
                           0, 0, 0))
                        count++;
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
                     if (menu_entries_append_enum(info->list, core_path,
                           msg_hash_to_str(MENU_ENUM_LABEL_DETECT_CORE_LIST_OK),
                           MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
                           FILE_TYPE_CORE, 0, 0))
                        count++;

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
         core_selected      = true;

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

                  if (menu_entries_append_enum(info->list,
                        path_get(RARCH_PATH_CORE),
                        path_get(RARCH_PATH_CORE),
                        MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
                        FILE_TYPE_DIRECT_LOAD,
                        0,
                        0))
                     count++;

                  {
                     struct retro_system_info *system = runloop_get_libretro_system_info();
                     const char *core_name            = system ? system->library_name : NULL;

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
                     if (menu_entries_append_enum(info->list, core_path, "",
                           MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
                           FILE_TYPE_CORE, 0, 0))
                        count++;
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
         {
            /* Number of displayed options is dynamic. If user opens
             * 'Quick Menu > Core Options', toggles something
             * that changes the number of displayed items, then
             * toggles the Quick Menu off and on again (returning
             * to the Core Options menu) the menu must be refreshed
             * (or undefined behaviour occurs).
             * We therefore have to cache the last set menu size,
             * and compare this with the new size after processing
             * the current core_option_manager_t struct.
             * Note: It would be 'nicer' to only refresh the menu
             * if the selection marker is at an index higher than
             * the new size, but we don't really have access that
             * information at this stage (i.e. the selection can
             * change after this function is called) */
            static size_t prev_count = 0;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
            {
               size_t                   opts = 0;
               settings_t      *settings     = config_get_ptr();

               rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

               if (settings->bools.game_specific_options)
               {
                  if (!rarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  {
                     if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE),
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_CREATE),
                           MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_CREATE,
                           MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0))
                        count++;
                  }
                  else
                     if (menu_entries_append_enum(info->list,
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE),
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_IN_USE),
                           MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS_IN_USE,
                           MENU_SETTINGS_CORE_OPTION_CREATE, 0, 0))
                        count++;
               }

               if (opts != 0)
               {
                  core_option_manager_t *coreopts = NULL;

                  rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

                  for (i = 0; i < opts; i++)
                  {
                     if (core_option_manager_get_visible(coreopts, i))
                     {
                        menu_entries_append_enum(info->list,
                              core_option_manager_get_desc(coreopts, i), "",
                              MENU_ENUM_LABEL_CORE_OPTION_ENTRY,
                              (unsigned)(MENU_SETTINGS_CORE_OPTION_START + i), 0, 0);
                        count++;
                     }
                  }
               }
            }

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE),
                     MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE,
                     MENU_SETTINGS_CORE_OPTION_NONE, 0, 0);

            if (count != prev_count)
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
               prev_count                  = count;
            }
            info->need_push                = true;
         }
         break;
      case DISPLAYLIST_ARCHIVE_ACTION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_COMPRESSION
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_OPEN_ARCHIVE),
               MENU_ENUM_LABEL_OPEN_ARCHIVE,
               0, 0, 0))
            count++;
#endif
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_ARCHIVE),
               MENU_ENUM_LABEL_LOAD_ARCHIVE,
               0, 0, 0))
            count++;

         info->need_push = true;
         break;
      case DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_COMPRESSION
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE),
               MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE,
               0, 0, 0))
            count++;
#endif
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE),
               MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE,
               0, 0, 0))
            count++;

         info->need_push = true;
         break;
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_HISTORY_LIST_ENABLE,                 PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE,                PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE,              PARSE_ONLY_INT },
               {MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME,               PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE,               PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,      PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,      PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE, PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG,                 PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE,       PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (menu_displaylist_parse_settings_enum(info->list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }

         if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST),
                  MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;

         info->need_push    = true;
         break;

      case DISPLAYLIST_PLAYLIST_MANAGER_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_playlist_manager_list(info);

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;

         break;
      case DISPLAYLIST_PLAYLIST_MANAGER_SETTINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (!menu_displaylist_parse_playlist_manager_settings(menu, info, info->path))
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;

         break;
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            unsigned i;

            for (i = 0; i < RARCH_BIND_LIST_END; i++)
            {
               if (menu_displaylist_parse_settings_enum(info->list,
                     (enum msg_hash_enums)(
                        MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i),
                     PARSE_ONLY_BIND, false) == 0)
                  count++;
            }
         }
         info->need_push    = true;
         break;
      case DISPLAYLIST_SAVING_SETTINGS_LIST:
      case DISPLAYLIST_DRIVER_SETTINGS_LIST:
      case DISPLAYLIST_LOGGING_SETTINGS_LIST:
      case DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST:
      case DISPLAYLIST_REWIND_SETTINGS_LIST:
      case DISPLAYLIST_DIRECTORY_SETTINGS_LIST:
      case DISPLAYLIST_CONFIGURATION_SETTINGS_LIST:
      case DISPLAYLIST_CORE_SETTINGS_LIST:
      case DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST:
      case DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST:
      case DISPLAYLIST_MENU_SOUNDS_LIST:
      case DISPLAYLIST_UPDATER_SETTINGS_LIST:
      case DISPLAYLIST_USER_SETTINGS_LIST:
      case DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST:
      case DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST:
      case DISPLAYLIST_SETTINGS_ALL:
      case DISPLAYLIST_PRIVACY_SETTINGS_LIST:
      case DISPLAYLIST_CONFIGURATIONS_LIST:
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
      case DISPLAYLIST_LATENCY_SETTINGS_LIST:
      case DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST:
#ifdef HAVE_VIDEO_LAYOUT
      case DISPLAYLIST_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST:
#endif
      case DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST:
      case DISPLAYLIST_ACCOUNTS_LIST:
      case DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST:
      case DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST:
      case DISPLAYLIST_LAKKA_SERVICES_LIST:
      case DISPLAYLIST_MIDI_SETTINGS_LIST:
      case DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST:
      case DISPLAYLIST_OPTIONS_DISK:
      case DISPLAYLIST_AI_SERVICE_SETTINGS_LIST:
      case DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST:
      case DISPLAYLIST_ACCOUNTS_TWITCH_LIST:
      case DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
      case DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST:
      case DISPLAYLIST_RECORDING_SETTINGS_LIST:
      case DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST:
      case DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST:
      case DISPLAYLIST_NETWORK_SETTINGS_LIST:
      case DISPLAYLIST_OPTIONS_CHEATS:
      case DISPLAYLIST_NETWORK_INFO:
      case DISPLAYLIST_DROPDOWN_LIST_RESOLUTION:
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE:
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
      case DISPLAYLIST_MENU_SETTINGS_LIST:
      case DISPLAYLIST_ADD_CONTENT_LIST:
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
      case DISPLAYLIST_FRAME_TIME_COUNTER_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_build_list(info->list, type);

         if (count == 0)
         {
            switch (type)
            {
               case DISPLAYLIST_ADD_CONTENT_LIST:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                        MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                        FILE_TYPE_NONE, 0, 0);
                  break;
               case DISPLAYLIST_DROPDOWN_LIST_RESOLUTION:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                        MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                        FILE_TYPE_NONE, 0, 0);
                  break;
               case DISPLAYLIST_PERFCOUNTERS_CORE:
               case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS),
                        MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS,
                        0, 0, 0);
                  break;
               case DISPLAYLIST_MENU_SETTINGS_LIST:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                        MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                        FILE_TYPE_NONE, 0, 0);
                  break;
               default:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
                        MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
                        0, 0, 0);
                  break;
            }
         }

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
                     if (menu_entries_append_enum(info->list,
                              ssid,
                              msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_WIFI),
                              MENU_ENUM_LABEL_CONNECT_WIFI,
                              MENU_WIFI, 0, 0))
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
      case DISPLAYLIST_VIDEO_SETTINGS_LIST:
      {
         gfx_ctx_flags_t flags;

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (video_display_server_get_flags(&flags))
         {
            if (BIT32_GET(flags.flags, DISPSERV_CTX_CRT_SWITCHRES))
               menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,
                     PARSE_ACTION, false);
         }

         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE,
               PARSE_ONLY_BOOL, false);

#if defined(GEKKO) || defined(__CELLOS_LV2__)
         if (true)
#else
         if (video_display_server_has_resolution_list())
#endif
         {
            menu_displaylist_parse_settings_enum(info->list,
                  MENU_ENUM_LABEL_SCREEN_RESOLUTION,
                  PARSE_ACTION, false);
         }

         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_PAL60_ENABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_GAMMA,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SOFT_FILTER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y,
               PARSE_ONLY_INT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SCALE,
               PARSE_ONLY_FLOAT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VI_WIDTH,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VFILTER,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_ROTATION,
               PARSE_ONLY_UINT, false);
         if (video_display_server_can_set_screen_orientation())
            menu_displaylist_parse_settings_enum(info->list,
                  MENU_ENUM_LABEL_SCREEN_ORIENTATION,
                  PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_THREADED,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_VSYNC,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL,
               PARSE_ONLY_UINT, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC,
               PARSE_ONLY_BOOL, false);
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION,
               PARSE_ONLY_BOOL, false);
         if (video_driver_supports_viewport_read())
            menu_displaylist_parse_settings_enum(info->list,
                  MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT,
                  PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN,
               PARSE_ONLY_BOOL, false);
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SMOOTH,
               PARSE_ONLY_BOOL, false);
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_SHADER_DELAY,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_VIDEO_FILTER,
               PARSE_ONLY_PATH, false);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      }
      case DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST:
         {
#ifdef HAVE_AUDIOMIXER
            unsigned i;
#endif
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_AUDIOMIXER
#if 1
            /* TODO - for developers -
             * turn this into #if 0 if you want to be able to see
             * the system streams as well. */
            for (i = 0; i < AUDIO_MIXER_MAX_STREAMS; i++)
#else
            for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
#endif
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
#endif

            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_AUDIO_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_MIDI_SETTINGS,
               PARSE_ACTION, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,
               PARSE_ACTION, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_MENU_SOUNDS,
               PARSE_ACTION, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_ENABLE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_DEVICE,
               PARSE_ONLY_STRING, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_LATENCY,
               PARSE_ONLY_UINT, false) == 0)
            count++;

         /* Volume */
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_MUTE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_MIXER_MUTE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_VOLUME,
               PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME,
               PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;

         /* Resampler */
         {
            settings_t *settings      = config_get_ptr();
            if (string_is_not_equal(settings->arrays.audio_resampler, "null"))
            {
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY,
                     PARSE_ONLY_UINT, false) == 0)
                  count++;
            }
         }
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE,
               PARSE_ONLY_UINT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES,
               PARSE_ONLY_UINT, false) == 0)
            count++;

         /* Synchronization */
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_SYNC,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW,
               PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
               PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,
               PARSE_ONLY_PATH, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT,
               PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
               PARSE_ONLY_INT, false) == 0)
            count++;

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         {
            settings_t *settings      = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            ret = menu_displaylist_parse_horizontal_list(menu, info, settings->bools.playlist_sort_alphabetical);

            info->need_sort    = settings->bools.playlist_sort_alphabetical;
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_content_actions(menu, info);
         info->need_refresh  = true;
         info->need_push     = true;

         if (core_selected)
         {
            info->need_clear = true;
            core_selected    = false;
         }

         break;
      case DISPLAYLIST_CONTENT_SETTINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_load_content_settings(menu, info);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INFORMATION_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count              = menu_displaylist_parse_information_list(info);

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
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_LIBRETRODB
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_DIRECTORY),
               MENU_ENUM_LABEL_SCAN_DIRECTORY,
               MENU_SETTING_ACTION, 0, 0))
            count++;
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE),
               MENU_ENUM_LABEL_SCAN_FILE,
               MENU_SETTING_ACTION, 0, 0))
            count++;
#endif

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
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                     msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                     MENU_ENUM_LABEL_FAVORITES,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_favorites)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
                     MENU_ENUM_LABEL_GOTO_FAVORITES,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_images)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
                     MENU_ENUM_LABEL_GOTO_IMAGES,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_music)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
                     MENU_ENUM_LABEL_GOTO_MUSIC,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            if (settings->bools.menu_content_show_video)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
                     msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
                     MENU_ENUM_LABEL_GOTO_VIDEO,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
#endif
         }

         {
            core_info_list_t *list        = NULL;
            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
            {
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(
                        MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
         }

#ifndef HAVE_LIBRETRODB
         {
            settings_t *settings = config_get_ptr();
            if (settings->bools.menu_show_advanced_settings)
#endif
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
#ifndef HAVE_LIBRETRODB
         }
#endif

         if (frontend_driver_parse_drive_list(info->list, true) != 0)
            if (menu_entries_append_enum(info->list, "/",
                  msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                  MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                  MENU_SETTING_ACTION, 0, 0))
               count++;

#if 0
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL_LIST),
               MENU_ENUM_LABEL_BROWSE_URL_LIST,
               MENU_SETTING_ACTION, 0, 0))
            count++;
#endif
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_OPTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#ifdef HAVE_NETWORKING
            settings_t *settings         = config_get_ptr();
#endif

#ifdef HAVE_LAKKA
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_LAKKA),
                     MENU_ENUM_LABEL_UPDATE_LAKKA,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            if (settings->bools.menu_show_legacy_thumbnail_updater)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
                        msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
                        MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST),
                     MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
                     MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#elif defined(HAVE_NETWORKING)
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#else
            if (settings->bools.menu_show_core_updater)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                        msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
                        MENU_ENUM_LABEL_CORE_UPDATER_LIST,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
#endif
            if (settings->bools.menu_show_legacy_thumbnail_updater)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST),
                        msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST),
                        MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST),
                     MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
                     MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

#ifdef HAVE_COMPRESSION
            if (settings->bools.menu_show_core_updater)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES),
                        msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES),
                        MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

#ifdef HAVE_UPDATE_ASSETS
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_ASSETS),
                     MENU_ENUM_LABEL_UPDATE_ASSETS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#endif

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES),
                     MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CHEATS),
                     MENU_ENUM_LABEL_UPDATE_CHEATS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

#ifdef HAVE_LIBRETRODB
#if !defined(VITA)
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_DATABASES),
                     MENU_ENUM_LABEL_UPDATE_DATABASES,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#endif
#endif

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_OVERLAYS),
                     MENU_ENUM_LABEL_UPDATE_OVERLAYS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            if (video_shader_is_supported(RARCH_SHADER_CG))
            {
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS),
                     msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_CG_SHADERS),
                     MENU_ENUM_LABEL_UPDATE_CG_SHADERS,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (video_shader_is_supported(RARCH_SHADER_GLSL))
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS),
                        msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS),
                        MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (video_shader_is_supported(RARCH_SHADER_SLANG))
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS),
                        msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS),
                        MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
#endif
#endif
#endif

#ifdef HAVE_NETWORKING
            if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;
#endif
         }

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                  MENU_ENUM_LABEL_NO_ITEMS,
                  MENU_SETTING_NO_ITEM, 0, 0);

         ret                = 0;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         {
            unsigned p;
            settings_t        *settings = config_get_ptr();
            unsigned max_users          = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
            bool is_rgui                = string_is_equal(
                  settings->arrays.menu_driver, "rgui");

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

               if (menu_displaylist_parse_settings(info->list,
                        key_type, PARSE_ONLY_UINT, true, 0) == 0)
                  count++;
               if (menu_displaylist_parse_settings(info->list,
                        key_analog, PARSE_ONLY_UINT, true, 0) == 0)
                  count++;
            }

            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD),
                     msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_LOAD),
                     MENU_ENUM_LABEL_REMAP_FILE_LOAD,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE),
                     msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE),
                     MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR),
                     msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR),
                     MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME),
                     msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME),
                     MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            if (rarch_ctl(RARCH_CTL_IS_REMAPS_CORE_ACTIVE, NULL))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE),
                        msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE),
                        MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (rarch_ctl(RARCH_CTL_IS_REMAPS_GAME_ACTIVE, NULL))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME),
                        msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME),
                        MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (rarch_ctl(RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE, NULL))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR),
                        msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR),
                        MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            for (p = 0; p < max_users; p++)
            {
               unsigned retro_id;
               unsigned device  = settings->uints.input_libretro_device[p];
               device &= RETRO_DEVICE_MASK;

               if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
               {
                  for (retro_id = 0; retro_id < RARCH_ANALOG_BIND_LIST_END; retro_id++)
                  {
                     char desc_label[400];
                     char descriptor[300];
                     const struct retro_keybind *keybind   =
                        &input_config_binds[p][retro_id];
                     const struct retro_keybind *auto_bind =
                        (const struct retro_keybind*)
                        input_config_get_bind_auto(p, retro_id);

                     input_config_get_bind_string(descriptor,
                           keybind, auto_bind, sizeof(descriptor));

                     if (!strstr(descriptor, "Auto"))
                     {
                        const struct retro_keybind *keyptr =
                           &input_config_binds[p][retro_id];

                        snprintf(desc_label, sizeof(desc_label),
                              "%s %s", msg_hash_to_str(keyptr->enum_idx), descriptor);
                        strlcpy(descriptor, desc_label, sizeof(descriptor));
                     }

                     /* Add user index when display driver == rgui and sublabels
                      * are disabled, but only if there is more than one user */
                     if (     (is_rgui)
                           && (max_users > 1)
                           && !settings->bools.menu_show_sublabels)
                     {
                        snprintf(desc_label, sizeof(desc_label),
                              "%s [%s %u]", descriptor, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), p + 1);
                        strlcpy(descriptor, desc_label, sizeof(descriptor));
                     }

                     if (menu_entries_append_enum(info->list, descriptor, "",
                              MSG_UNKNOWN,
                              MENU_SETTINGS_INPUT_DESC_BEGIN +
                              (p * (RARCH_FIRST_CUSTOM_BIND + 8)) +  retro_id, 0, 0))
                        count++;
                  }
               }
               else if (device == RETRO_DEVICE_KEYBOARD)
               {
                  for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                  {
                     char desc_label[400];
                     char descriptor[300];
                     const struct retro_keybind *keybind   =
                        &input_config_binds[p][retro_id];
                     const struct retro_keybind *auto_bind =
                        (const struct retro_keybind*)
                        input_config_get_bind_auto(p, retro_id);

                     input_config_get_bind_string(descriptor,
                           keybind, auto_bind, sizeof(descriptor));

                     if (!strstr(descriptor, "Auto"))
                     {
                        const struct retro_keybind *keyptr =
                           &input_config_binds[p][retro_id];

                        strlcpy(descriptor,
                              msg_hash_to_str(keyptr->enum_idx), sizeof(descriptor));
                     }

                     /* Add user index when display driver == rgui and sublabels
                      * are disabled, but only if there is more than one user */
                     if (     (is_rgui)
                           && (max_users > 1)
                           && !settings->bools.menu_show_sublabels)
                     {
                        snprintf(desc_label, sizeof(desc_label),
                              "%s [%s %u]", descriptor,
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), p + 1);
                        strlcpy(descriptor, desc_label, sizeof(descriptor));
                     }

                     if (menu_entries_append_enum(info->list, descriptor, "",
                              MSG_UNKNOWN,
                              MENU_SETTINGS_INPUT_DESC_KBD_BEGIN +
                              (p * RARCH_FIRST_CUSTOM_BIND) + retro_id, 0, 0))
                        count++;
                  }
               }
            }
         }

         ret                = 0;

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_OPTIONS_OVERRIDES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         {
            settings_t      *settings     = config_get_ptr();
            if (settings->bools.quick_menu_show_save_core_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (settings->bools.quick_menu_show_save_content_dir_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (settings->bools.quick_menu_show_save_game_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
                     msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
                     MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
                     MENU_SETTING_ACTION, 0, 0))
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
      case DISPLAYLIST_SHADER_PRESET_REMOVE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_GLOBAL))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_CORE))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_PARENT))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_GAME))
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_PRESETS_FOUND),
                  MENU_ENUM_LABEL_NO_PRESETS_FOUND,
                  0, 0, 0);
#endif
         }

         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PRESET_SAVE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#endif
         }
         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_SHADER_PARAMETERS:
      case DISPLAYLIST_SHADER_PARAMETERS_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            video_shader_ctx_t shader_info;

            if (video_shader_driver_get_current_shader(&shader_info))
            {
               unsigned i;
               struct video_shader *shader = shader_info.data;
               size_t list_size            = shader ? shader->num_parameters : 0;
               unsigned     base_parameter = (type == DISPLAYLIST_SHADER_PARAMETERS)
                  ? MENU_SETTINGS_SHADER_PARAMETER_0
                  : MENU_SETTINGS_SHADER_PRESET_PARAMETER_0;

               for (i = 0; i < list_size; i++)
                  if (menu_entries_append_enum(info->list, shader->parameters[i].desc,
                           info->label, MENU_ENUM_LABEL_SHADER_PARAMETERS_ENTRY,
                           base_parameter + i, 0, 0))
                     count++;
            }
         }
#endif

         if (count == 0)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_SHADER_PARAMETERS),
                  MENU_ENUM_LABEL_NO_SHADER_PARAMETERS,
                  0, 0, 0);

         ret                = 0;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            settings_t      *settings      = config_get_ptr();
            rarch_system_info_t *sys_info  = runloop_get_system_info();

            if (rarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
                  if (menu_displaylist_parse_settings_enum(info->list,
                           MENU_ENUM_LABEL_CONTENT_SETTINGS,
                           PARSE_ACTION, false) == 0)
                     count++;
            }
            else
            {
               if (sys_info && sys_info->load_no_content)
                  if (menu_displaylist_parse_settings_enum(info->list,
                           MENU_ENUM_LABEL_START_CORE, PARSE_ACTION, false) == 0)
                     count++;

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     if (menu_displaylist_parse_settings_enum(info->list,
                              MENU_ENUM_LABEL_CORE_LIST, PARSE_ACTION, false) == 0)
                        count++;
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               const struct retro_subsystem_info* subsystem = subsystem_data;
               /* Core not loaded completely, use the data we
                * peeked on load core */

               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;

               /* Core fully loaded, use the subsystem data */
               if (sys_info && sys_info->subsystem.data)
                     subsystem = sys_info->subsystem.data;

               menu_subsystem_populate(subsystem, info);
            }

            if (settings->bools.menu_content_show_history)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (settings->bools.menu_show_load_disc)
            {
               if (menu_displaylist_parse_settings_enum(info->list,
                        MENU_ENUM_LABEL_LOAD_DISC,
                        PARSE_ACTION, false) == 0)
                  count++;
            }

            if (settings->bools.menu_show_dump_disc)
            {
               if (menu_displaylist_parse_settings_enum(info->list,
                        MENU_ENUM_LABEL_DUMP_DISC,
                        PARSE_ACTION, false) == 0)
                  count++;
            }

            if (string_is_equal(settings->arrays.menu_driver, "rgui") &&
#ifndef HAVE_LIBRETRODB
                settings->bools.menu_show_advanced_settings &&
#endif
                settings->bools.menu_content_show_playlists)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_add)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_content_show_netplay)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_NETPLAY,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_show_online_updater)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (menu_displaylist_parse_settings_enum(info->list,
                  MENU_ENUM_LABEL_SETTINGS, PARSE_ACTION, false) == 0)
               count++;
            if (settings->bools.menu_show_information)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_show_configurations)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_show_help)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (settings->bools.menu_show_restart_retroarch)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (settings->bools.menu_show_quit_retroarch)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
               PARSE_ACTION, false) == 0)
               count++;

            if (menu_displaylist_parse_settings_enum(info->list,
               MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL,
               PARSE_ACTION, false) == 0)
               count++;

            if (settings->bools.menu_show_reboot)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_show_shutdown)
               if (menu_displaylist_parse_settings_enum(info->list,
                     MENU_ENUM_LABEL_SHUTDOWN,
                     PARSE_ACTION, false) == 0)
                  count++;

            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_HELP:
         if (menu_entries_append_enum(info->list, info->path,
               info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0))
            count++;
         menu_dialog_unset_pending_push();
         break;
      case DISPLAYLIST_HELP_SCREEN_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS),
               MENU_ENUM_LABEL_HELP_CONTROLS,
               0, 0, 0))
            count++;
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO),
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO),
               MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO,
               0, 0, 0))
            count++;

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_BROWSE_URL_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL),
               MENU_ENUM_LABEL_BROWSE_URL,
               0, 0, 0))
            count++;
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_START),
               msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_START),
               MENU_ENUM_LABEL_BROWSE_START,
               0, 0, 0))
            count++;

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

            if (menu_entries_append_enum(info->list,
                  link,
                  name,
                  MSG_UNKNOWN,
                  0, 0, 0))
               count++;
         }
#endif

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_INFO:
         if (menu_entries_append_enum(info->list, info->path,
               info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0))
            count++;
         break;
      case DISPLAYLIST_FILE_BROWSER_SCAN_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_FILE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_CORE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_SIDELOAD_CORE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION:
      case DISPLAYLIST_GENERIC:
         {
            menu_ctx_list_t list_info;

            list_info.type    = MENU_LIST_PLAIN;
            list_info.action  = 0;

            menu_driver_list_cache(&list_info);

            if (menu_entries_append_enum(info->list, info->path,
                  info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0))
               count++;

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

            if (menu_entries_append_enum(info->list, info->path,
                  info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0))
               count++;

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
            ret = menu_displaylist_parse_settings(info->list,
                  lbl, PARSE_NONE, true, MENU_SETTINGS_INPUT_BEGIN);
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASES:
         {
            settings_t *settings      = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            filebrowser_clear_type();
            if (!string_is_empty(info->exts))
               free(info->exts);
            if (info->path)
               free(info->path);
            info->type_default = FILE_TYPE_RDB;
            info->exts         = strdup(".rdb");
            info->enum_idx     = MENU_ENUM_LABEL_PLAYLISTS_TAB;
            load_content       = false;
            use_filebrowser    = true;
            info->path         = strdup(settings->paths.path_content_database);
         }
         break;
      case DISPLAYLIST_DATABASE_CURSORS:
         {
            settings_t *settings      = config_get_ptr();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            filebrowser_clear_type();
            if (!string_is_empty(info->exts))
               free(info->exts);
            if (info->path)
               free(info->path);
            info->type_default = FILE_TYPE_CURSOR;
            info->exts         = strdup("dbc");
            load_content       = false;
            use_filebrowser    = true;
            info->path         = strdup(settings->paths.directory_cursor);
         }
         break;
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            char new_exts[PATH_MAX_LENGTH];
            union string_list_elem_attr attr;
            struct string_list *str_list    = string_list_new();

            attr.i = 0;

            new_exts[0] = '\0';

            filebrowser_clear_type();

            if      (type == DISPLAYLIST_SHADER_PRESET)
               info->type_default = FILE_TYPE_SHADER_PRESET;
            else if (type == DISPLAYLIST_SHADER_PASS)
               info->type_default = FILE_TYPE_SHADER;

            if (video_shader_is_supported(RARCH_SHADER_CG))
            {
               if (type == DISPLAYLIST_SHADER_PRESET)
                  string_list_append(str_list, "cgp", attr);
               else if (type == DISPLAYLIST_SHADER_PASS)
                  string_list_append(str_list, "cg", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_GLSL))
            {
               if (type == DISPLAYLIST_SHADER_PRESET)
                  string_list_append(str_list, "glslp", attr);
               else if (type == DISPLAYLIST_SHADER_PASS)
                  string_list_append(str_list, "glsl", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_SLANG))
            {
               if (type == DISPLAYLIST_SHADER_PRESET)
                  string_list_append(str_list, "slangp", attr);
               else if (type == DISPLAYLIST_SHADER_PASS)
                  string_list_append(str_list, "slang", attr);
            }

            string_list_join_concat(new_exts, sizeof(new_exts), str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
            string_list_free(str_list);
            use_filebrowser    = true;
         }
#endif
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

            (void)attr;

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
            string_list_join_concat(new_exts,
                  sizeof(new_exts), str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
            string_list_free(str_list);
         }
         use_filebrowser    = true;
         break;
      case DISPLAYLIST_PLAYLIST:
         {
            settings_t *settings      = config_get_ptr();

            menu_displaylist_parse_playlist_generic(menu, info,
                  path_basename(info->path),
                  info->path,
                  true, /* Is a collection */
                  settings->bools.playlist_sort_alphabetical,
                  &ret);
            ret = 0; /* Why do we do this...? */

            if (ret == 0)
            {
               info->need_sort    = settings->bools.playlist_sort_alphabetical;
               info->need_refresh = true;
               info->need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_IMAGES_HISTORY:
         info->count           = 0;
#ifdef HAVE_IMAGEVIEWER
         {
            settings_t *settings      = config_get_ptr();
            if (settings->bools.history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     "images_history",
                     settings->paths.path_content_image_history,
                     false, /* Not a collection */
                     false, /* Do not sort */
                     &ret);
               count++;
            }
         }
#endif
         if (info->count == 0)
         {
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_IMAGES_AVAILABLE),
                  MENU_ENUM_LABEL_NO_IMAGES_AVAILABLE,
                  MENU_INFO_MESSAGE, 0, 0);
            info->need_push_no_playlist_entries = false;
            ret = 0;
         }

         ret                         = 0;
         info->need_refresh          = true;
         info->need_push             = true;
#if !defined(IS_SALAMANDER) && defined(HAVE_IMAGEVIEWER)
         info->need_navigation_clear =
               history_needs_navigation_clear(menu, g_defaults.image_history);
#endif

         break;
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_CONFIG_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_RGUI_THEME_PRESETS:
      case DISPLAYLIST_STREAM_CONFIG_FILES:
      case DISPLAYLIST_RECORD_CONFIG_FILES:
      case DISPLAYLIST_OVERLAYS:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_CHEAT_FILES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         if (!string_is_empty(info->exts))
            free(info->exts);
         switch (type)
         {
            case DISPLAYLIST_VIDEO_FILTERS:
               info->type_default = FILE_TYPE_VIDEOFILTER;
               info->exts         = strdup("filt");
               break;
            case DISPLAYLIST_CONFIG_FILES:
               info->type_default = FILE_TYPE_CONFIG;
               info->exts         = strdup("cfg");
               break;
            case DISPLAYLIST_REMAP_FILES:
               info->type_default    = FILE_TYPE_REMAP;
               info->exts            = strdup("rmp");
               break;
            case DISPLAYLIST_RGUI_THEME_PRESETS:
               info->type_default = FILE_TYPE_RGUI_THEME_PRESET;
               info->exts         = strdup("cfg");
               break;
            case DISPLAYLIST_STREAM_CONFIG_FILES:
               info->type_default = FILE_TYPE_STREAM_CONFIG;
               info->exts         = strdup("cfg");
               break;
            case DISPLAYLIST_RECORD_CONFIG_FILES:
               info->type_default = FILE_TYPE_RECORD_CONFIG;
               info->exts         = strdup("cfg");
               break;
            case DISPLAYLIST_OVERLAYS:
               info->type_default = FILE_TYPE_OVERLAY;
               info->exts         = strdup("cfg");
               break;
            case DISPLAYLIST_FONTS:
               info->type_default = FILE_TYPE_FONT;
               info->exts         = strdup("ttf");
               break;
            case DISPLAYLIST_AUDIO_FILTERS:
               info->type_default = FILE_TYPE_AUDIOFILTER;
               info->exts         = strdup("dsp");
               break;
            case DISPLAYLIST_CHEAT_FILES:
               info->type_default = FILE_TYPE_CHEAT;
               info->exts         = strdup("cht");
               break;
            default:
               break;
         }
         load_content       = false;
         use_filebrowser    = true;
         break;
#ifdef HAVE_VIDEO_LAYOUT
      case DISPLAYLIST_VIDEO_LAYOUT_PATH:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_VIDEO_LAYOUT;
         load_content       = false;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("lay|zip");
         break;
#endif
      case DISPLAYLIST_CONTENT_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         filebrowser_clear_type();
         info->type_default = FILE_TYPE_PLAIN;
         use_filebrowser    = true;
         if (!string_is_empty(info->exts))
            free(info->exts);
         info->exts         = strdup("lpl");
         break;
      case DISPLAYLIST_DATABASE_PLAYLISTS:
      case DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL:
         {
            bool is_horizontal =
               (type == DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL);

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count =  menu_displaylist_parse_playlists(info, is_horizontal);

            if (count == 0 && !is_horizontal)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_PLAYLISTS),
                     MENU_ENUM_LABEL_NO_PLAYLISTS,
                     MENU_SETTING_NO_ITEM, 0, 0);

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
            if (frontend_driver_get_core_extension(
                     ext_name, sizeof(ext_name)))
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
         if (string_is_equal(info->label,
                  msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
            info->push_builtin_cores = true;
         break;
      case DISPLAYLIST_DEFAULT:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         load_content    = false;
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
                  const char *val                 = NULL;

                  rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

                  if (coreopts)
                  {
                     settings_t *settings            = config_get_ptr();
                     unsigned size                   = (unsigned)tmp_str_list->size;
                     unsigned menu_index             = atoi(tmp_str_list->elems[size-1].data);
                     unsigned visible_index          = 0;
                     unsigned option_index           = 0;
                     bool option_found               = false;
                     struct core_option *option      = NULL;
                     bool checked_found              = false;
                     unsigned checked                = 0;
                     unsigned i;

                     /* Note: Although we display value labels here,
                      * most logic is performed using values. This seems
                      * more appropriate somehow... */

                     /* Convert menu index to option index */
                     if (settings->bools.game_specific_options)
                        menu_index--;

                     for (i = 0; i < coreopts->size; i++)
                     {
                        if (core_option_manager_get_visible(coreopts, i))
                        {
                           if (visible_index == menu_index)
                           {
                              option_found = true;
                              option_index = i;
                              break;
                           }
                           visible_index++;
                        }
                     }

                     if (option_found)
                     {
                        val    = core_option_manager_get_val(coreopts, option_index);
                        option = (struct core_option*)&coreopts->opts[option_index];
                     }

                     if (option)
                     {
                        unsigned k;
                        for (k = 0; k < option->vals->size; k++)
                        {
                           const char *val_str       = option->vals->elems[k].data;
                           const char *val_label_str = option->val_labels->elems[k].data;

                           if (!string_is_empty(val_label_str))
                           {
                              char val_d[256];
                              snprintf(val_d, sizeof(val_d), "%d", option_index);

                              if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)))
                                 val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON);
                              else if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)))
                                 val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);

                              menu_entries_append_enum(info->list,
                                    val_label_str,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM, k, 0);

                              if (!checked_found && string_is_equal(val_str, val))
                              {
                                 checked = k;
                                 checked_found = true;
                              }

                              count++;
                           }
                        }

                        if (checked_found)
                           menu_entries_set_checked(info->list, checked, true);
                     }
                  }
               }

               if (tmp_str_list)
                  string_list_free(tmp_str_list);
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
                              unsigned size        = (unsigned)tmp_str_list->size;
                              bool checked_found   = false;
                              unsigned checked     = 0;

                              for (i = 0; i < size; i++)
                              {
                                 char val_d[256];
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 if (menu_entries_append_enum(info->list,
                                       tmp_str_list->elems[i].data,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM, i, 0))
                                    count++;

                                 if (!checked_found && string_is_equal(tmp_str_list->elems[i].data, setting->value.target.string))
                                 {
                                    checked = i;
                                    checked_found = true;
                                 }
                              }

                              if (checked_found)
                                 menu_entries_set_checked(info->list, checked, true);
                           }

                           if (tmp_str_list)
                              string_list_free(tmp_str_list);
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
                           bool checked_found     = false;
                           unsigned checked       = 0;

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

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
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

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
                              }
                           }

                           if (checked_found)
                              menu_entries_set_checked(info->list, checked, true);
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
                           bool checked_found     = false;
                           unsigned checked       = 0;

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

                                 if (!checked_found && fabs(i - orig_value) <= 0.01f)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
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

                                 if (!checked_found && fabs(i - orig_value) <= 0.01f)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
                              }
                           }

                           if (checked_found)
                              menu_entries_set_checked(info->list, checked, true);
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
                           bool checked_found     = false;
                           unsigned checked       = 0;

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

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
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

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = count;
                                    checked_found = true;
                                 }
                                 count++;
                              }
                           }

                           if (checked_found)
                              menu_entries_set_checked(
                                    info->list, checked, true);
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
      case DISPLAYLIST_DROPDOWN_LIST_SPECIAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (strstr(info->path, "core_option_"))
         {
            struct string_list *tmp_str_list = string_split(info->path, "_");

            if (tmp_str_list && tmp_str_list->size > 0)
            {
               core_option_manager_t *coreopts = NULL;
               const char *val                 = NULL;

               rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

               if (coreopts)
               {
                  unsigned size                   = (unsigned)tmp_str_list->size;
                  unsigned menu_index             = atoi(tmp_str_list->elems[size-1].data);
                  unsigned visible_index          = 0;
                  unsigned option_index           = 0;
                  bool option_found               = false;
                  struct core_option *option      = NULL;
                  bool checked_found              = false;
                  unsigned checked                = 0;
                  unsigned i;

                  /* Note: Although we display value labels here,
                   * most logic is performed using values. This seems
                   * more appropriate somehow... */

                  /* Convert menu index to option index */
                  menu_index--;

                  for (i = 0; i < coreopts->size; i++)
                  {
                     if (core_option_manager_get_visible(coreopts, i))
                     {
                        if (visible_index == menu_index)
                        {
                           option_found = true;
                           option_index = i;
                           break;
                        }
                        visible_index++;
                     }
                  }

                  if (option_found)
                  {
                     val    = core_option_manager_get_val(coreopts, option_index);
                     option = (struct core_option*)&coreopts->opts[option_index];
                  }

                  if (option)
                  {
                     unsigned k;
                     for (k = 0; k < option->vals->size; k++)
                     {
                        const char *val_str       = option->vals->elems[k].data;
                        const char *val_label_str = option->val_labels->elems[k].data;

                        if (!string_is_empty(val_label_str))
                        {
                           char val_d[256];
                           snprintf(val_d, sizeof(val_d), "%d", option_index);

                           if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)))
                              val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON);
                           else if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)))
                              val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);

                           menu_entries_append_enum(info->list,
                                 val_label_str,
                                 val_d,
                                 MENU_ENUM_LABEL_NO_ITEMS,
                                 MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM_SPECIAL, k, 0);

                           if (!checked_found && string_is_equal(val_str, val))
                           {
                              checked = k;
                              checked_found = true;
                           }
                        }
                     }

                     if (checked_found)
                        menu_entries_set_checked(info->list, checked, true);
                  }
               }
            }

            if (tmp_str_list)
               string_list_free(tmp_str_list);
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
                           unsigned size        = (unsigned)tmp_str_list->size;
                           bool checked_found   = false;
                           unsigned checked     = 0;

                           for (i = 0; i < size; i++)
                           {
                              char val_d[256];
                              snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                              menu_entries_append_enum(info->list,
                                    tmp_str_list->elems[i].data,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM_SPECIAL, i, 0);

                              if (!checked_found && string_is_equal(tmp_str_list->elems[i].data, setting->value.target.string))
                              {
                                 checked = i;
                                 checked_found = true;
                              }
                           }

                           if (checked_found)
                              menu_entries_set_checked(info->list, checked, true);
                        }

                        if (tmp_str_list)
                           string_list_free(tmp_str_list);
                     }
                     break;
                  case ST_INT:
                     {
                        float i;
                        int32_t orig_value     = *setting->value.target.integer;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_INT_ITEM_SPECIAL;
                        float step             = setting->step;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;

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

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }
                              count++;
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

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }

                              count++;
                           }
                        }

                        if (checked_found)
                           menu_entries_set_checked(info->list, checked, true);
                     }
                     break;
                  case ST_FLOAT:
                     {
                        float i;
                        float orig_value       = *setting->value.target.fraction;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM_SPECIAL;
                        float step             = setting->step;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;

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

                              if (!checked_found && fabs(i - orig_value) <= 0.01f)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }
                              count++;
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

                              if (!checked_found && fabs(i - orig_value) <= 0.01f)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }
                              count++;
                           }
                        }

                        if (checked_found)
                           menu_entries_set_checked(info->list, checked, true);
                     }
                     break;
                  case ST_UINT:
                     {
                        float i;
                        unsigned orig_value    = *setting->value.target.unsigned_integer;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL;
                        float step             = setting->step;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;

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

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }
                              count++;
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

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = count;
                                 checked_found = true;
                              }
                              count++;
                           }
                        }

                        if (checked_found)
                           menu_entries_set_checked(info->list, checked, true);
                     }
                     break;
                  default:
                     break;
               }
            }
         }

         info->need_refresh       = true;
         info->need_push          = true;
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
