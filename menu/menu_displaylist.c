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

#include <array/rbuf.h>
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
#include "../cheevos/cheevos.h"
#include "../cheevos/cheevos_menu.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_http_parse.h>
#include "../../network/netplay/netplay.h"
#include "../core_updater_list.h"
#endif

#ifdef HAVE_LAKKA_SWITCH
#include "../../lakka.h"
#endif

#ifdef HAVE_LIBNX
#include <switch.h>
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_LIBNX)
#include "../../switch_performance_profiles.h"
#endif

#if defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../frontend/drivers/platform_unix.h"
#endif

#if defined(ANDROID)
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#include <media/media_detect_cd.h>
#endif

#include "../audio/audio_driver.h"
#include "../record/record_driver.h"
#include "menu_cbs.h"
#include "menu_driver.h"
#include "menu_entries.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu_shader.h"
#endif
#include "menu_dialog.h"
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
#include "menu_screensaver.h"
#endif

#include "../configuration.h"
#include "../file_path_special.h"
#include "../defaults.h"
#include "../verbosity.h"
#ifdef HAVE_CHEATS
#include "../cheat_manager.h"
#endif
#include "../core_option_manager.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../core.h"
#include "../frontend/frontend_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../gfx/video_display_server.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif
#include "../config.features.h"
#include "../version_git.h"
#include "../list_special.h"
#include "../performance_counters.h"
#include "../core_info.h"
#include "../bluetooth/bluetooth_driver.h"
#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
#include "../network/wifi_driver.h"
#endif
#include "../tasks/task_content.h"
#include "../tasks/tasks_internal.h"
#include "../dynamic.h"
#include "../runtime_file.h"
#include "../manual_content_scan.h"
#include "../core_backup.h"
#include "../misc/cpufreq/cpufreq.h"
#include "../input/input_remapping.h"

#ifdef HAVE_MIST
#include "../steam/steam.h"
#endif

/* Spacers used for '<content> - <core name>' labels
 * in playlists */
#define PL_LABEL_SPACER_DEFAULT "   |   "
#define PL_LABEL_SPACER_RGUI    " | "
#define PL_LABEL_SPACER_MAXLEN  8

#define BYTES_TO_MB(bytes) ((bytes) / 1024 / 1024)
#define BYTES_TO_GB(bytes) (((bytes) / 1024) / 1024 / 1024)

#ifdef HAVE_NETWORKING
#if !defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(SWITCH) && defined(HAVE_LIBNX)) || defined(GEKKO)
#include <net/net_ifinfo.h>
#endif
#endif

/* TODO/FIXME - globals - need to find a way to
 * get rid of these */
struct menu_displaylist_state
{
   enum filebrowser_enums filebrowser_types;
};

static struct menu_displaylist_state menu_displist_st = {
   FILEBROWSER_NONE /* filebrowser_types */
};

extern struct key_desc key_descriptors[RARCH_MAX_KEYS];

enum filebrowser_enums filebrowser_get_type(void)
{
   struct menu_displaylist_state *p_displist = &menu_displist_st;
   return p_displist->filebrowser_types;
}

void filebrowser_clear_type(void)
{
   struct menu_displaylist_state *p_displist = &menu_displist_st;
   p_displist->filebrowser_types = FILEBROWSER_NONE;
}

void filebrowser_set_type(enum filebrowser_enums type)
{
   struct menu_displaylist_state *p_displist = &menu_displist_st;
   p_displist->filebrowser_types = type;
}

static void filebrowser_parse(
      menu_displaylist_info_t *info,
      unsigned type_data,
      bool show_hidden_files,
      bool builtin_mediaplayer_enable,
      bool builtin_imageviewer_enable,
      bool filter_ext
      )
{
   size_t i, list_size;
   const struct retro_subsystem_info *subsystem = NULL;
   bool ret                                     = false;
   struct string_list str_list                  = {0};
   unsigned items_found                         = 0;
   enum menu_displaylist_ctl_state type         = (enum menu_displaylist_ctl_state)type_data;
   enum filebrowser_enums filebrowser_type      = filebrowser_get_type();
   const char *path                             = info->path;
   bool path_is_compressed                      = !string_is_empty(path) ?
         path_is_compressed_file(path) : false;
   menu_search_terms_t *search_terms            = menu_entries_search_get_terms();

   if (path_is_compressed)
   {
      if (filebrowser_type == FILEBROWSER_SELECT_FILE_SUBSYSTEM)
      {
         runloop_state_t *runloop_st          = runloop_state_get_ptr();
         rarch_system_info_t *system          = &runloop_st->system;
         /* Core fully loaded, use the subsystem data */
         if (system->subsystem.data)
            subsystem = system->subsystem.data + content_get_subsystem();
         /* Core not loaded completely, use the data we peeked on load core */
         else
            subsystem = runloop_st->subsystem_data + content_get_subsystem();

         if (subsystem && runloop_st->subsystem_current_count > 0)
            ret = file_archive_get_file_list_noalloc(&str_list,
                  path,
                  subsystem->roms[
                  content_get_subsystem_rom_id()].valid_extensions);
      }
      else
         ret = file_archive_get_file_list_noalloc(&str_list,
               path, info->exts);
   }
   else if (!string_is_empty(path))
   {
      if (info->type_default == FILE_TYPE_SHADER_PRESET ||
            info->type_default == FILE_TYPE_SHADER)
         filter_ext = true;

      if (string_is_equal(info->label,
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)))
         filter_ext = false;

      if (filebrowser_type == FILEBROWSER_SELECT_FILE_SUBSYSTEM)
      {
         runloop_state_t *runloop_st = runloop_state_get_ptr();
         rarch_system_info_t *system = &runloop_st->system;
         /* Core fully loaded, use the subsystem data */
         if (system->subsystem.data)
            subsystem = system->subsystem.data + content_get_subsystem();
         /* Core not loaded completely, use the data we peeked on load core */
         else
            subsystem = runloop_st->subsystem_data + content_get_subsystem();

         if (subsystem && runloop_st->subsystem_current_count > 0 && content_get_subsystem_rom_id() < subsystem->num_roms)
            ret = dir_list_initialize(&str_list,
                  path,
                  filter_ext ? subsystem->roms[content_get_subsystem_rom_id()].valid_extensions : NULL,
                  true, show_hidden_files, true, false);
      }
      else if ((info->type_default == FILE_TYPE_MANUAL_SCAN_DAT) || (info->type_default == FILE_TYPE_SIDELOAD_CORE))
         ret = dir_list_initialize(&str_list, path,
               info->exts, true, show_hidden_files, false, false);
      else
         ret = dir_list_initialize(&str_list, path,
               filter_ext ? info->exts : NULL,
               true, show_hidden_files, true, false);
   }

   switch (filebrowser_type)
   {
      case FILEBROWSER_SCAN_DIR:
#ifdef HAVE_LIBRETRODB
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY),
               MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY,
               FILE_TYPE_SCAN_DIRECTORY, 0 ,0);
#endif
         break;
      case FILEBROWSER_MANUAL_SCAN_DIR:
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY),
               MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY,
               FILE_TYPE_MANUAL_SCAN_DIRECTORY, 0 ,0);
         break;
      case FILEBROWSER_SELECT_DIR:
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_USE_THIS_DIRECTORY),
               MENU_ENUM_LABEL_USE_THIS_DIRECTORY,
               FILE_TYPE_USE_DIRECTORY, 0 ,0);
         break;
      default:
         break;
   }

   if (!ret)
   {
      const char *str = path_is_compressed
         ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      menu_entries_append_enum(info->list, str, "",
            MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND, 0, 0, 0);
      goto end;
   }

   dir_list_sort(&str_list, true);

   list_size = str_list.size;

   if (list_size > 0)
   {
      for (i = 0; i < list_size; i++)
      {
         enum msg_hash_enums enum_idx      = MSG_UNKNOWN;
         enum rarch_content_type path_type = RARCH_CONTENT_NONE;
         enum msg_file_type file_type      = FILE_TYPE_NONE;
         const char *file_path             = str_list.elems[i].data;

         if (string_is_empty(file_path))
            continue;

         if ((str_list.elems[i].attr.i != RARCH_DIRECTORY) &&
             ((filebrowser_type == FILEBROWSER_SELECT_DIR) ||
              (filebrowser_type == FILEBROWSER_SCAN_DIR)   ||
              (filebrowser_type == FILEBROWSER_MANUAL_SCAN_DIR)))
            continue;

         if (!path_is_compressed)
         {
            file_path = path_basename_nocompression(file_path);
            if (string_is_empty(file_path))
               continue;
         }

         /* Check whether entry matches search terms,
          * if required */
         if (search_terms)
         {
            bool skip_entry = false;
            size_t j;

            for (j = 0; j < search_terms->size; j++)
            {
               const char *search_term = search_terms->terms[j];

               if (!string_is_empty(search_term) &&
                   !strcasestr(file_path, search_term))
               {
                  skip_entry = true;
                  break;
               }
            }

            if (skip_entry)
               continue;
         }

         switch (str_list.elems[i].attr.i)
         {
            case RARCH_DIRECTORY:
               file_type = FILE_TYPE_DIRECTORY;
               items_found++;
               menu_entries_append_enum(info->list, file_path, "",
                     MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY,
                     file_type, 0, 0);
               continue;
            case RARCH_COMPRESSED_ARCHIVE:
               file_type = FILE_TYPE_CARCHIVE;
               break;
            case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
               file_type = FILE_TYPE_IN_CARCHIVE;
               break;
            case RARCH_PLAIN_FILE:
            default:
               if (filebrowser_type == FILEBROWSER_SELECT_VIDEO_FONT)
                  file_type = FILE_TYPE_VIDEO_FONT;
               else
                  file_type = (enum msg_file_type)info->type_default;
               switch (type)
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  case DISPLAYLIST_CORES_DETECTED:
                     if (path_is_compressed_file(file_path))
                        file_type = FILE_TYPE_CARCHIVE;
                     break;
                  default:
                     break;
               }
               break;
         }

         path_type = path_is_media_type(file_path);

         if (filebrowser_type == FILEBROWSER_SELECT_COLLECTION)
            file_type = FILE_TYPE_PLAYLIST_COLLECTION;

         if (path_type == RARCH_CONTENT_MUSIC)
            file_type = FILE_TYPE_MUSIC;
         else if (builtin_mediaplayer_enable ||
                  builtin_imageviewer_enable)
         {
            switch (path_type)
            {
               case RARCH_CONTENT_MOVIE:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
                  if (builtin_mediaplayer_enable)
                     file_type = FILE_TYPE_MOVIE;
#endif
                  break;
               case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
                  if (builtin_imageviewer_enable
                        && type != DISPLAYLIST_IMAGES)
                     file_type = FILE_TYPE_IMAGEVIEWER;
                  else
                     file_type = FILE_TYPE_IMAGE;
#endif
                  if (filebrowser_type == FILEBROWSER_SELECT_IMAGE)
                     file_type = FILE_TYPE_IMAGE;
                  break;
               default:
                  break;
            }
         }

         switch (file_type)
         {
            case FILE_TYPE_MOVIE:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN;
               break;
            case FILE_TYPE_MUSIC:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN;
               break;
            case FILE_TYPE_IMAGE:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_IMAGE;
               break;
            case FILE_TYPE_IMAGEVIEWER:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER;
               break;
            case FILE_TYPE_PLAIN:
            default:
               break;
         }

         items_found++;
         menu_entries_append_enum(info->list, file_path, "",
               enum_idx, file_type, 0, 0);
      }
   }

   dir_list_deinitialize(&str_list);

   if (items_found == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
            MENU_ENUM_LABEL_NO_ITEMS,
            MENU_SETTING_NO_ITEM, 0, 0);
   }

end:
   if (!path_is_compressed)
      menu_entries_prepend(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY),
            path,
            MENU_ENUM_LABEL_PARENT_DIRECTORY,
            FILE_TYPE_PARENT_DIRECTORY, 0, 0);
}

static int menu_displaylist_parse_core_info(menu_displaylist_info_t *info,
      settings_t *settings)
{
   char tmp[PATH_MAX_LENGTH];
   unsigned i, count             = 0;
   core_info_t *core_info        = NULL;
   const char *core_path         = NULL;
   const char *savestate_support = NULL;
   bool kiosk_mode_enable        = settings->bools.kiosk_mode_enable;
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#if defined(HAVE_NETWORKING) && defined(HAVE_ONLINE_UPDATER)
   bool menu_show_core_updater   = settings->bools.menu_show_core_updater;
#endif
#endif
#if defined(HAVE_DYNAMIC)
   enum menu_contentless_cores_display_type
         contentless_display_type = (enum menu_contentless_cores_display_type)
               settings->uints.menu_content_show_contentless_cores;
#endif

   tmp[0] = '\0';

   /* Check whether we are parsing information for a
    * core updater/manager entry or the currently loaded core */
   if ((info->type == FILE_TYPE_DOWNLOAD_CORE) ||
       (info->type == MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS))
   {
      core_info_t *core_info_menu = NULL;

      core_path = info->path;

      /* Core updater entry - search for corresponding
       * core info */
      if (core_info_find(core_path, &core_info_menu))
         core_info = core_info_menu;
   }
   else if (core_info_get_current_core(&core_info) && core_info)
      core_path = core_info->path;

   if (!core_info || !core_info->has_info)
   {
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE),
            MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE,
            0, 0, 0))
         count++;

      goto end;
   }

   {
      struct retro_system_info *system  = &runloop_state_get_ptr()->system.info;
      const char *core_version          = (system && system->library_version)
            ? system->library_version
            : "";
      unsigned i;
      typedef struct menu_features_info
      {
         const char *name;
         enum msg_hash_enums msg;
      } menu_features_info_t;

      menu_features_info_t info_list[] = {
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME},
         {NULL, MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER},
      };
      info_list[0].name = core_info->core_name;
      info_list[1].name = core_info->display_name;
      info_list[2].name = !string_is_empty(core_version) ? core_version : core_info->display_version;
      info_list[3].name = core_info->systemname;
      info_list[4].name = core_info->system_manufacturer;

      for (i = 0; i < ARRAY_SIZE(info_list); i++)
      {
         if (!info_list[i].name)
            continue;

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(info_list[i].msg),
               ": ",
               info_list[i].name,
               sizeof(tmp));
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
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
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   if (core_info->authors_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->authors_list, ", ");
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   if (core_info->permissions_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->permissions_list, ", ");
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   if (core_info->licenses_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->licenses_list, ", ");
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   if (core_info->supported_extensions_list)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->supported_extensions_list, ", ");
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   if (core_info->required_hw_api)
   {
      fill_pathname_noext(tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API),
            ": ",
            sizeof(tmp));
      string_list_join_concat(tmp, sizeof(tmp),
            core_info->required_hw_api_list, ", ");
      if (menu_entries_append_enum(info->list, tmp, "",
            MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   switch (core_info->savestate_support_level)
   {
      case CORE_INFO_SAVESTATE_BASIC:
         savestate_support = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC);
         break;
      case CORE_INFO_SAVESTATE_SERIALIZED:
         savestate_support = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED);
         break;
      case CORE_INFO_SAVESTATE_DETERMINISTIC:
         savestate_support = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC);
         break;
      default:
         if (core_info->savestate_support_level >
               CORE_INFO_SAVESTATE_DETERMINISTIC)
            savestate_support = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC);
         else
            savestate_support = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED);
         break;
   }
   fill_pathname_join_concat_noext(tmp,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL),
         ": ",
         savestate_support,
         sizeof(tmp));
   if (menu_entries_append_enum(info->list, tmp, "",
         MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

   if (core_info->firmware_count > 0)
   {
      core_info_ctx_firmware_t firmware_info;
      bool update_missing_firmware   = false;
      bool set_missing_firmware      = false;

      firmware_info.path             = core_info->path;
      firmware_info.directory.system = settings->paths.directory_system;

      retroarch_ctl(RARCH_CTL_UNSET_MISSING_BIOS, NULL);

      update_missing_firmware        = core_info_list_update_missing_firmware(&firmware_info, &set_missing_firmware);

      if (set_missing_firmware)
         retroarch_ctl(RARCH_CTL_SET_MISSING_BIOS, NULL);

      if (update_missing_firmware)
      {
         fill_pathname_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE),
               ": ",
               sizeof(tmp));

         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;

         /* FIXME: This looks hacky and probably
          * needs to be improved for good translation support. */

         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (!core_info->firmware[i].desc)
               continue;

            snprintf(tmp, sizeof(tmp), "(!) %s %s",
                  core_info->firmware[i].missing ?
                  (core_info->firmware[i].optional ?
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL) :
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED))
                  :
                  (core_info->firmware[i].optional ?
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL) :
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED)),
                  core_info->firmware[i].desc ?
                  core_info->firmware[i].desc :
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME)
                  );

            if (menu_entries_append_enum(info->list, tmp, "",
                  MENU_ENUM_LABEL_CORE_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
         }
      }
   }

   if (core_info->notes)
   {
      for (i = 0; i < core_info->note_list->size; i++)
      {
         strlcpy(tmp,
               core_info->note_list->elems[i].data, sizeof(tmp));
         if (menu_entries_append_enum(info->list, tmp, "",
               MENU_ENUM_LABEL_CORE_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }
   }

end:

#if defined(HAVE_DYNAMIC)
   /* Exclude core from contentless cores menu */
   if ((contentless_display_type ==
            MENU_CONTENTLESS_CORES_DISPLAY_CUSTOM) &&
       core_info &&
       core_info->supports_no_game &&
       !string_is_empty(core_path) &&
       !kiosk_mode_enable)
   {
      /* Note: Have to set core_path as both the
       * 'path' and 'label' parameters (otherwise
       * cannot access it in menu_cbs_get_value.c
       * or menu_cbs_left/right.c), which means
       * entry name must be set as 'alt' text */
      if (menu_entries_append_enum(info->list,
            core_path,
            core_path,
            MENU_ENUM_LABEL_CORE_SET_STANDALONE_EXEMPT,
            MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT, 0, 0))
      {
         file_list_set_alt_at_offset(
               info->list, count,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT));
         count++;
      }
   }
#endif

#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   if (!string_is_empty(core_path) && !kiosk_mode_enable)
   {
      /* Check whether core is currently locked */
      bool core_locked = core_info_get_core_lock(core_path, true);

#if defined(ANDROID)
      /* Play Store builds do not support
       * core locking */
      if (!play_feature_delivery_enabled())
#endif
      {
         /* Lock core
          * > Note: Have to set core_path as both the
          *   'path' and 'label' parameters (otherwise
          *   cannot access it in menu_cbs_get_value.c
          *   or menu_cbs_left/right.c), which means
          *   entry name must be set as 'alt' text */
         if (menu_entries_append_enum(info->list,
               core_path,
               core_path,
               MENU_ENUM_LABEL_CORE_LOCK,
               MENU_SETTING_ACTION_CORE_LOCK, 0, 0))
         {
            file_list_set_alt_at_offset(
                  info->list, count, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LOCK));
            count++;
         }
      }

      /* Backup core */
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP),
            core_path,
            MENU_ENUM_LABEL_CORE_CREATE_BACKUP,
            MENU_SETTING_ACTION_CORE_CREATE_BACKUP, 0, 0))
         count++;

      /* Restore core from backup */
      if (!core_locked)
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST),
               core_path,
               MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST,
               MENU_SETTING_ACTION_CORE_RESTORE_BACKUP, 0, 0))
            count++;

      /* Delete core backup */
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST),
            core_path,
            MENU_ENUM_LABEL_CORE_DELETE_BACKUP_LIST,
            MENU_SETTING_ACTION_CORE_DELETE_BACKUP, 0, 0))
         count++;

      /* Delete core
       * > Only add this option if online updater is
       *   enabled/activated, otherwise user could end
       *   up in a situation where a core cannot be
       *   restored */
#if defined(HAVE_NETWORKING) && defined(HAVE_ONLINE_UPDATER)
      if (menu_show_core_updater && !core_locked)
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_DELETE),
               core_path,
               MENU_ENUM_LABEL_CORE_DELETE,
               MENU_SETTING_ACTION_CORE_DELETE, 0, 0))
            count++;
#endif
   }
#endif

   return count;
}

static unsigned menu_displaylist_parse_core_backup_list(
      menu_displaylist_info_t *info,
      settings_t *settings,
      bool restore)
{
   enum msg_hash_enums enum_idx;
   enum menu_settings_type settings_type;
   unsigned count                  = 0;
   const char *core_path           = info->path;
   core_backup_list_t *backup_list = NULL;
   const char *dir_core_assets     = settings->paths.directory_core_assets;
   enum core_backup_date_separator_type
         date_separator            = (enum core_backup_date_separator_type)
               settings->uints.menu_timedate_date_separator;

   if (restore)
   {
      enum_idx      = MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_ENTRY;
      settings_type = MENU_SETTING_ITEM_CORE_RESTORE_BACKUP;
   }
   else
   {
      /* If we're not restoring, we're deleting */
      enum_idx      = MENU_ENUM_LABEL_CORE_DELETE_BACKUP_ENTRY;
      settings_type = MENU_SETTING_ITEM_CORE_DELETE_BACKUP;
   }

   /* Get backup list */
   backup_list = core_backup_list_init(core_path, dir_core_assets);

   if (backup_list)
   {
      size_t i;
      size_t menu_index = 0;

      for (i = 0; i < core_backup_list_size(backup_list); i++)
      {
         const core_backup_list_entry_t *entry = NULL;

         /* Ensure entry is valid */
         if (core_backup_list_get_index(backup_list, i, &entry) &&
             entry && !string_is_empty(entry->backup_path))
         {
            char timestamp[128];
            char crc[16];

            timestamp[0] = '\0';
            crc[0]       = '\0';

            /* Get timestamp and crc strings */
            core_backup_list_get_entry_timestamp_str(
                  entry, date_separator, timestamp, sizeof(timestamp));
            core_backup_list_get_entry_crc_str(
                  entry, crc, sizeof(crc));

            /* Append 'auto backup' tag to timestamp, if required */
            if (entry->backup_mode == CORE_BACKUP_MODE_AUTO)
            {
               strlcat(timestamp, " ", sizeof(timestamp));
               strlcat(timestamp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO),
                     sizeof(timestamp));
            }

            /* Add menu entry */
            if (menu_entries_append_enum(info->list,
                  timestamp,
                  entry->backup_path,
                  enum_idx,
                  settings_type, 0, 0))
            {
               /* We need to set backup path, timestamp and crc
                * > Only have 2 useable fields as standard
                *   ('path' and 'label'), so have to set the
                *   crc as 'alt' text */
               file_list_set_alt_at_offset(
                     info->list, menu_index, crc);

               menu_index++;
               count++;
            }
         }
      }

      core_backup_list_free(backup_list);
   }

   /* Fallback, in case no backups are found */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_CORE_BACKUPS_AVAILABLE),
            MENU_ENUM_LABEL_NO_CORE_BACKUPS_AVAILABLE,
            0, 0, 0))
         count++;

   return count;
}

static unsigned menu_displaylist_parse_core_manager_list(
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   unsigned count                   = 0;
   core_info_list_t *core_info_list = NULL;
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   bool kiosk_mode_enable           = settings->bools.kiosk_mode_enable;
#endif

   /* Get core list */
   core_info_get_list(&core_info_list);

   if (core_info_list)
   {
      menu_search_terms_t *search_terms= menu_entries_search_get_terms();
      core_info_t *core_info           = NULL;
      size_t menu_index                = 0;
      size_t i;

      /* Sort cores alphabetically */
      core_info_qsort(core_info_list, CORE_INFO_LIST_SORT_DISPLAY_NAME);

      /* Loop through cores */
      for (i = 0; i < core_info_list->count; i++)
      {
         core_info = NULL;
         core_info = core_info_get(core_info_list, i);

         if (core_info)
         {
            /* If a search is active, skip non-matching
             * entries */
            if (search_terms)
            {
               bool entry_valid = true;
               size_t j;

               for (j = 0; j < search_terms->size; j++)
               {
                  const char *search_term = search_terms->terms[j];

                  if (!string_is_empty(search_term) &&
                      !string_is_empty(core_info->display_name) &&
                      !strcasestr(core_info->display_name, search_term))
                  {
                     entry_valid = false;
                     break;
                  }
               }

               if (!entry_valid)
                  continue;
            }

            if (menu_entries_append_enum(info->list,
                     core_info->path,
                     "",
                     MENU_ENUM_LABEL_CORE_MANAGER_ENTRY,
                     MENU_SETTING_ACTION_CORE_MANAGER_OPTIONS,
                     0, 0))
            {
               file_list_set_alt_at_offset(
                     info->list, menu_index, core_info->display_name);

               menu_index++;
               count++;
            }
         }
      }
   }

   /* Add 'sideload core' entry */
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   if (!kiosk_mode_enable)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_SIDELOAD_CORE_LIST),
            MENU_ENUM_LABEL_SIDELOAD_CORE_LIST,
            MENU_SETTING_ACTION, 0, 0))
         count++;
#endif

   return count;
}

#ifdef HAVE_MIST
static unsigned menu_displaylist_parse_core_manager_steam_list(
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   MistResult result;
   steam_core_dlc_list_t *dlc_list;
   steam_core_dlc_t *dlc_info;
   size_t i;
   unsigned count = 0;

   result = steam_get_core_dlcs(&dlc_list, false);
   if (MIST_IS_ERROR(result)) goto error;

   for (i = 0; i < dlc_list->count; i++)
   {
      dlc_info = steam_core_dlc_list_get(dlc_list, i);

      if (menu_entries_append_enum(info->list,
            dlc_info->name,
            "",
            MENU_ENUM_LABEL_CORE_MANAGER_STEAM_ENTRY,
            MENU_SETTING_ACTION_CORE_MANAGER_STEAM_OPTIONS,
            0, 0))
         count++;
   }

   return count;
   
error:
   /* TODO: Send error notification */
   RARCH_ERR("[Steam] Error enumerating core dlcs for core manager (%d-%d)\n", MIST_UNPACK_RESULT(result));
   return count;
}

static unsigned menu_displaylist_parse_core_information_steam(
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   unsigned count = 0;
   MistResult result;
   steam_core_dlc_list_t *dlc_list;
   steam_core_dlc_t *core_dlc = NULL;
   bool installed = false;

   result = steam_get_core_dlcs(&dlc_list, false);
   if (MIST_IS_ERROR(result)) goto error;

   /* Get the core dlc information */
   core_dlc = steam_get_core_dlc_by_name(dlc_list, info->path);
   if (core_dlc == NULL) return count;
   
   /* Check if installed */
   result = mist_steam_apps_is_dlc_installed(core_dlc->app_id, &installed);
   if (MIST_IS_ERROR(result)) goto error;

   if (installed)
   {
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL),
            core_dlc->name,
            MENU_ENUM_LABEL_CORE_STEAM_UNINSTALL,
            MENU_SETTING_ACTION_CORE_STEAM_UNINSTALL,
            0, 0))
         count++;
   }
   else
   {
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL),
            core_dlc->name,
            MENU_ENUM_LABEL_CORE_STEAM_INSTALL,
            MENU_SETTING_ACTION_CORE_STEAM_INSTALL,
            0, 0))
         count++;
   }
   
   return count;
error:
   /* TODO: Send error notification */
   RARCH_ERR("[Steam] Error getting core information (%d-%d)\n", MIST_UNPACK_RESULT(result));
   return count;
}
#endif

static unsigned menu_displaylist_parse_core_option_dropdown_list(
      menu_displaylist_info_t *info)
{
   unsigned count                  = 0;
   struct string_list tmp_str_list = {0};
   unsigned option_index           = 0;
   unsigned checked                = 0;
   bool checked_found              = false;
   core_option_manager_t *coreopts = NULL;
   struct core_option *option      = NULL;
   const char *val                 = NULL;
   unsigned j;

   /* Fetch options */
   retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

   if (!coreopts)
      goto end;

   /* Path string has the format core_option_<opt_idx>
    * > Extract option index */
   if (string_is_empty(info->path))
      goto end;

   string_list_initialize(&tmp_str_list);
   string_split_noalloc(&tmp_str_list, info->path, "_");

   if (tmp_str_list.size < 1)
      goto end;

   option_index = string_to_unsigned(
         tmp_str_list.elems[tmp_str_list.size - 1].data);

   /* Get option itself + current value */
   option = (struct core_option*)&coreopts->opts[option_index];
   val    = core_option_manager_get_val(coreopts, option_index);

   if (!option ||
       string_is_empty(val))
      goto end;

   /* Loop over all option values */
   for (j = 0; j < option->vals->size; j++)
   {
      const char *val_str       = option->vals->elems[j].data;
      const char *val_label_str = option->val_labels->elems[j].data;

      if (!string_is_empty(val_label_str))
      {
         char val_d[256];

         val_d[0] = '\0';
         snprintf(val_d, sizeof(val_d), "%d", option_index);

         if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)))
            val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON);
         else if (string_is_equal(val_label_str, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)))
            val_label_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);

         if (menu_entries_append_enum(info->list,
               val_label_str,
               val_d,
               MENU_ENUM_LABEL_NO_ITEMS,
               MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM, j, 0))
            count++;

         if (!checked_found && string_is_equal(val_str, val))
         {
            checked       = j;
            checked_found = true;
         }
      }
   }

   if (checked_found)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            info->list->list[checked].actiondata;

      if (cbs)
         cbs->checked = true;

      menu_navigation_set_selection(checked);
   }

end:
   string_list_deinitialize(&tmp_str_list);
   return count;
}

static unsigned menu_displaylist_parse_core_option_override_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   unsigned count               = 0;
   bool core_has_options        = !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL) &&
         retroarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL);
   bool game_options_active     = retroarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL);
   bool folder_options_active   = retroarch_ctl(RARCH_CTL_IS_FOLDER_OPTIONS_ACTIVE, NULL);
   bool show_core_options_flush = settings ?
         settings->bools.quick_menu_show_core_options_flush : false;

   /* Sanity check - cannot handle core option
    * overrides if:
    * - Core is 'dummy'
    * - Core has no options
    * - No content has been loaded */
   if (!core_has_options ||
       string_is_empty(path_get(RARCH_PATH_CONTENT)))
      goto end;

   /* Show currently active core options file */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO),
         msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_INFO),
         MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_INFO,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

   /* Save core option overrides */
   if (!game_options_active)
   {
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE),
            msg_hash_to_str(MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE),
            MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
            MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_CREATE, 0, 0))
         count++;

      if (!folder_options_active)
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE),
               msg_hash_to_str(MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE),
               MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
               MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE, 0, 0))
            count++;
   }

   /* Remove core option overrides */
   if (game_options_active)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE),
            msg_hash_to_str(MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE),
            MENU_ENUM_LABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
            MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_REMOVE, 0, 0))
         count++;

   if (folder_options_active)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE),
            msg_hash_to_str(MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE),
            MENU_ENUM_LABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
            MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE, 0, 0))
         count++;
end:
   if (core_has_options)
   {
      /* Flush core options to disk */
      if (show_core_options_flush)
         if (menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH),
               MENU_ENUM_LABEL_CORE_OPTIONS_FLUSH,
               MENU_SETTING_ACTION_CORE_OPTIONS_FLUSH, 0, 0))
            count++;

      /* Reset core options */
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS_RESET),
            MENU_ENUM_LABEL_CORE_OPTIONS_RESET,
            MENU_SETTING_ACTION_CORE_OPTIONS_RESET, 0, 0))
         count++;
   }

   /* Fallback, in case we open this menu while running
    * a core without options */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND),
            MENU_ENUM_LABEL_NO_SETTINGS_FOUND,
            0, 0, 0))
         count++;

   return count;
}

static unsigned menu_displaylist_parse_remap_file_manager_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   unsigned count                = 0;
   bool has_content              = !string_is_empty(path_get(RARCH_PATH_CONTENT));
   bool core_remap_active        = retroarch_ctl(RARCH_CTL_IS_REMAPS_CORE_ACTIVE, NULL);
   bool content_dir_remap_active = retroarch_ctl(RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE, NULL);
   bool game_remap_active        = retroarch_ctl(RARCH_CTL_IS_REMAPS_GAME_ACTIVE, NULL);

   /* Sanity check - cannot handle remap files
    * unless a valid core is running */
   if (!retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL) ||
       retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      goto end;

   /* Show currently 'active' remap file */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_INFO),
         MENU_ENUM_LABEL_REMAP_FILE_INFO,
         MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

   /* Load remap file */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_LOAD),
         MENU_ENUM_LABEL_REMAP_FILE_LOAD,
         MENU_SETTING_ACTION_REMAP_FILE_LOAD, 0, 0))
      count++;

   /* Save remap files */
   if (has_content &&
       !game_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME),
            MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME,
            MENU_SETTING_ACTION_REMAP_FILE_SAVE_GAME, 0, 0))
      count++;

   if (has_content &&
       !game_remap_active &&
       !content_dir_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR),
            MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR,
            MENU_SETTING_ACTION_REMAP_FILE_SAVE_CONTENT_DIR, 0, 0))
      count++;

   if (!game_remap_active &&
       !content_dir_remap_active &&
       !core_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE),
            MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE,
            MENU_SETTING_ACTION_REMAP_FILE_SAVE_CORE, 0, 0))
      count++;

   /* Remove remap files */
   if (has_content &&
       game_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME,
            MENU_SETTING_ACTION_REMAP_FILE_REMOVE_GAME, 0, 0))
      count++;

   if (has_content &&
       content_dir_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
            MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CONTENT_DIR, 0, 0))
      count++;

   if (core_remap_active &&
       menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE),
            msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE),
            MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE,
            MENU_SETTING_ACTION_REMAP_FILE_REMOVE_CORE, 0, 0))
      count++;

   /* Reset input remaps */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET),
         msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_RESET),
         MENU_ENUM_LABEL_REMAP_FILE_RESET,
         MENU_SETTING_ACTION_REMAP_FILE_RESET, 0, 0))
      count++;
end:
   /* Fallback */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0))
         count++;

   return count;
}

static unsigned menu_displaylist_parse_supported_cores(menu_displaylist_info_t *info,
      settings_t *settings, const char *content_path,
      enum msg_hash_enums core_enum_label,
      enum msg_hash_enums current_core_enum_label)
{
   unsigned count                   = 0;
   core_info_list_t *core_info_list = NULL;
   bool core_available              = false;
   const char *core_path_current    = path_get(RARCH_PATH_CORE);
#if defined(HAVE_DYNAMIC)
   bool enable_load_with_current    = !string_is_empty(core_path_current);
#else
   bool enable_load_with_current    = !string_is_empty(core_path_current) &&
         !settings->bools.always_reload_core_on_run_content;
#endif

   /* Get core list */
   if (core_info_get_list(&core_info_list) &&
       core_info_list)
   {
      const core_info_t *core_infos = NULL;
      size_t core_infos_size        = 0;
      size_t core_idx               = 0;
      size_t entry_idx              = 0;
      const char *pending_core_path = NULL;
      const char *pending_core_name = NULL;
      bool core_is_pending          = false;

      /* Get list of supported cores */
      core_info_list_get_supported_cores(core_info_list,
            content_path, &core_infos, &core_infos_size);

      for (core_idx = 0; core_idx < core_infos_size; core_idx++)
      {
         const core_info_t *core_info = (const core_info_t*)&core_infos[core_idx];
         const char *core_path        = NULL;
         const char *core_name        = NULL;

         if (!info)
            continue;

         core_path = core_info->path;
         core_name = core_info->display_name;

         if (string_is_empty(core_path) ||
             string_is_empty(core_name))
            continue;

         /* If the content is supported by the currently
          * loaded core, add a 'special' entry which
          * (a) highlights this fact and (b) allows
          * special handling of the the content (e.g.
          * allow it to be launched without a core reload)
          * > This functionality is disabled on static platforms
          *   when 'always_reload_core_on_run_content' is enabled */
         if (enable_load_with_current &&
             string_is_equal(core_path, core_path_current))
         {
            /* This is a 'pending' entry. It will be
             * prepended to the displaylist once all
             * other cores have been added */
            pending_core_path = core_path;
            pending_core_name = core_name;
            core_is_pending   = true;
         }
         else if (menu_entries_append_enum(info->list, core_path,
               msg_hash_to_str(core_enum_label), core_enum_label,
               FILE_TYPE_CORE, 0, 0))
         {
            file_list_set_alt_at_offset(info->list, entry_idx, core_name);
            core_available = true;
            entry_idx++;
            count++;
         }
      }

      /* If cores were found, sort the displaylist now */
      if (core_available)
         file_list_sort_on_alt(info->list);

      /* If we have a 'pending' entry, prepended it
       * to the displaylist */
      if (core_is_pending)
      {
         char entry_alt_text[256];
         entry_alt_text[0] = '\0';

         snprintf(entry_alt_text, sizeof(entry_alt_text), "%s (%s)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE),
               pending_core_name);

         menu_entries_prepend(info->list, pending_core_path,
               msg_hash_to_str(current_core_enum_label),
               current_core_enum_label, FILE_TYPE_CORE, 0, 0);

         file_list_set_alt_at_offset(info->list, 0, entry_alt_text);

         core_available = true;
         count++;
      }
   }

   /* Fallback */
   if (!core_available)
   {
      /* We have to handle an annoying special case here.
       * Some RetroArch ports (e.g. emscripten) are 'broken'
       * by design - they do not handle cores correctly,
       * resulting in empty core_info lists and thus no
       * concept of supported content. These builds are
       * typically static, such that the currently running
       * core is the only one available. To enable the
       * loading of *any* content on these troublesome
       * platforms, we therefore have to:
       *   1) Detect the presence of a running core
       *   2) Blindly add a menu entry enabling the
       *      selection of this core
       *   3) Hope that the user does not attempt to
       *      load unsupported content... */
      char exts[32];
      exts[0] = '\0';

      /* Attempt to identify 'broken' platforms by fetching
       * the core file extension - if there is none, then
       * it is impossible for RetroArch to populate a
       * core_info list */
#if !defined(LOAD_WITHOUT_CORE_INFO)
      if (!frontend_driver_get_core_extension(exts, sizeof(exts)) ||
          string_is_empty(exts))
#endif
      {
         struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
         const char *core_path            = core_path_current;
         const char *core_name            = system ? system->library_name : NULL;

         if (!string_is_empty(core_path))
         {
            /* If we have a valid 'currently running' core
             * path, add an entry for this core to the list */
            if (menu_entries_append_enum(info->list, core_path,
                  msg_hash_to_str(current_core_enum_label),
                  current_core_enum_label, FILE_TYPE_DIRECT_LOAD, 0, 0))
            {
               if (!string_is_empty(core_name))
                  file_list_set_alt_at_offset(info->list, 0, core_name);
               core_available = true;
               count++;
            }
         }
         else if (!string_is_empty(core_name))
         {
            /* If we have a valid core name, but no core
             * path, then RetroArch on this platform is
             * likely to be unusable. But this legacy code
             * path has existed for many years, and since
             * we do not know who added it, or why, we
             * shall continue to include it lest we break
             * an unknown platform... */
            if (menu_entries_append_enum(info->list, core_name,
                  msg_hash_to_str(current_core_enum_label),
                  current_core_enum_label, FILE_TYPE_DIRECT_LOAD, 0, 0))
            {
               core_available = true;
               count++;
            }
         }
      }

      if (!core_available)
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

   return count;
}

static unsigned menu_displaylist_parse_system_info(file_list_t *list)
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
   const char *menu_driver               = menu_driver_ident();

   tmp[0] = '\0';

   snprintf(tmp, sizeof(tmp), "%s: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE), __DATE__);

   if (menu_entries_append_enum(list, tmp, "",
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
   if (menu_entries_append_enum(list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;
#endif

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, tmp, sizeof(tmp));
   if (menu_entries_append_enum(list, tmp, "",
         MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
      count++;

#ifdef ANDROID
   perms = test_permissions(internal_storage_path);

   snprintf(tmp, sizeof(tmp), "%s",
         perms
         ? msg_hash_to_str(MSG_READ_WRITE)
         : msg_hash_to_str(MSG_READ_ONLY));
   if (menu_entries_append_enum(list, tmp, "",
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
         strlcat(cpu_str, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), sizeof(cpu_str));
      else
         strlcat(cpu_str, model, sizeof(cpu_str));

      if (menu_entries_append_enum(list, cpu_str, "",
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
      if (menu_entries_append_enum(list, cpu_str, "",
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

      frontend_driver_get_cpu_architecture_str(
            cpu_arch_str, sizeof(cpu_arch_str));

      snprintf(cpu_str, sizeof(cpu_str), "%s %s", cpu_text_str, cpu_arch_str);

      if (menu_entries_append_enum(list, cpu_str,
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
      if (menu_entries_append_enum(list, cpu_str,
            msg_hash_to_str(MENU_ENUM_LABEL_CPU_CORES),
            MENU_ENUM_LABEL_CPU_CORES, MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;
   }

   for (controller = 0; controller < MAX_USERS; controller++)
   {
      if (input_config_get_device_autoconfigured(controller))
      {
         snprintf(tmp, sizeof(tmp),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME),
            controller,
            input_config_get_device_name(controller),
            input_config_get_device_name_index(controller));

         if (menu_entries_append_enum(list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;

#ifdef HAVE_RGUI
         if (string_is_equal(menu_driver, "rgui"))
         {
            snprintf(tmp, sizeof(tmp), " Device display name: %s",
               input_config_get_device_display_name(controller) ?
               input_config_get_device_display_name(controller) :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
            if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
            snprintf(tmp, sizeof(tmp), " Device config name: %s",
               input_config_get_device_config_name(controller) ?
               input_config_get_device_config_name(controller)  :
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
            if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
            snprintf(tmp, sizeof(tmp), " Device VID/PID: %d/%d",
               input_config_get_device_vid(controller),
               input_config_get_device_pid(controller));
            if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
         }
#endif
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
      if (menu_entries_append_enum(list, tmp, "",
            MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
            MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
         count++;

      if (frontend->get_lakka_version)
      {
         frontend->get_lakka_version(tmp2, sizeof(tmp2));

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION),
               ": ",
               tmp2,
               sizeof(tmp));
         if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      if (frontend->get_name)
      {
         frontend->get_name(tmp2, sizeof(tmp2));

         fill_pathname_join_concat_noext(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME),
               ": ",
               tmp2,
               sizeof(tmp));
         if (menu_entries_append_enum(list, tmp, "",
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
         if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      if (frontend->get_rating)
      {
         snprintf(tmp, sizeof(tmp), "%s : %d",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL),
               frontend->get_rating());
         if (menu_entries_append_enum(list, tmp, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }

      {
         char tmp[PATH_MAX_LENGTH];
         uint64_t memory_total = frontend_driver_get_total_memory();
         uint64_t memory_used  = memory_total - frontend_driver_get_free_memory();

         tmp[0] = '\0';

         if (memory_used != 0 && memory_total != 0)
         {
            snprintf(tmp, sizeof(tmp),
                  "%s %s: %" PRIu64 "/%" PRIu64 " MB",
                  msg_hash_to_str(MSG_MEMORY),
                  msg_hash_to_str(MSG_IN_MEGABYTES),
                  BYTES_TO_MB(memory_used),
                  BYTES_TO_MB(memory_total)
                  );

            if (menu_entries_append_enum(list, tmp, "",
                  MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
                  MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
               count++;
         }
      }

      if (frontend->get_powerstate)
      {
         int seconds    = 0, percent = 0;
         char tmp2[PATH_MAX_LENGTH];
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
         if (menu_entries_append_enum(list, tmp, "",
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
   if (menu_entries_append_enum(list, tmp, "",
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
         if (menu_entries_append_enum(list, tmp, "",
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
         if (menu_entries_append_enum(list, tmp, "",
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
         if (menu_entries_append_enum(list, tmp, "",
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
         if (menu_entries_append_enum(list, feat_str, "",
               MENU_ENUM_LABEL_SYSTEM_INFO_ENTRY,
               MENU_SETTINGS_CORE_INFO_NONE, 0, 0))
            count++;
      }
   }

   return count;
}

static int menu_displaylist_parse_playlist(menu_displaylist_info_t *info,
      playlist_t *playlist,
      settings_t *settings,
      const char *path_playlist, bool is_collection)
{
   unsigned i;
   char label_spacer[PL_LABEL_SPACER_MAXLEN];
   size_t           list_size        = playlist_size(playlist);
   bool show_inline_core_name        = false;
   const char *menu_driver           = menu_driver_ident();
   menu_search_terms_t *search_terms = menu_entries_search_get_terms();
   unsigned pl_show_inline_core_name = settings->uints.playlist_show_inline_core_name;
   bool pl_show_sublabels            = settings->bools.playlist_show_sublabels;
   void (*sanitization)(char*);

   label_spacer[0] = '\0';
   info->count     = 0;

   if (list_size == 0)
      goto error;

   /* Check whether core name should be added to playlist entries */
   if (!string_is_equal(menu_driver, "ozone") &&
       !pl_show_sublabels &&
       ((pl_show_inline_core_name == PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS) ||
        (!is_collection && !(pl_show_inline_core_name == PLAYLIST_INLINE_CORE_DISPLAY_NEVER))))
   {
      show_inline_core_name = true;

#ifdef HAVE_RGUI
      /* Get spacer for menu entry labels (<content><spacer><core>)
       * > Note: Only required when showing inline core names */
      if (string_is_equal(menu_driver, "rgui"))
         strlcpy(label_spacer, PL_LABEL_SPACER_RGUI, sizeof(label_spacer));
      else
#endif
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
   if (
         string_is_equal(path_playlist, "history")   ||
         string_is_equal(path_playlist, "favorites") ||
         string_ends_with_size(path_playlist, "_history",
            strlen(path_playlist), STRLEN_CONST("_history")))
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
      const struct playlist_entry *entry = NULL;
      const char *entry_path             = NULL;
      bool entry_valid                   = true;

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
            /* Both core name and core path must be valid */
            if (!string_is_empty(entry->core_name) && !string_is_equal(entry->core_name, "DETECT") &&
                !string_is_empty(entry->core_path) && !string_is_equal(entry->core_path, "DETECT"))
            {
               strlcat(menu_entry_label, label_spacer, sizeof(menu_entry_label));
               strlcat(menu_entry_label, entry->core_name, sizeof(menu_entry_label));
            }
         }

         entry_path = entry->path;
      }
      else
      {
         /* Playlist entry without content...
          * This is useless/broken, but have to include
          * it otherwise synchronisation between the menu
          * and the underlying playlist will be lost...
          * > Use label if available, otherwise core name
          * > If both are missing, add an empty menu entry */
         if (!string_is_empty(entry->label))
            strlcpy(menu_entry_label, entry->label, sizeof(menu_entry_label));
         else if (!string_is_empty(entry->core_name))
            strlcpy(menu_entry_label, entry->core_name, sizeof(menu_entry_label));

         entry_path = path_playlist;
      }

      /* Check whether entry matches search terms,
       * if required */
      if (search_terms)
      {
         size_t j;

         for (j = 0; j < search_terms->size; j++)
         {
            const char *search_term = search_terms->terms[j];

            if (!string_is_empty(search_term) &&
                !strcasestr(menu_entry_label, search_term))
            {
               entry_valid = false;
               break;
            }
         }
      }

      /* Add menu entry */
      if (entry_valid && menu_entries_append_enum(info->list,
            menu_entry_label, entry_path,
            MENU_ENUM_LABEL_PLAYLIST_ENTRY, FILE_TYPE_RPL_ENTRY, 0, i))
         info->count++;
   }

   if (info->count < 1)
      goto error;

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
   struct string_list str_list;
   union string_list_elem_attr attr;
   char tmp[PATH_MAX_LENGTH];
   char *output_label               = NULL;
   int str_len                      = 0;

   if (!string_list_initialize(&str_list))
      return -1;

   attr.i                           = 0;
   tmp[0]                           = '\0';

   str_len += strlen(label) + 1;
   string_list_append(&str_list, label, attr);

   str_len += strlen(actual_string) + 1;
   string_list_append(&str_list, actual_string, attr);

   str_len += strlen(path) + 1;
   string_list_append(&str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_deinitialize(&str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, &str_list, "|");
   string_list_deinitialize(&str_list);

   fill_pathname_join_concat_noext(tmp, desc, ": ",
         actual_string, sizeof(tmp));
   menu_entries_append_enum(list, tmp, output_label,
         enum_idx,
         0, 0, 0);

   free(output_label);

   return 0;
}

static int create_string_list_rdb_entry_int(
      enum msg_hash_enums enum_idx,
      const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   struct string_list str_list;
   union string_list_elem_attr attr;
   char tmp[PATH_MAX_LENGTH];
   char str[PATH_MAX_LENGTH];
   char output_label[PATH_MAX_LENGTH];
   int str_len                      = 0;

   tmp[0]          = '\0';
   str[0]          = '\0';
   output_label[0] = '\0';

   if (!string_list_initialize(&str_list))
      return -1;

   attr.i                           = 0;

   str_len                         += strlen(label) + 1;
   string_list_append(&str_list, label, attr);

   snprintf(str, sizeof(str), "%d", actual_int);
   str_len                         += strlen(str) + 1;
   string_list_append(&str_list, str, attr);

   str_len                         += strlen(path) + 1;
   string_list_append(&str_list, path, attr);

   string_list_join_concat(output_label, str_len, &str_list, "|");
   string_list_deinitialize(&str_list);

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
      settings_t *settings,
      menu_displaylist_info_t *info)
{
   unsigned i, j, k;
   char path_playlist[PATH_MAX_LENGTH];
   char path_base[PATH_MAX_LENGTH];
   char query[PATH_MAX_LENGTH];
   playlist_config_t playlist_config;
   playlist_t *playlist                = NULL;
   database_info_list_t *db_info       = NULL;
   bool show_advanced_settings         = settings->bools.menu_show_advanced_settings;
   const char *dir_playlist            = settings->paths.directory_playlist;
   const char *menu_driver             = menu_driver_ident();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);

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

   fill_pathname_join(path_playlist, dir_playlist, path_base,
         sizeof(path_playlist));

   playlist_config_set_path(&playlist_config, path_playlist);
   playlist = playlist_init(&playlist_config);

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

      snprintf(crc_str, sizeof(crc_str), "%08lX", (unsigned long)db_info_entry->crc32);

      /* This allows thumbnails to be shown while viewing database
       * entries...
       * It only makes sense to do this for the first info entry,
       * since menu drivers cannot handle multiple successive
       * calls of menu_driver_set_thumbnail_content()...
       * Note that thumbnail updates must be disabled when using
       * RGUI and GLUI, since this functionality is handled elsewhere
       * (and doing it here creates harmful conflicts) */
      if ((i == 0) &&
          !string_is_equal(menu_driver, "rgui") &&
          !string_is_equal(menu_driver, "glui"))
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
            bool match_found                    = false;

            playlist_get_index(playlist, j, &entry);

            if (entry->crc32)
            {
               struct string_list tmp_str_list = {0};

               string_list_initialize(&tmp_str_list);
               string_split_noalloc(&tmp_str_list, entry->crc32, "|");

               if (tmp_str_list.size > 0)
               {
                  if (tmp_str_list.size > 1)
                  {
                     const char *elem0 = tmp_str_list.elems[0].data;
                     const char *elem1 = tmp_str_list.elems[1].data;

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

               string_list_deinitialize(&tmp_str_list);
            }

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

      if (db_info_entry->category)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_CATEGORY,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_CATEGORY),
                  db_info_entry->category, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->language)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_LANGUAGE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_LANGUAGE),
                  db_info_entry->language, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->region)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_REGION,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_REGION),
                  db_info_entry->region, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->score)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_SCORE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_SCORE),
                  db_info_entry->score, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->media)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_MEDIA,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_MEDIA),
                  db_info_entry->media, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->controls)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_CONTROLS,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_CONTROLS),
                  db_info_entry->controls, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->artstyle)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ARTSTYLE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ARTSTYLE),
                  db_info_entry->artstyle, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->gameplay)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_GAMEPLAY,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_GAMEPLAY),
                  db_info_entry->gameplay, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->narrative)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_NARRATIVE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_NARRATIVE),
                  db_info_entry->narrative, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->pacing)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_PACING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_PACING),
                  db_info_entry->pacing, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->perspective)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_PERSPECTIVE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_PERSPECTIVE),
                  db_info_entry->perspective, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->setting)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_SETTING,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_SETTING),
                  db_info_entry->setting, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->visual)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_VISUAL,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_VISUAL),
                  db_info_entry->visual, info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->vehicular)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_VEHICULAR,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_VEHICULAR),
                  db_info_entry->vehicular, info->path, info->list) == -1)
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

      if (db_info_entry->achievements == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_ACHIEVEMENTS,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_ACHIEVEMENTS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->console_exclusive == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_CONSOLE_EXCLUSIVE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_CONSOLE_EXCLUSIVE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE), info->path, info->list) == -1)
            goto error;
      }

      if (db_info_entry->platform_exclusive == 1)
      {
         if (create_string_list_rdb_entry_string(
                  MENU_ENUM_LABEL_RDB_ENTRY_PLATFORM_EXCLUSIVE,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE),
                  msg_hash_to_str(MENU_ENUM_LABEL_RDB_ENTRY_PLATFORM_EXCLUSIVE),
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

int menu_displaylist_parse_settings_enum(
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
   uint64_t flags;
   enum setting_type precond   = precond_lut[parse_type];
   size_t             count    = 0;

   if (!setting)
      return -1;

   flags                       = setting->flags;

#ifdef HAVE_LAKKA
   if (flags & (SD_FLAG_ADVANCED | SD_FLAG_LAKKA_ADVANCED))
#else
   if (flags & (SD_FLAG_ADVANCED))
#endif
   {
      settings_t *settings        = config_get_ptr();
      bool show_advanced_settings = settings->bools.menu_show_advanced_settings;
      if (!show_advanced_settings)
         goto end;
   }

   for (;;)
   {
      bool time_to_exit             = false;
      const char *short_description = setting->short_description;
      const char *name              = setting->name;
      enum setting_type type        = setting->type;
      rarch_setting_t **list        = &setting;
      int type_flags                = 0;

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

      switch (type)
      {
         case ST_STRING_OPTIONS:
            type_flags = MENU_SETTING_STRING_OPTIONS;
            break;
         case ST_ACTION:
            type_flags = MENU_SETTING_ACTION;
            break;
         case ST_PATH:
            type_flags = FILE_TYPE_PATH;
            break;
         case ST_GROUP:
            type_flags = MENU_SETTING_GROUP;
            break;
         case ST_SUB_GROUP:
            type_flags = MENU_SETTING_SUBGROUP;
            break;
         default:
            break;
      }

      if (is_enum)
         menu_entries_append_enum(info_list,
               short_description, name,
               (enum msg_hash_enums)entry_type,
               type_flags, 0, 0);
      else
      {
         if (
               (entry_type >= MENU_SETTINGS_INPUT_BEGIN) &&
               (entry_type < MENU_SETTINGS_INPUT_END)
            )
            entry_type = (unsigned)(MENU_SETTINGS_INPUT_BEGIN + count);
         if (entry_type == 0)
            entry_type = type_flags;

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
            if (setting->type == precond)
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
      menu_handle_t *menu, settings_t *settings,
      const char *path, bool sort_enabled)
{
   playlist_config_t playlist_config;
   const char *playlist_file_name      = path_basename_nocompression(path);
   int content_favorites_size          = settings->ints.content_favorites_size;
   unsigned content_history_size       = settings->uints.content_history_size;
   bool playlist_sort_alphabetical     = settings->bools.playlist_sort_alphabetical;

   playlist_config_set_path(&playlist_config, path);
   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);

   menu->db_playlist_file[0]       = '\0';

   if (playlist_get_cached())
      playlist_free_cached();

   /* Get proper playlist capacity */
   if (!string_is_empty(playlist_file_name))
   {
      if (string_ends_with_size(playlist_file_name, "_history.lpl",
               strlen(playlist_file_name), STRLEN_CONST("_history.lpl")))
         playlist_config.capacity = content_history_size;
      else if (string_is_equal(playlist_file_name,
                     FILE_PATH_CONTENT_FAVORITES) &&
               (content_favorites_size >= 0))
         playlist_config.capacity = (unsigned)content_favorites_size;
   }

   if (playlist_init_cached(&playlist_config))
   {
      playlist_t *playlist                      = playlist_get_cached();
      enum playlist_sort_mode current_sort_mode = playlist_get_sort_mode(playlist);

      /* Sort playlist, if required */
      if (sort_enabled &&
          ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
           (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL)))
         playlist_qsort(playlist);

      strlcpy(
            menu->db_playlist_file,
            path,
            sizeof(menu->db_playlist_file));
   }
}

static int menu_displaylist_parse_horizontal_list(
      menu_handle_t *menu,
      settings_t *settings,
      menu_displaylist_info_t *info)
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
      const char *dir_playlist  = settings->paths.directory_playlist;

      lpl_basename[0]           = '\0';
      path_playlist[0]          = '\0';

      fill_pathname_join(path_playlist, dir_playlist, item->path,
            sizeof(path_playlist));

      /* Horizontal lists are always 'collections'
       * > Enable sorting (if allowed by user config) */
      menu_displaylist_set_new_playlist(menu, settings, path_playlist, true);

      /* Thumbnail system must be set *after* playlist
       * is loaded/cached */
      fill_pathname_base_noext(lpl_basename, item->path, sizeof(lpl_basename));
      menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));
   }

   playlist = playlist_get_cached();

   if (playlist)
      menu_displaylist_parse_playlist(info,
            playlist, settings,
            msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION), true);

   return 0;
}

static int menu_displaylist_parse_load_content_settings(
      file_list_t *list, settings_t *settings,
      bool horizontal)
{
   unsigned count         = 0;

   if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
#ifdef HAVE_LAKKA
      bool show_advanced_settings         = settings->bools.menu_show_advanced_settings;
#endif
      bool quickmenu_show_resume_content  = settings->bools.quick_menu_show_resume_content;
      bool quickmenu_show_restart_content = settings->bools.quick_menu_show_restart_content;
      bool savestates_enabled             = core_info_current_supports_savestate();
      rarch_system_info_t *system         = &runloop_state_get_ptr()->system;

      if (quickmenu_show_resume_content)
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_RESUME_CONTENT),
               MENU_ENUM_LABEL_RESUME_CONTENT,
               MENU_SETTING_ACTION_RUN, 0, 0))
            count++;

      if (quickmenu_show_restart_content)
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_RESTART_CONTENT),
               MENU_ENUM_LABEL_RESTART_CONTENT,
               MENU_SETTING_ACTION_RUN, 0, 0))
            count++;

      /* Note: Entry type depends on whether quick menu
       * was accessed via a playlist ('horizontal content')
       * or the main menu
       * > This allows us to identify a close content event
       *   triggered via 'Main Menu > Quick Menu', which
       *   subsequently requires the menu stack to be flushed
       *   in order to prevent the display of an empty
       *   'No items' menu */
      if (settings->bools.quick_menu_show_close_content)
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT),
               msg_hash_to_str(MENU_ENUM_LABEL_CLOSE_CONTENT),
               MENU_ENUM_LABEL_CLOSE_CONTENT,
               horizontal ? MENU_SETTING_ACTION_CLOSE_HORIZONTAL :
                     MENU_SETTING_ACTION_CLOSE,
               0, 0))
            count++;

#ifdef HAVE_SCREENSHOTS
      if (settings->bools.quick_menu_show_take_screenshot)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_TAKE_SCREENSHOT),
               MENU_ENUM_LABEL_TAKE_SCREENSHOT,
               MENU_SETTING_ACTION_SCREENSHOT, 0, 0))
            count++;
      }
#endif

      if (savestates_enabled &&
          settings->bools.quick_menu_show_save_load_state)
      {
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
               MENU_ENUM_LABEL_STATE_SLOT, PARSE_ONLY_INT, true) == 0)
            count++;

         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_STATE),
               MENU_ENUM_LABEL_SAVE_STATE,
               MENU_SETTING_ACTION_SAVESTATE, 0, 0))
            count++;

         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_STATE),
               MENU_ENUM_LABEL_LOAD_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0))
            count++;
      }

      if (savestates_enabled &&
          settings->bools.quick_menu_show_save_load_state &&
          settings->bools.quick_menu_show_undo_save_load_state)
      {
#ifdef HAVE_CHEEVOS
         if (!rcheevos_hardcore_active())
#endif
         {
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE),
                  msg_hash_to_str(MENU_ENUM_LABEL_UNDO_LOAD_STATE),
                  MENU_ENUM_LABEL_UNDO_LOAD_STATE,
                  MENU_SETTING_ACTION_LOADSTATE, 0, 0))
               count++;
         }

         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE),
               msg_hash_to_str(MENU_ENUM_LABEL_UNDO_SAVE_STATE),
               MENU_ENUM_LABEL_UNDO_SAVE_STATE,
               MENU_SETTING_ACTION_LOADSTATE, 0, 0))
            count++;
      }

      if (
            settings->bools.quick_menu_show_add_to_favorites &&
            settings->bools.menu_content_show_favorites
         )
      {
         bool add_to_favorites_enabled = true;

         /* Skip 'Add to Favourites' if we are currently
          * viewing an entry of the favourites playlist */
         if (horizontal)
         {
            playlist_t *playlist      = playlist_get_cached();
            const char *playlist_path = playlist_get_conf_path(playlist);
            const char *playlist_file = NULL;

            if (!string_is_empty(playlist_path))
               playlist_file = path_basename_nocompression(playlist_path);

            if (!string_is_empty(playlist_file) &&
                string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
               add_to_favorites_enabled = false;
         }

         if (add_to_favorites_enabled &&
             menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES),
                  MENU_ENUM_LABEL_ADD_TO_FAVORITES, FILE_TYPE_PLAYLIST_ENTRY, 0, 0))
            count++;
      }

      if (string_is_not_equal(settings->arrays.record_driver, "null"))
      {
         recording_state_t *recording_st = recording_state_get_ptr();
         if (!recording_st->enable)
         {
            if (settings->bools.quick_menu_show_start_recording && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING),
                     MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING, MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (settings->bools.quick_menu_show_start_streaming && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING),
                     msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING),
                     MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING, MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
         }
         else
         {
            recording_state_t *recording_st = recording_state_get_ptr();
            if (recording_st->streaming_enable)
            {
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING),
                        msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING),
                        MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING, MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
            else
            {
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING),
                        msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING),
                        MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING, MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
         }
      }

      if (settings->bools.quick_menu_show_options && !settings->bools.kiosk_mode_enable)
      {
         /* Empty 'path' string signifies top level
          * core options menu */
         if (menu_entries_append_enum(list,
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS),
               MENU_ENUM_LABEL_CORE_OPTIONS,
               MENU_SETTING_ACTION_CORE_OPTIONS, 0, 0))
            count++;
      }

      if (settings->bools.menu_show_overlays && !settings->bools.kiosk_mode_enable)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS),
               MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }

#ifdef HAVE_VIDEO_LAYOUT
      if (settings->bools.menu_show_video_layout && !settings->bools.kiosk_mode_enable)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS),
               MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }
#endif

#ifdef HAVE_REWIND
      if (settings->bools.menu_show_rewind &&
          !settings->bools.kiosk_mode_enable &&
          core_info_current_supports_rewind())
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS),
               MENU_ENUM_LABEL_REWIND_SETTINGS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }
#endif

      if (settings->bools.menu_show_latency && !settings->bools.kiosk_mode_enable)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_LATENCY_SETTINGS),
               MENU_ENUM_LABEL_LATENCY_SETTINGS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }

#if 0
      if (menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_SETTINGS),
            MENU_ENUM_LABEL_NETPLAY_SETTINGS,
            MENU_SETTING_ACTION, 0, 0))
         count++;
#endif

      if (settings->bools.quick_menu_show_controls && !settings->bools.kiosk_mode_enable)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS),
               MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }

#ifdef HAVE_CHEATS
      if (settings->bools.quick_menu_show_cheats)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS),
               MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }
#endif

      if ((!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            && disk_control_enabled(&system->disk_control))
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS),
               MENU_ENUM_LABEL_DISK_OPTIONS,
               MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0, 0))
            count++;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
      if (video_shader_any_supported())
      {
         if (settings->bools.quick_menu_show_shaders && !settings->bools.kiosk_mode_enable)
         {
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS),
                  msg_hash_to_str(MENU_ENUM_LABEL_SHADER_OPTIONS),
                  MENU_ENUM_LABEL_SHADER_OPTIONS,
                  MENU_SETTING_ACTION, 0, 0))
               count++;
         }
      }
#endif

      if ((settings->bools.quick_menu_show_save_core_overrides ||
         settings->bools.quick_menu_show_save_game_overrides) &&
         !settings->bools.kiosk_mode_enable)
      {
         if (menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS),
            MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS,
            MENU_SETTING_ACTION, 0, 0))
            count++;
      }

#ifdef HAVE_CHEEVOS
      if (settings->bools.cheevos_enable)
      {
         if (menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_ACHIEVEMENT_LIST),
            MENU_ENUM_LABEL_ACHIEVEMENT_LIST,
            MENU_SETTING_ACTION, 0, 0))
            count++;
      }
#endif

      if (settings->bools.quick_menu_show_information)
      {
         if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INFORMATION),
               msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION),
               MENU_ENUM_LABEL_INFORMATION,
               MENU_SETTING_ACTION, 0, 0))
            count++;
      }
   }

   return count;
}

static int menu_displaylist_parse_horizontal_content_actions(
      menu_handle_t *menu,
      settings_t *settings,
      menu_displaylist_info_t *info)
{
   bool content_loaded             = false;
   playlist_t *playlist            = playlist_get_cached();
   const char *fullpath            = path_get(RARCH_PATH_CONTENT);
   unsigned idx                    = menu->rpl_entry_selection_ptr;
   const struct playlist_entry *entry  = NULL;

   if (playlist)
      playlist_get_index(playlist, idx, &entry);

   content_loaded = !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
         && string_is_equal(menu->deferred_path, fullpath);

   if (content_loaded)
   {
      if (menu_displaylist_parse_load_content_settings(info->list,
               settings, true) == 0)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
               msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
               MENU_ENUM_LABEL_NO_ITEMS,
               MENU_SETTING_NO_ITEM, 0, 0);
   }
   else
   {
      const char *playlist_path = NULL;
      const char *playlist_file = NULL;
#ifdef HAVE_AUDIOMIXER
      const char *ext           = NULL;

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
      playlist_path = playlist_get_conf_path(playlist);
      if (!string_is_empty(playlist_path))
         playlist_file = path_basename_nocompression(playlist_path);

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

         switch (settings->uints.playlist_entry_remove_enable)
         {
            case PLAYLIST_ENTRY_REMOVE_ENABLE_ALL:
               remove_entry_enabled = true;
               break;
            case PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV:
               {
                  char system[PATH_MAX_LENGTH];
                  system[0] = '\0';

                  menu_driver_get_thumbnail_system(system, sizeof(system));

                  if (!string_is_empty(system))
                     remove_entry_enabled =
                        string_is_equal(system,  "history")   ||
                        string_is_equal(system,  "favorites") ||
                        string_ends_with_size(system, "_history",
                              strlen(system), STRLEN_CONST("_history") );

                  /* An annoyance: if the user navigates to the information menu,
                   * then to the database entry, the thumbnail system will be changed.
                   * This breaks the above 'remove_entry_enabled' check for the
                   * history and favorites playlists. We therefore have to check
                   * the playlist file name as well... */
                  if (!remove_entry_enabled &&
                      settings->bools.quick_menu_show_information &&
                      !string_is_empty(playlist_file))
                     remove_entry_enabled = string_is_equal(playlist_file, FILE_PATH_CONTENT_HISTORY) ||
                        string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES);
               }
               break;
         }

         if (remove_entry_enabled)
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY),
                  msg_hash_to_str(MENU_ENUM_LABEL_DELETE_ENTRY),
                  MENU_ENUM_LABEL_DELETE_ENTRY,
                  MENU_SETTING_ACTION_DELETE_ENTRY, 0, 0);
      }

      /* Skip 'Add to Favourites' if we are currently
       * viewing an entry of the favourites playlist */
      if (
            settings->bools.quick_menu_show_add_to_favorites &&
            settings->bools.menu_content_show_favorites &&
            !(!string_is_empty(playlist_file) &&
              string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
         )
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST),
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST),
               MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (   settings->bools.quick_menu_show_set_core_association &&
            !settings->bools.kiosk_mode_enable)
      {
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION),
               msg_hash_to_str(MENU_ENUM_LABEL_SET_CORE_ASSOCIATION),
               MENU_ENUM_LABEL_SET_CORE_ASSOCIATION, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
      }

      if (
             settings->bools.quick_menu_show_reset_core_association &&
            !settings->bools.kiosk_mode_enable)
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
   if (
          settings->bools.quick_menu_show_download_thumbnails &&
         !settings->bools.kiosk_mode_enable)
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
            download_enabled = !string_ends_with_size(
                  system, "_history", strlen(system), STRLEN_CONST("_history"));
      }

      if (settings->bools.network_on_demand_thumbnails)
         download_enabled = false;

      if (download_enabled)
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS),
               msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS),
               MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS, FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
   }
#endif

   return 0;
}

static unsigned menu_displaylist_parse_information_list(file_list_t *info_list)
{
   unsigned count                   = 0;
   core_info_t   *core_info         = NULL;
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;

   core_info_get_current_core(&core_info);

   if (  system &&
         (!string_is_empty(system->library_name) &&
          !string_is_equal(system->library_name,
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
         )
         && core_info && core_info->has_info
      )
      if (menu_entries_append_enum(info_list,
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
            if (menu_entries_append_enum(info_list,
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
#if !defined (HAVE_SOCKET_LEGACY) || defined(GEKKO)
   if (menu_entries_append_enum(info_list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_INFORMATION),
         MENU_ENUM_LABEL_NETWORK_INFORMATION,
         MENU_SETTING_ACTION, 0, 0))
      count++;
#endif
#endif

   if (menu_entries_append_enum(info_list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION),
         msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_INFORMATION),
         MENU_ENUM_LABEL_SYSTEM_INFORMATION,
         MENU_SETTING_ACTION, 0, 0))
      count++;

#ifdef HAVE_LIBRETRODB
   if (menu_entries_append_enum(info_list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST),
         MENU_ENUM_LABEL_DATABASE_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0))
      count++;
   if (menu_entries_append_enum(info_list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER),
         msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_MANAGER_LIST),
         MENU_ENUM_LABEL_CURSOR_MANAGER_LIST,
         MENU_SETTING_ACTION, 0, 0))
      count++;
#endif

   if (retroarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
   {
      if (menu_entries_append_enum(info_list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_FRONTEND_COUNTERS),
            MENU_ENUM_LABEL_FRONTEND_COUNTERS,
            MENU_SETTING_ACTION, 0, 0))
         count++;

      if (menu_entries_append_enum(info_list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS),
            msg_hash_to_str(MENU_ENUM_LABEL_CORE_COUNTERS),
            MENU_ENUM_LABEL_CORE_COUNTERS,
            MENU_SETTING_ACTION, 0, 0))
         count++;
   }

   return count;
}

static unsigned menu_displaylist_parse_playlists(
      menu_displaylist_info_t *info,
      settings_t *settings,
      bool horizontal)
{
   size_t i, list_size;
   struct string_list str_list  = {0};
   unsigned count               = 0;
   const char *path             = info->path;
   bool show_hidden_files       = settings->bools.show_hidden_files;

   if (string_is_empty(path))
   {
      int ret = frontend_driver_parse_drive_list(info->list, true);
      /* TODO/FIXME - we need to know the actual count number here */
      if (ret == 0)
         count++;
      else
         if (menu_entries_append_enum(info->list, "/", "",
               MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
               FILE_TYPE_DIRECTORY, 0, 0))
            count++;
      return count;
   }

   if (!horizontal)
   {
      bool show_add_content  = false;
#if defined(HAVE_XMB) || defined(HAVE_OZONE)
      const char *menu_ident = menu_driver_ident();

      if (string_is_equal(menu_ident, "xmb") ||
          string_is_equal(menu_ident, "ozone"))
         show_add_content = settings->bools.menu_content_show_add;
      else
#endif
         show_add_content = (settings->uints.menu_content_show_add_entry ==
               MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB);

      if (show_add_content)
         if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST),
                  MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;

#if defined(HAVE_LIBRETRODB)
      if (settings->bools.menu_content_show_explore)
         if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE),
                  msg_hash_to_str(MENU_ENUM_LABEL_GOTO_EXPLORE),
                  MENU_ENUM_LABEL_GOTO_EXPLORE,
                  MENU_EXPLORE_TAB, 0, 0))
            count++;
#endif

#if defined(HAVE_DYNAMIC)
      if (settings->uints.menu_content_show_contentless_cores !=
            MENU_CONTENTLESS_CORES_DISPLAY_NONE)
         if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES),
                  msg_hash_to_str(MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES),
                  MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES,
                  MENU_CONTENTLESS_CORES_TAB, 0, 0))
            count++;
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

   if (!dir_list_initialize(&str_list, path, NULL, true,
         show_hidden_files, true, false))
      return count;

   dir_list_sort(&str_list, true);

   list_size = str_list.size;

   for (i = 0; i < list_size; i++)
   {
      const char *path             = str_list.elems[i].data;
      const char *playlist_file    = NULL;
      enum msg_file_type file_type = FILE_TYPE_NONE;

      switch (str_list.elems[i].attr.i)
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

      playlist_file = path_basename_nocompression(path);

      if (string_is_empty(playlist_file))
         continue;

      /* Ignore non-playlist files */
      if (!string_is_equal_noncase(path_get_extension(playlist_file),
               "lpl"))
         continue;

      /* Ignore history/favourites */
      if (
               string_ends_with_size(path, "_history.lpl",
                  strlen(path), STRLEN_CONST("_history.lpl"))
            || string_is_equal(playlist_file,
               FILE_PATH_CONTENT_FAVORITES))
         continue;

      file_type = FILE_TYPE_PLAYLIST_COLLECTION;

      if (horizontal)
         path = playlist_file;

      if (menu_entries_append_enum(info->list, path, "",
            MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY,
            file_type, 0, 0))
         count++;
   }

   dir_list_deinitialize(&str_list);

   return count;
}

static unsigned menu_displaylist_parse_cores(
      menu_handle_t       *menu,
      settings_t *settings,
      menu_displaylist_info_t *info)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;
   unsigned items_found         = 0;
   const char *path             = info->path;
   bool ok                      = false;
   bool show_hidden_files       = settings->bools.show_hidden_files;

   if (string_is_empty(path))
   {
      if (frontend_driver_parse_drive_list(info->list, true) != 0)
         menu_entries_append_enum(info->list, "/", "",
               MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR, FILE_TYPE_DIRECTORY, 0, 0);
      items_found++;
      return items_found;
   }

   str_list = string_list_new();
   ok       = dir_list_append(str_list, path, info->exts,
         true, show_hidden_files, false, false);

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   /* UWP: browse the optional packages for additional cores */
   struct string_list *core_packages = string_list_new();
   uwp_fill_installed_core_packages(core_packages);
   for (i = 0; i < core_packages->size; i++)
      dir_list_append(str_list, core_packages->elems[i].data, info->exts,
            true, show_hidden_files, true, false);

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
      char out_dir[PATH_MAX_LENGTH];

      out_dir[0] = '\0';

      fill_pathname_parent_dir(out_dir, path, sizeof(out_dir));

      if (string_is_empty(out_dir))
         menu_entries_prepend(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY),
               path,
               MENU_ENUM_LABEL_PARENT_DIRECTORY,
               FILE_TYPE_PARENT_DIRECTORY, 0, 0);
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
         path = path_basename_nocompression(path);

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

      list_size                      = info->list->size;

      for (i = 0; i < list_size; i++)
      {
         const char *path                   = info->list->list[i].path;
         unsigned type                      = info->list->list[i].type;

         if (type == FILE_TYPE_CORE)
         {
            char core_path[PATH_MAX_LENGTH];
            char display_name[PATH_MAX_LENGTH];
            core_path[0]       =
            display_name[0]    = '\0';

            fill_pathname_join(core_path, dir, path, sizeof(core_path));

            if (core_info_list_get_display_name(list,
                     core_path, display_name, sizeof(display_name)))
               file_list_set_alt_at_offset(info->list, i, display_name);
         }
      }
      info->need_sort = true;
   }

   return items_found;
}

static unsigned menu_displaylist_parse_playlist_manager_list(
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   unsigned count               = 0;
   const char *dir_playlist     = settings->paths.directory_playlist;
   bool show_hidden_files       = settings->bools.show_hidden_files;
   bool history_list_enable     = settings->bools.history_list_enable;
   struct string_list *str_list = dir_list_new_special(
         dir_playlist,
         DIR_LIST_COLLECTIONS, NULL,
         show_hidden_files);

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

         playlist_file = path_basename_nocompression(path);

         if (string_is_empty(playlist_file))
            continue;

         /* Ignore non-playlist files */
         if (!string_is_equal_noncase(path_get_extension(playlist_file),
                  "lpl"))
            continue;

         /* Ignore history/favourites
          * > content_history + favorites are handled separately
          * > music/video/image_history are ignored */
         if (
                  string_ends_with_size(path, "_history.lpl",
                     strlen(path), STRLEN_CONST("_history.lpl"))
               || string_is_equal(playlist_file,
                  FILE_PATH_CONTENT_FAVORITES))
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
   if (history_list_enable)
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
      settings_t *settings,
      menu_displaylist_info_t *info,
      const char *playlist_path)
{
   enum msg_hash_enums right_thumbnail_label_value;
   enum msg_hash_enums left_thumbnail_label_value;
   bool is_content_history;
   const char *playlist_file    = NULL;
   playlist_t *playlist         = NULL;
   const char *menu_driver      = menu_driver_ident();

   if (string_is_empty(playlist_path))
      return false;

   playlist_file = path_basename_nocompression(playlist_path);

   if (string_is_empty(playlist_file))
      return false;

   /* Note: We are caching the current playlist
    * here so we can get its path and/or modify
    * its metadata when performing management
    * tasks. We *don't* care about entry order
    * at this stage, so we can save a few clock
    * cycles by disabling sorting */
   menu_displaylist_set_new_playlist(menu, settings, playlist_path, false);

   playlist = playlist_get_cached();

   if (!playlist)
      return false;

   /* Check whether this is a content history playlist */
   is_content_history = string_ends_with_size(
         playlist_path, "_history.lpl", strlen(playlist_path),
         STRLEN_CONST("_history.lpl"));

   /* Default core association
    * > This is only shown for collection playlists
    *   (i.e. it is not relevant for history/favourites) */
   if (!is_content_history &&
       !string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
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
         MENU_SETTING_ACTION_PLAYLIST_MANAGER_RESET_CORES, 0, 0);

   /* Label display mode */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
         MENU_SETTING_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE, 0, 0);

   /* Thumbnail modes */
   right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
   left_thumbnail_label_value  = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS;

   /* > Get label values */
#ifdef HAVE_RGUI
   if (string_is_equal(menu_driver, "rgui"))
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI;
      left_thumbnail_label_value  = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI;
   }
#endif
#ifdef HAVE_OZONE
   if (string_is_equal(menu_driver, "ozone"))
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS;
      left_thumbnail_label_value  = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE;
   }
#endif
#ifdef HAVE_MATERIALUI
   if (string_is_equal(menu_driver, "glui"))
   {
      right_thumbnail_label_value = MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI;
      left_thumbnail_label_value  = MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI;
   }
#endif

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

   /* Sorting mode
    * > Not relevant for history playlists  */
   if (!is_content_history)
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE),
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE),
            MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE,
            MENU_SETTING_PLAYLIST_MANAGER_SORT_MODE, 0, 0);

   /* Refresh playlist */
   if (playlist_scan_refresh_enabled(playlist))
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST),
            msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST),
            MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
            MENU_SETTING_ACTION_PLAYLIST_MANAGER_REFRESH_PLAYLIST, 0, 0);

   /* Clean playlist */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST),
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST),
         MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
         MENU_SETTING_ACTION_PLAYLIST_MANAGER_CLEAN_PLAYLIST, 0, 0);

   /* Delete playlist */
   menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST),
         msg_hash_to_str(MENU_ENUM_LABEL_DELETE_PLAYLIST),
         MENU_ENUM_LABEL_DELETE_PLAYLIST,
         MENU_SETTING_ACTION_DELETE_PLAYLIST, 0, 0);

   return true;
}

#ifdef HAVE_NETWORKING
static unsigned menu_displaylist_parse_pl_thumbnail_download_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   unsigned count               = 0;
   const char *dir_playlist     = settings->paths.directory_playlist;
   bool show_hidden_files       = settings->bools.show_hidden_files;
   struct string_list *str_list = dir_list_new_special(
         dir_playlist,
         DIR_LIST_COLLECTIONS, NULL,
         show_hidden_files);

   if (str_list && str_list->size)
   {
      unsigned i;

      dir_list_sort(str_list, true);

      for (i = 0; i < str_list->size; i++)
      {
         char path_base[PATH_MAX_LENGTH];
         const char *path;

         path_base[0] = '\0';

         if (str_list->elems[i].attr.i == FILE_TYPE_DIRECTORY)
            continue;

         path = path_basename(str_list->elems[i].data);

         if (      string_is_empty(path)
               || !string_is_equal_noncase(path_get_extension(path),
                  "lpl"))
            continue;

         strlcpy(path_base, path, sizeof(path_base));
         path_remove_extension(path_base);

         menu_entries_append_enum(info->list,
               path_base,
               path,
               MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_ENTRY,
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
      settings_t *settings,
      menu_displaylist_info_t *info)
{
   char tmp[8192];
   char core_name[PATH_MAX_LENGTH];
   playlist_t *playlist                = playlist_get_cached();
   unsigned idx                        = menu->rpl_entry_selection_ptr;
   const struct playlist_entry *entry  = NULL;
   const char *loaded_content_path     = path_get(RARCH_PATH_CONTENT);
   const char *loaded_core_path        = path_get(RARCH_PATH_CORE);
   const char *content_label           = NULL;
   const char *content_path            = NULL;
   const char *core_path               = NULL;
   const char *db_name                 = NULL;
   bool playlist_origin                = true;
   bool playlist_valid                 = false;
   const char *origin_label            = NULL;
   struct menu_state *menu_st          = menu_state_get_ptr();
   file_list_t *list                   = NULL;
   unsigned count                      = 0;
   bool content_loaded                 = !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL)
      && !string_is_empty(loaded_content_path)
      && string_is_equal(menu->deferred_path, loaded_content_path);
   bool core_supports_no_game          = false;

   core_name[0]                        = '\0';

   /* Check the origin menu from which the information
    * entry was selected
    * > Can only assume a valid playlist if the origin
    *   was an actual playlist - i.e. cached playlist is
    *   dubious if information was selected from
    *   'Main Menu > Quick Menu' or 'Standalone Cores >
    *   Quick Menu' */
   if (menu_st->entries.list)
      list  = MENU_LIST_GET(menu_st->entries.list, 0);
   if (list && (list->size > 2))
   {
      file_list_get_at_offset(list, list->size - 3, NULL,
            &origin_label, NULL, NULL);

      if (string_is_equal(origin_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU)) ||
          string_is_equal(origin_label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB)) ||
          string_is_equal(origin_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST)))
         playlist_origin = false;
   }

   /* If origin menu was a playlist, may rely on
    * return value from playlist_get_cached() */
   if (playlist_origin)
      playlist_valid = !!playlist;
   else
   {
      /* If origin menu was not a playlist, then
       * check currently loaded content against
       * last cached playlist */
      if (content_loaded &&
          !string_is_empty(loaded_core_path))
         playlist_valid = playlist_index_is_valid(
               playlist, idx, loaded_content_path, loaded_core_path);
   }

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

         /* Only display core name if both core name and
          * core path are valid */
         if (!string_is_empty(entry->core_name) &&
             !string_is_empty(core_path) &&
             !string_is_equal(core_path, "DETECT"))
            strlcpy(core_name, entry->core_name, sizeof(core_name));
      }
   }
   else
   {
      core_info_t *core_info = NULL;

      /* No playlist - just extract what we can... */
      content_path   = loaded_content_path;
      core_path      = loaded_core_path;

      if (core_info_find(core_path, &core_info))
      {
         core_supports_no_game = core_info->supports_no_game;

         if (!string_is_empty(core_info->display_name))
            strlcpy(core_name, core_info->display_name, sizeof(core_name));
      }
   }

   /* If content path is empty and core supports
    * contentless operation, skip label/path entries */
   if (!(core_supports_no_game && string_is_empty(content_path)))
   {
      /* Content label */
      tmp[0]   = '\0';
      snprintf(tmp, sizeof(tmp),
            "%s: %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL),
            !string_is_empty(content_label)
                  ? content_label
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
            );
      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_LABEL),
            MENU_ENUM_LABEL_CONTENT_INFO_LABEL,
            0, 0, 0))
         count++;

      /* Content path */
      tmp[0]   = '\0';
      snprintf(tmp, sizeof(tmp),
            "%s: %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH),
            !string_is_empty(content_path)
                  ? content_path
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
            );
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
      snprintf(tmp, sizeof(tmp),
            "%s: %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME),
            core_name
            );
      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_CORE_NAME),
            MENU_ENUM_LABEL_CONTENT_INFO_CORE_NAME,
            0, 0, 0))
         count++;
   }

   /* Runtime */
   if (((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE) &&
         settings->bools.content_runtime_log) ||
       ((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_AGGREGATE) &&
         !settings->bools.content_runtime_log_aggregate))
   {
      runtime_log_t *runtime_log = runtime_log_init(
            content_path,
            core_path,
            settings->paths.directory_runtime_log,
            settings->paths.directory_playlist,
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
                  (enum playlist_sublabel_last_played_style_type)settings->uints.playlist_sublabel_last_played_style,
                  (enum playlist_sublabel_last_played_date_separator_type)settings->uints.menu_timedate_date_separator);

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

#ifdef HAVE_CHEEVOS
   /* RetroAchievements Hash */
   if (settings->bools.cheevos_enable && settings->arrays.cheevos_token[0] &&
      !string_is_empty(loaded_content_path))
   {
      tmp[0]   = '\0';
      snprintf(tmp, sizeof(tmp),
            "%s: %s",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH),
            rcheevos_get_hash()
            );
      if (menu_entries_append_enum(info->list, tmp,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH),
            MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
            0, 0, 0))
         count++;
   }
#endif

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
         snprintf(tmp, sizeof(tmp),
               "%s: %s",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE),
               db_name_no_ext
               );
         if (menu_entries_append_enum(info->list, tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_INFO_DATABASE),
               MENU_ENUM_LABEL_CONTENT_INFO_DATABASE,
               0, 0, 0))
            count++;
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

static unsigned menu_displaylist_parse_disk_options(
      file_list_t *list)
{
   unsigned count                = 0;
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
   bool disk_ejected             = false;

   /* Sanity Check */
   if (!sys_info)
      return count;

   if (!disk_control_enabled(&sys_info->disk_control))
      return count;

   /* Check whether disk is currently ejected */
   disk_ejected = disk_control_get_eject_state(&sys_info->disk_control);

   /* Always show a 'DISK_CYCLE_TRAY_STATUS' entry
    * > These perform the same function, but just have
    *   different labels/sublabels */
   if (disk_ejected)
   {
      if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_INSERT),
               MENU_ENUM_LABEL_DISK_TRAY_INSERT,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0))
         count++;
   }
   else
      if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_TRAY_EJECT),
               MENU_ENUM_LABEL_DISK_TRAY_EJECT,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0, 0))
         count++;

   /* Only show disk index if disk is currently ejected */
   if (disk_ejected)
      if (menu_entries_append_enum(list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_DISK_INDEX),
               MENU_ENUM_LABEL_DISK_INDEX,
               MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0, 0))
         count++;

   /* If core does not support appending images,
    * can stop here */
   if (!disk_control_append_enabled(&sys_info->disk_control))
      return count;

   /* Always show a 'DISK_IMAGE_APPEND' entry
    * > If tray is currently shut, this will:
    *   - Open tray
    *   - Append disk image
    *   - Close tray
    * > If tray is currently open, this will
    *   only append a disk image */
   if (menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND),
            msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND),
            MENU_ENUM_LABEL_DISK_IMAGE_APPEND,
            MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0, 0))
      count++;

   return count;
}

static int menu_displaylist_parse_input_device_type_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   const struct retro_controller_description *desc = NULL;
   const char *name             = NULL;

   rarch_system_info_t *system  = &runloop_state_get_ptr()->system;

   enum msg_hash_enums enum_idx = (enum msg_hash_enums)atoi(info->path);
   rarch_setting_t     *setting = menu_setting_find_enum(enum_idx);
   size_t menu_index            = 0;
   unsigned count               = 0;

   unsigned i                   = 0;
   unsigned types               = 0;
   unsigned port                = 0;
   unsigned current_device      = 0;
   unsigned devices[128]        = {0};

   char device_id[10];
   device_id[0]                 = '\0';

   if (!system || !settings || !setting)
      goto end;

   port = setting->index_offset;

   if (port >= MAX_USERS)
      goto end;

   types          = libretro_device_get_size(devices, ARRAY_SIZE(devices), port);
   current_device = input_config_get_device(port);
   for (i = 0; i < types; i++)
   {
      snprintf(device_id, sizeof(device_id), "%d", devices[i]);

      desc = NULL;
      name = NULL;

      if (system && port < system->ports.size)
         desc = libretro_find_controller_description(
               &system->ports.data[port],
               devices[i]);
      if (desc)
         name = desc->desc;

      if (!name)
      {
         /* Find generic name. */
         switch (devices[i])
         {
            case RETRO_DEVICE_NONE:
               name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
               break;
            case RETRO_DEVICE_JOYPAD:
               name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETROPAD);
               break;
            case RETRO_DEVICE_ANALOG:
               name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG);
               break;
            default:
               name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);
               break;
         }
      }

      /* Add menu entry */
      if (menu_entries_append_enum(info->list,
            name,
            device_id,
            MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE,
            MENU_SETTING_DROPDOWN_ITEM_INPUT_DEVICE_TYPE,
            0, i))
      {
         /* Add checkmark if input is currently
          * mapped to this entry */
         if (current_device == devices[i])
         {
            menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[menu_index].actiondata;
            if (cbs)
               cbs->checked = true;
            menu_navigation_set_selection(menu_index);
         }

         count++;
         menu_index++;
      }
   }

end:
   /* Fallback */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0))
         count++;

   return count;
}

static int menu_displaylist_parse_input_device_index_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   rarch_system_info_t *system  = &runloop_state_get_ptr()->system;
   enum msg_hash_enums enum_idx = (enum msg_hash_enums)atoi(info->path);
   rarch_setting_t     *setting = menu_setting_find_enum(enum_idx);
   size_t menu_index            = 0;
   unsigned count               = 0;

   unsigned i                   = 0;
   unsigned port                = 0;
   unsigned map                 = 0;
   int current_device           = -1;
   unsigned max_devices         = input_config_get_device_count();

   char device_id[10];
   char device_label[128];
   const char *device_name      = NULL;

   device_id[0]                 = '\0';
   device_label[0]              = '\0';

   if (!system || !settings || !setting)
      goto end;

   port = setting->index_offset;
   map  = settings->uints.input_joypad_index[port];

   if (port >= MAX_USERS)
      goto end;

   for (i = max_devices + 1; i--;)
   {
      snprintf(device_id, sizeof(device_id), "%d", i);

      device_label[0] = '\0';
      device_name     = NULL;

      if (i < max_devices)
      {
         device_name = input_config_get_device_display_name(i) ?
               input_config_get_device_display_name(i) : input_config_get_device_name(i);

         if (!string_is_empty(device_name))
         {
            unsigned idx = input_config_get_device_name_index(i);

            /*if idx is non-zero, it's part of a set*/
            if (idx > 0)
               snprintf(device_label, sizeof(device_label),
                     "%s (#%u)",
                     device_name,
                     idx);
            else
               strlcpy(device_label, device_name, sizeof(device_label));
         }
         else
            snprintf(device_label, sizeof(device_label), "%s (%s %u)",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
                  map);
      }
      else
         strlcpy(device_label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), sizeof(device_label));

      /* Add menu entry */
      if (menu_entries_append_enum(info->list,
            device_label,
            device_id,
            MENU_ENUM_LABEL_INPUT_BIND_DEVICE_INDEX,
            MENU_SETTING_DROPDOWN_ITEM_INPUT_DEVICE_INDEX,
            0, i))
      {
         /* Add checkmark if input is currently
          * mapped to this entry */
         if (i == map)
         {
            menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[menu_index].actiondata;
            if (cbs)
               cbs->checked = true;
            menu_navigation_set_selection(menu_index);
            current_device = i;
         }

         count++;
         menu_index++;
      }
   }

   /* Disabled is the last device, which is the first entry */
   if (current_device < 0)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[0].actiondata;
      if (cbs)
         cbs->checked = true;
      menu_navigation_set_selection(0);
   }

end:
   /* Fallback */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0))
         count++;

   return count;
}

static int menu_displaylist_parse_input_description_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   unsigned count              = 0;
   rarch_system_info_t *system = &runloop_state_get_ptr()->system;
   size_t menu_index           = 0;
   bool current_input_mapped   = false;
   unsigned user_idx;
   unsigned btn_idx;
   unsigned current_remap_idx;
   unsigned mapped_port;
   size_t i, j;
   char entry_label[21];

   entry_label[0] = '\0';

   if (!system || !settings)
      goto end;

   /* Determine user/button indices */
   user_idx    = (info->type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx     = (info->type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;

   if ((user_idx >= MAX_USERS) ||
       (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      goto end;

   mapped_port = settings->uints.input_remap_ports[user_idx];

   if (mapped_port >= MAX_USERS)
      goto end;

   /* Get current mapping for selected button */
   current_remap_idx = settings->uints.input_remap_ids[user_idx][btn_idx];

   if (current_remap_idx >= RARCH_CUSTOM_BIND_LIST_END)
      current_remap_idx = RARCH_UNMAPPED;

   /* An annoyance: Menu entries do not have
    * enough parameters to pass on all the information
    * required to interpret a remap selection without
    * adding workarounds...
    * We need to record the current user/button indices,
    * and so have to convert 'info->type' to a string
    * and pass it as the entry label... */
   snprintf(entry_label, sizeof(entry_label),
         "%u", info->type);

   /* Loop over core input definitions */
   for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
   {
      const char *input_desc_btn;

      i = (j < RARCH_ANALOG_BIND_LIST_END) ? input_config_bind_order[j] : j;
      input_desc_btn = system->input_desc_btn[mapped_port][i];

      /* Check whether an input is defined for
       * this button */
      if (!string_is_empty(input_desc_btn))
      {
         char input_description[256];

         input_description[0] = '\0';

         /* > Up to RARCH_FIRST_CUSTOM_BIND, inputs
          *   are buttons - description can be used
          *   directly
          * > Above RARCH_FIRST_CUSTOM_BIND, inputs
          *   are analog axes - have to add +/-
          *   indicators */
         if (i < RARCH_FIRST_CUSTOM_BIND)
            strlcpy(input_description, input_desc_btn,
                  sizeof(input_description));
         else
            snprintf(input_description, sizeof(input_description),
                     "%s %c", input_desc_btn,
                     ((i % 2) == 0) ? '+' : '-');

         if (string_is_empty(input_description))
            continue;

         /* Add menu entry */
         if (menu_entries_append_enum(info->list,
               input_description,
               entry_label,
               MENU_ENUM_LABEL_INPUT_DESCRIPTION,
               MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION,
               0, i))
         {
            /* Add checkmark if input is currently
             * mapped to this entry */
            if (current_remap_idx == i)
            {
               menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[menu_index].actiondata;
               if (cbs)
                  cbs->checked = true;
               menu_navigation_set_selection(menu_index);
               current_input_mapped = true;
            }

            count++;
            menu_index++;
         }
      }
   }

   /* Add 'unmapped' entry at end of list */
   if (menu_entries_append_enum(info->list,
         "---",
         entry_label,
         MENU_ENUM_LABEL_INPUT_DESCRIPTION,
         MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION,
         0, RARCH_UNMAPPED))
   {
      /* Add checkmark if input is currently unmapped */
      if (!current_input_mapped)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[menu_index].actiondata;
         if (cbs)
            cbs->checked = true;
         menu_navigation_set_selection(menu_index);
      }

      count++;
      menu_index++;
   }

end:
   /* Fallback */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0))
         count++;

   return count;
}

static int menu_displaylist_parse_input_description_kbd_list(
      menu_displaylist_info_t *info, settings_t *settings)
{
   size_t i;
   unsigned user_idx;
   unsigned btn_idx;
   unsigned current_key_id;
   char entry_label[21];
   unsigned count       = 0;
   size_t menu_index    = 0;

   entry_label[0] = '\0';

   if (!settings)
      goto end;

   /* Determine user/button indices */
   user_idx    = (info->type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_ANALOG_BIND_LIST_END;
   btn_idx     = (info->type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_ANALOG_BIND_LIST_END * user_idx;

   if ((user_idx >= MAX_USERS) ||
       (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      goto end;

   /* Get current mapping for selected button */
   current_key_id = settings->uints.input_keymapper_ids[user_idx][btn_idx];

   if (current_key_id >= (RARCH_MAX_KEYS + MENU_SETTINGS_INPUT_DESC_KBD_BEGIN))
      current_key_id = RETROK_FIRST;

   /* An annoyance: Menu entries do not have
    * enough parameters to pass on all the information
    * required to interpret a remap selection without
    * adding workarounds...
    * We need to record the current user/button indices,
    * and so have to convert 'info->type' to a string
    * and pass it as the entry label... */
   snprintf(entry_label, sizeof(entry_label),
         "%u", info->type);

   /* Loop over keyboard keys */
   for (i = 0; i < RARCH_MAX_KEYS; i++)
   {
      unsigned key_id       = key_descriptors[i].key;
      const char *key_label = key_descriptors[i].desc;
      char input_description[256];

      if (string_is_empty(key_label))
         continue;

      /* TODO/FIXME: Localise 'Keyboard' */
      if (key_id == RETROK_FIRST)
      {
         input_description[0] = '\0';
         strcpy_literal(input_description, "---");
      }
      else
         snprintf(input_description, sizeof(input_description),
               "Keyboard %s", key_label);

      /* Add menu entry */
      if (menu_entries_append_enum(info->list,
            input_description,
            entry_label,
            MENU_ENUM_LABEL_INPUT_DESCRIPTION_KBD,
            MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD,
            0, key_id))
      {
         /* Add checkmark if input is currently
          * mapped to this entry */
         if (current_key_id == key_id)
         {
            menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[menu_index].actiondata;
            if (cbs)
               cbs->checked = true;
            menu_navigation_set_selection(menu_index);
         }

         count++;
         menu_index++;
      }
   }

end:
   /* Fallback */
   if (count == 0)
      if (menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0))
         count++;

   return count;
}

static void menu_displaylist_parse_playlist_generic(
      menu_handle_t *menu,
      menu_displaylist_info_t *info,
      settings_t *settings,
      const char *playlist_name,
      const char *playlist_path,
      bool is_collection,
      bool sort_enabled,
      int *ret)
{
   playlist_t *playlist = NULL;

   menu_displaylist_set_new_playlist(menu, settings,
         playlist_path, sort_enabled);

   playlist             = playlist_get_cached();

   if (!playlist)
      return;

   *ret                 = menu_displaylist_parse_playlist(info,
         playlist, settings, playlist_name, is_collection);
}

#ifdef HAVE_BLUETOOTH
static void bluetooth_scan_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   unsigned i;
   file_list_t *file_list        = NULL;
   struct string_list *device_list = NULL;

   const char *path              = NULL;
   const char *label             = NULL;
   unsigned menu_type            = 0;

   menu_entries_get_last_stack(&path, &label, &menu_type, NULL, NULL);

   /* Don't push the results if we left the bluetooth menu */
   if (!string_is_equal(label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_BLUETOOTH_SETTINGS_LIST)))
      return;

   file_list = menu_entries_get_selection_buf_ptr(0);
   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, file_list);

   device_list = string_list_new();

   driver_bluetooth_get_devices(device_list);

   for (i = 0; i < device_list->size; i++)
   {
      const char *device = device_list->elems[i].data;
      menu_entries_append_enum(file_list,
            device,
            msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_BLUETOOTH),
            MENU_ENUM_LABEL_CONNECT_BLUETOOTH,
            MENU_BLUETOOTH, 0, 0);
   }

   string_list_free(device_list);
}
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
static void wifi_scan_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   bool refresh = false;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
}
#endif

bool menu_displaylist_process(menu_displaylist_info_t *info)
{
   size_t                              idx   = 0;
#if defined(HAVE_NETWORKING)
   settings_t              *settings         = config_get_ptr();
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
#ifdef HAVE_UPDATE_CORES
         menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
               MENU_ENUM_LABEL_CORE_UPDATER_LIST,
               MENU_SETTING_ACTION, 0, 0);
#endif

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
         {
            menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
            if (cbs)
               cbs->checked = true;
            menu_navigation_set_selection(i);
         }
      }
   }

   return count;
}

static bool menu_displaylist_parse_manual_content_scan_list(
      menu_displaylist_info_t *info)
{
   unsigned count = 0;

   /* Content directory */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR),
         msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR),
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR,
         MENU_SETTING_MANUAL_CONTENT_SCAN_DIR, 0, 0))
      count++;

   /* System name */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME),
         msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME),
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
         MENU_SETTING_MANUAL_CONTENT_SCAN_SYSTEM_NAME, 0, 0))
      count++;

   /* Custom system name */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, PARSE_ONLY_STRING,
         false) == 0)
      count++;

   /* Core name */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME),
         msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME),
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
         MENU_SETTING_MANUAL_CONTENT_SCAN_CORE_NAME, 0, 0))
      count++;

   /* File extensions */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_FILE_EXTS, PARSE_ONLY_STRING,
         false) == 0)
      count++;

   /* Search recursively */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY, PARSE_ONLY_BOOL,
         false) == 0)
      count++;

   /* Search inside archive files */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES, PARSE_ONLY_BOOL,
         false) == 0)
      count++;

   /* Arcade DAT file */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE, PARSE_ONLY_PATH,
         false) == 0)
      count++;

   /* Arcade DAT filter */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER, PARSE_ONLY_BOOL,
         false) == 0)
      count++;

   /* Overwrite playlist */
   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_OVERWRITE, PARSE_ONLY_BOOL,
         false) == 0)
      count++;

   /* Validate existing entries */
   if (!(*manual_content_scan_get_overwrite_playlist_ptr()) &&
       MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES, PARSE_ONLY_BOOL,
         false) == 0)
      count++;

   /* Start scan */
   if (menu_entries_append_enum(info->list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START),
         msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_START),
         MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_START,
         MENU_SETTING_ACTION_MANUAL_CONTENT_SCAN_START, 0, 0))
      count++;

   return (count > 0);
}

#ifdef HAVE_CDROM
static int menu_displaylist_parse_disc_info(file_list_t *info_list,
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

      if (menu_entries_append_enum(info_list,
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

static unsigned menu_displaylist_populate_subsystem(
      const struct retro_subsystem_info* subsystem,
      settings_t *settings,
      file_list_t *list)
{
   char star_char[16];
   unsigned count           = 0;
   const char *menu_driver  = menu_driver_ident();
   bool menu_show_sublabels = settings->bools.menu_show_sublabels;
   /* Note: Create this string here explicitly (rather than
    * using a #define elsewhere) since we need to be aware of
    * its length... */
#if defined(__APPLE__)
   /* UTF-8 support is currently broken on Apple devices... */
   static const char utf8_star_char[] = "*";
#else
   /* <BLACK STAR>
    * UCN equivalent: "\u2605" */
   static const char utf8_star_char[] = "\xE2\x98\x85";
#endif
   unsigned   i = 0;
   int        n = 0;
   bool is_rgui = string_is_equal(menu_driver, "rgui");

   /* Select appropriate 'star' marker for subsystem menu entries
    * (i.e. RGUI does not support unicode, so use a 'standard'
    * character fallback) */
   snprintf(star_char, sizeof(star_char),
         "%s", is_rgui ? "*" : utf8_star_char);

   if (menu_displaylist_has_subsystems())
   {
      runloop_state_t *runloop_st = runloop_state_get_ptr();

      for (i = 0; i < runloop_st->subsystem_current_count; i++, subsystem++)
      {
         char s[PATH_MAX_LENGTH];
         if (content_get_subsystem() == i)
         {
            if (content_get_subsystem_rom_id() < subsystem->num_roms)
            {
               snprintf(s, sizeof(s),
                  "Load %s %s",
                  subsystem->desc,
                  star_char);

               /* If using RGUI with sublabels disabled, add the
                * appropriate text to the menu entry itself... */
               if (is_rgui && !menu_show_sublabels)
               {
                  char tmp[PATH_MAX_LENGTH];

                  n = snprintf(tmp, sizeof(tmp),
                     "%s [%s %s]", s, "Current Content:",
                     subsystem->roms[content_get_subsystem_rom_id()].desc);

                  /* Stupid GCC will warn about snprintf() truncation even though
                   * we couldn't care less about it (if the menu entry label gets
                   * truncated then the string will already be too long to view in
                   * any usable manner on screen, so the fact that the end is
                   * missing is irrelevant). There are two ways to silence this noise:
                   * 1) Make the destination buffers large enough that text cannot be
                   *    truncated. This is a waste of memory.
                   * 2) Check the snprintf() return value (and take action). This is
                   *    the most harmless option, so we just print a warning if anything
                   *    is truncated.
                   * To reiterate: The actual warning generated here is pointless, and
                   * should be ignored. */
                  if ((n < 0) || (n >= PATH_MAX_LENGTH))
                  {
                     if (verbosity_is_enabled())
                     {
                        RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                     }
                  }

                  strlcpy(s, tmp, sizeof(s));
               }

               if (menu_entries_append_enum(list,
                  s,
                  msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
                  MENU_ENUM_LABEL_SUBSYSTEM_ADD,
                  MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0))
                  count++;
            }
            else
            {
               snprintf(s, sizeof(s),
                  "Start %s %s",
                  subsystem->desc,
                  star_char);

               /* If using RGUI with sublabels disabled, add the
                * appropriate text to the menu entry itself... */
               if (is_rgui && !menu_show_sublabels)
               {
                  unsigned j = 0;
                  char rom_buff[PATH_MAX_LENGTH];
                  char tmp[PATH_MAX_LENGTH];
                  rom_buff[0] = '\0';

                  for (j = 0; j < content_get_subsystem_rom_id(); j++)
                  {
                     strlcat(rom_buff,
                           path_basename(content_get_subsystem_rom(j)),
                           sizeof(rom_buff));
                     if (j != content_get_subsystem_rom_id() - 1)
                        strlcat(rom_buff, "|", sizeof(rom_buff));
                  }

                  if (!string_is_empty(rom_buff))
                  {
                     n = snprintf(tmp, sizeof(tmp), "%s [%s]", s, rom_buff);

                     /* More snprintf() gcc warning suppression... */
                     if ((n < 0) || (n >= PATH_MAX_LENGTH))
                     {
                        if (verbosity_is_enabled())
                        {
                           RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                        }
                     }

                     strlcpy(s, tmp, sizeof(s));
                  }
               }

               if (menu_entries_append_enum(list,
                  s,
                  msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_LOAD),
                  MENU_ENUM_LABEL_SUBSYSTEM_LOAD,
                  MENU_SETTINGS_SUBSYSTEM_LOAD, 0, 0))
                  count++;
            }
         }
         else
         {
            snprintf(s, sizeof(s),
               "Load %s",
               subsystem->desc);

            /* If using RGUI with sublabels disabled, add the
             * appropriate text to the menu entry itself... */
            if (is_rgui && !menu_show_sublabels)
            {
               /* This check is probably not required (it's not done
                * in menu_cbs_sublabel.c action_bind_sublabel_subsystem_add(),
                * anyway), but no harm in being safe... */
               if (subsystem->num_roms > 0)
               {
                  char tmp[PATH_MAX_LENGTH];

                  n = snprintf(tmp, sizeof(tmp),
                     "%s [%s %s]", s, "Current Content:",
                     subsystem->roms[0].desc);

                  /* More snprintf() gcc warning suppression... */
                  if ((n < 0) || (n >= PATH_MAX_LENGTH))
                  {
                     if (verbosity_is_enabled())
                     {
                        RARCH_WARN("Menu subsystem entry: Description label truncated.\n");
                     }
                  }

                  strlcpy(s, tmp, sizeof(s));
               }
            }

            if (menu_entries_append_enum(list,
               s,
               msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD),
               MENU_ENUM_LABEL_SUBSYSTEM_ADD,
               MENU_SETTINGS_SUBSYSTEM_ADD + i, 0, 0))
               count++;
         }
      }
   }

   return count;
}

unsigned menu_displaylist_build_list(
      file_list_t *list,
      settings_t *settings,
      enum menu_displaylist_ctl_state type,
      bool include_everything)
{
   unsigned i;
   unsigned count = 0;

   switch (type)
   {
      case DISPLAYLIST_SUBSYSTEM_SETTINGS_LIST:
         {
            runloop_state_t *runloop_st                  = runloop_state_get_ptr();
            const struct retro_subsystem_info* subsystem = runloop_st->subsystem_data;
            rarch_system_info_t *sys_info                = &runloop_st->system;
            /* Core not loaded completely, use the data we
             * peeked on load core */

            /* Core fully loaded, use the subsystem data */
            if (sys_info && sys_info->subsystem.data)
               subsystem = sys_info->subsystem.data;

            count = menu_displaylist_populate_subsystem(subsystem, settings,
                  list);
         }
         break;
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST),
                  MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;

         {
            bool playlist_show_sublabels = settings->bools.playlist_show_sublabels;
            bool history_list_enable     = settings->bools.history_list_enable;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_HISTORY_LIST_ENABLE,                 PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE,                PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE,              PARSE_ONLY_INT,  true},
               {MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME,               PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE,               PARSE_ONLY_UINT, true},
               {MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL,          PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT,             PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_COMPRESSION,                PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,      PARSE_ONLY_UINT, true},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_ENTRY_IDX,             PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS,             PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_SHOW_HISTORY_ICONS,         PARSE_ONLY_UINT, true},
               {MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,      PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE, PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,        PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH,             PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME,        PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME, PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG,                 PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE,       PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_PLAYLIST_PORTABLE_PATHS,             PARSE_ONLY_BOOL, true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE:
                  case MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE:
                     if (playlist_show_sublabels)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
                     if (history_list_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }

         break;
      case DISPLAYLIST_INPUT_TURBO_FIRE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_INPUT_TURBO_PERIOD,         PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_INPUT_DUTY_CYCLE,           PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_INPUT_TURBO_MODE,           PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_INPUT_TURBO_DEFAULT_BUTTON, PARSE_ONLY_UINT},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx, build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST:
         {
            const char *input_driver_id  = settings->arrays.input_driver;

            if (string_is_equal(input_driver_id, "android"))
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIBRATE_ON_KEYPRESS,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_ENABLE_DEVICE_VIBRATION,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
            }
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN,
                     PARSE_ONLY_UINT, false) == 0)
                  count++;
         }
         break;
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
         {
            bool hotkey_enable_found   = false;
            size_t hotkey_enable_index = 0;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_QUIT_PRESS_TWICE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_INPUT_QUIT_GAMEPAD_COMBO,
                     PARSE_ONLY_UINT, false) == 0)
               count++;

            /* Hotkey enable bind comes first - due to the
             * way binds are implemented, have to search the
             * entire list for it... */
            for (i = 0; i < RARCH_BIND_LIST_END; i++)
            {
               if (input_config_bind_map_get_retro_key(i) == RARCH_ENABLE_HOTKEY)
               {
                  hotkey_enable_found = true;
                  hotkey_enable_index = i;

                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           (enum msg_hash_enums)(
                              MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i),
                           PARSE_ONLY_BIND, false) == 0)
                     count++;

                  break;
               }
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_INPUT_HOTKEY_BLOCK_DELAY,
                     PARSE_ONLY_UINT, false) == 0)
               count++;

            /* All other binds come last */
            for (i = 0; i < RARCH_BIND_LIST_END; i++)
            {
               if (hotkey_enable_found && (hotkey_enable_index == i))
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        (enum msg_hash_enums)(
                           MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i),
                        PARSE_ONLY_BIND, false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SHADER_PRESET_REMOVE:
         {
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            const char *dir_video_shader  = settings->paths.directory_video_shader;
            const char *dir_menu_config   = settings->paths.directory_menu_config;
            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_GLOBAL,
                     dir_video_shader, dir_menu_config))
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_CORE,
                     dir_video_shader, dir_menu_config))
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_PARENT,
                     dir_video_shader, dir_menu_config))
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (menu_shader_manager_auto_preset_exists(SHADER_PRESET_GAME,
                     dir_video_shader, dir_menu_config))
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME),
                        MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
#endif
         }
         break;
      case DISPLAYLIST_SHADER_PRESET_SAVE:
         {
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            bool has_content = !string_is_empty(path_get(RARCH_PATH_CONTENT));

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (has_content && menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
            if (has_content && menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME),
                     MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#endif
         }
         break;
      case DISPLAYLIST_NETPLAY_ROOM_LIST:
#ifdef HAVE_NETWORKING
         count = menu_displaylist_netplay_refresh_rooms(list);
#endif
         break;
      case DISPLAYLIST_CONTENT_SETTINGS:
         count = menu_displaylist_parse_load_content_settings(list,
               settings, false);

         if (count == 0)
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
                     MENU_ENUM_LABEL_NO_ITEMS,
                     MENU_SETTING_NO_ITEM, 0, 0))
               count++;
         break;
      case DISPLAYLIST_BROWSE_URL_START:
#ifdef HAVE_NETWORKING
         {
            char link[1024];
            char name[1024];
            const char *line  = "<a href=\"http://www.test.com/somefile.zip\">Test</a>\n";

            link[0] = name[0] = '\0';

            string_parse_html_anchor(line, link, name, sizeof(link), sizeof(name));

            if (menu_entries_append_enum(list,
                  link,
                  name,
                  MSG_UNKNOWN,
                  0, 0, 0))
               count++;
         }
#endif
         break;
      case DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST:
         {
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
               if (menu_entries_append_enum(list, msg, msg_lbl,
                        MSG_UNKNOWN,
                        (MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN  +  i),
                        0, 0))
                  count++;
            }
#endif
         }
         break;
      case DISPLAYLIST_BLUETOOTH_SETTINGS_LIST:
#ifdef HAVE_BLUETOOTH
         {
            if (!string_is_equal(settings->arrays.bluetooth_driver, "null"))
            {
               struct string_list *device_list = string_list_new();
               driver_bluetooth_get_devices(device_list);

               if (device_list->size == 0)
                  task_push_bluetooth_scan(bluetooth_scan_callback);
               else
               {
                  unsigned i;
                  for (i = 0; i < device_list->size; i++)
                  {
                     const char *device = device_list->elems[i].data;
                     if (menu_entries_append_enum(list,
                              device,
                              msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_BLUETOOTH),
                              MENU_ENUM_LABEL_CONNECT_BLUETOOTH,
                              MENU_BLUETOOTH, 0, 0))
                        count++;
                  }
               }
            }
         }
#endif
         break;
      case DISPLAYLIST_WIFI_SETTINGS_LIST:
#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
         {
            bool wifi_enabled = settings->bools.wifi_enabled;
            bool connected    = driver_wifi_connection_info(NULL);

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_WIFI_ENABLED,              PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_WIFI_NETWORK_SCAN,         PARSE_ACTION,    false},
               {MENU_ENUM_LABEL_WIFI_DISCONNECT,           PARSE_ACTION,    false},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_WIFI_NETWORK_SCAN:
                     build_list[i].checked = wifi_enabled;
                     break;
                  case MENU_ENUM_LABEL_WIFI_DISCONNECT:
                     build_list[i].checked = wifi_enabled && connected;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (build_list[i].checked &&
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
#endif
         break;
      case DISPLAYLIST_WIFI_NETWORKS_LIST:
#if defined(HAVE_NETWORKING) && defined(HAVE_WIFI)
         if (!string_is_equal(settings->arrays.wifi_driver, "null"))
         {
            wifi_network_scan_t *scan = driver_wifi_get_ssids();

            /* Temporary hack: scan periodically, until we have a submenu */
            if (!scan || time(NULL) > scan->scan_time + 30)
               task_push_wifi_scan(wifi_scan_callback);
            else
            {
               unsigned i;
               for (i = 0; i < RBUF_LEN(scan->net_list); i++)
               {
                  const char *ssid = scan->net_list[i].ssid;
                  if (menu_entries_append_enum(list,
                           string_is_empty(ssid) ? msg_hash_to_str(MSG_WIFI_EMPTY_SSID) : ssid,
                           msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_WIFI),
                           MENU_ENUM_LABEL_CONNECT_WIFI,
                           MENU_WIFI, 0, 0))
                     count++;
               }
            }
         }
#endif
         break;
      case DISPLAYLIST_SYSTEM_INFO:
         count              = menu_displaylist_parse_system_info(list);
         break;
      case DISPLAYLIST_EXPLORE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
#if defined(HAVE_LIBRETRODB)
         count              = menu_displaylist_explore(list, settings);
#endif
         break;
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
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
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST),
                  MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;
         break;
      case DISPLAYLIST_INFORMATION_LIST:
         count              = menu_displaylist_parse_information_list(list);
         break;
      case DISPLAYLIST_HELP_SCREEN_LIST:
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS),
                  msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS),
                  MENU_ENUM_LABEL_HELP_CONTROLS,
                  0, 0, 0))
            count++;
         break;
      case DISPLAYLIST_AUDIO_RESAMPLER_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER,
                  PARSE_ONLY_STRING_OPTIONS, false) == 0)
            count++;

         if (string_is_not_equal(settings->arrays.audio_resampler, "null"))
         {
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
         }
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         break;
      case DISPLAYLIST_AUDIO_OUTPUT_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_DRIVER,
                  PARSE_ONLY_STRING_OPTIONS, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_DEVICE,
                  PARSE_ONLY_STRING, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_LATENCY,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
                  PARSE_ONLY_INT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         break;
      case DISPLAYLIST_AUDIO_SYNCHRONIZATION_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_SYNC,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         break;
      case DISPLAYLIST_AUDIO_SETTINGS_LIST:
      {
         bool audio_mute_enable       = *audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE);
#if defined(HAVE_AUDIOMIXER)
         bool audio_mixer_mute_enable = *audio_get_bool_ptr(AUDIO_ACTION_MIXER_MUTE_ENABLE);
#else
         bool audio_mixer_mute_enable = true;
#endif
         menu_displaylist_build_info_selective_t build_list[] = {
            {MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS,           PARSE_ACTION,     true  },
            {MENU_ENUM_LABEL_AUDIO_RESAMPLER_SETTINGS,        PARSE_ACTION,     true  },
            {MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS,  PARSE_ACTION,     true  },
            {MENU_ENUM_LABEL_MIDI_SETTINGS,                   PARSE_ACTION,     true  },
            {MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,            PARSE_ACTION,     false },
            {MENU_ENUM_LABEL_MENU_SOUNDS,                     PARSE_ACTION,     true  },
            {MENU_ENUM_LABEL_AUDIO_MUTE,                      PARSE_ONLY_BOOL,  true  },
            {MENU_ENUM_LABEL_AUDIO_MIXER_MUTE,                PARSE_ONLY_BOOL,  true  },
            {MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE,          PARSE_ONLY_BOOL,  true  },
            {MENU_ENUM_LABEL_AUDIO_VOLUME,                    PARSE_ONLY_FLOAT, false },
            {MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME,              PARSE_ONLY_FLOAT, false },
            {MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE,               PARSE_ONLY_BOOL,  true  },
#if defined(HAVE_DSP_FILTER)
            {MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,                PARSE_ONLY_PATH,  true  },
#endif
         };

         for (i = 0; i < ARRAY_SIZE(build_list); i++)
         {
            switch (build_list[i].enum_idx)
            {
               case MENU_ENUM_LABEL_AUDIO_VOLUME:
                  if (!audio_mute_enable)
                     build_list[i].checked = true;
                  break;
               case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
               case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
                  if (!audio_mixer_mute_enable)
                     build_list[i].checked = true;
                  break;
               default:
                  break;
            }
         }

         for (i = 0; i < ARRAY_SIZE(build_list); i++)
         {
            if (!build_list[i].checked && !include_everything)
               continue;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     build_list[i].enum_idx,  build_list[i].parse_type,
                     false) == 0)
               count++;
         }
      }

#if defined(HAVE_DSP_FILTER)
         if (!string_is_empty(settings->paths.path_audio_dsp_plugin))
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE),
                     msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN_REMOVE),
                     MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN_REMOVE,
                     MENU_SETTING_ACTION_AUDIO_DSP_PLUGIN_REMOVE, 0, 0))
               count++;
#endif
         break;
      case DISPLAYLIST_VIDEO_SETTINGS_LIST:
         {
            gfx_ctx_flags_t flags;

            if (video_display_server_get_flags(&flags))
            {
               if (BIT32_GET(flags.flags, DISPSERV_CTX_CRT_SWITCHRES))
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,
                           PARSE_ACTION, false) == 0)
                     count++;
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_OUTPUT_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_WINDOWED_MODE_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SCALING_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_HDR_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
                     PARSE_ACTION, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_BRIGHTNESS_CONTROL,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_THREADED,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
#ifdef HAVE_SCREENSHOTS
            if (video_driver_supports_viewport_read())
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SMOOTH,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_CTX_SCALING,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
#if defined(DINGUX)
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
#if defined(RS90) || defined(MIYOO)
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
#endif
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SHADER_DELAY,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
#ifdef HAVE_VIDEO_FILTER
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_FILTER,
                     PARSE_ONLY_PATH, false) == 0)
               count++;

            if (!string_is_empty(settings->paths.path_softfilter_plugin))
               if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FILTER_REMOVE),
                     MENU_ENUM_LABEL_VIDEO_FILTER_REMOVE,
                     MENU_SETTING_ACTION_VIDEO_FILTER_REMOVE, 0, 0))
                  count++;
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_NOTCH_WRITE_OVER,
                     PARSE_ONLY_BOOL, false) == 0)
                     count++;
         }
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
         {
            unsigned p;
            unsigned max_users = settings->uints.input_max_users;

#ifdef HAVE_CONFIGFILE
            if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_REMAP_FILE_MANAGER_LIST),
                  MENU_ENUM_LABEL_REMAP_FILE_MANAGER_LIST,
                  MENU_SETTING_ACTION_REMAP_FILE_MANAGER_LIST, 0, 0))
               count++;
#endif
            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS),
                     MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            for (p = 0; p < max_users; p++)
            {
               char val_s[256], val_d[16];
               snprintf(val_s, sizeof(val_s),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS),
                     p+1);
               snprintf(val_d, sizeof(val_d), "%d", p);
               if (menu_entries_append_enum(list, val_s, val_d,
                        MSG_UNKNOWN,
                        MENU_SETTINGS_REMAPPING_PORT_BEGIN + p, p, 0))
                  count++;
            }
         }
         break;
      case DISPLAYLIST_LOAD_CONTENT_LIST:
      case DISPLAYLIST_LOAD_CONTENT_SPECIAL:
         {
            const char *dir_menu_content     = settings->paths.directory_menu_content;
            bool menu_content_show_favorites = settings->bools.menu_content_show_favorites;

            if (!string_is_empty(dir_menu_content))
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                        msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                        MENU_ENUM_LABEL_FAVORITES,
                        MENU_SETTING_ACTION_FAVORITES_DIR, 0, 0))
                  count++;

#if defined(HAVE_LIBRETRODB)
            if (settings->bools.menu_content_show_explore)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_EXPLORE),
                        MENU_ENUM_LABEL_GOTO_EXPLORE,
                        MENU_EXPLORE_TAB, 0, 0))
                  count++;
#endif

#if defined(HAVE_DYNAMIC)
            if (settings->uints.menu_content_show_contentless_cores !=
                  MENU_CONTENTLESS_CORES_DISPLAY_NONE)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES),
                        MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES,
                        MENU_CONTENTLESS_CORES_TAB, 0, 0))
                  count++;
#endif
            if (menu_content_show_favorites)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_FAVORITES),
                        MENU_ENUM_LABEL_GOTO_FAVORITES,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_images)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_IMAGES),
                        MENU_ENUM_LABEL_GOTO_IMAGES,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

            if (settings->bools.menu_content_show_music)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_MUSIC),
                        MENU_ENUM_LABEL_GOTO_MUSIC,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            if (settings->bools.menu_content_show_video)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO),
                        msg_hash_to_str(MENU_ENUM_LABEL_GOTO_VIDEO),
                        MENU_ENUM_LABEL_GOTO_VIDEO,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
#endif
         }

         {
            core_info_list_t *info_list        = NULL;
            core_info_get_list(&info_list);
            if (info_list && info_list->info_count > 0)
            {
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                        msg_hash_to_str(
                           MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                        MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }
         }

         {
            bool menu_content_show_playlists = settings->bools.menu_content_show_playlists;
            if (menu_content_show_playlists)
               if (menu_entries_append_enum(list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                        msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                        MENU_ENUM_LABEL_PLAYLISTS_TAB,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
         }

         if (frontend_driver_parse_drive_list(list, true) != 0)
            if (menu_entries_append_enum(list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

#if 0
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL_LIST),
                  MENU_ENUM_LABEL_BROWSE_URL_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;
#endif
         break;
      case DISPLAYLIST_INPUT_MENU_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_MENU_SCROLL_FAST,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_MENU_SCROLL_DELAY,
                  PARSE_ONLY_UINT, false) == 0)
            count++;

         break;
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_MAX_USERS,
                  PARSE_ONLY_UINT, false) == 0)
            count++;

         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;

         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_ICADE_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
#if defined(GEKKO)
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_MOUSE_SCALE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
#endif
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_TOUCH_SCALE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_ANALOG_DEADZONE,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_ANALOG_SENSITIVITY,
                  PARSE_ONLY_FLOAT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_BIND_HOLD,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_BIND_MODE,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_NOWINKEY_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
#endif
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_SENSORS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_AUTO_MOUSE_GRAB,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_AUTO_GAME_FOCUS,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
                  PARSE_ACTION, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_MENU_SETTINGS,
                  PARSE_ACTION, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,
                  PARSE_ACTION, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS,
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

               if (MENU_DISPLAYLIST_PARSE_SETTINGS(list,
                        key_split_joycon, PARSE_ONLY_UINT, true, 0) != -1)
                  count++;
            }
         }
#endif

         {
            unsigned user;
            unsigned max_users          = settings->uints.input_max_users;
            for (user = 0; user < max_users; user++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user),
                        PARSE_ACTION, false) != -1)
                  count++;
            }

         }
         break;
      case DISPLAYLIST_ACCESSIBILITY_SETTINGS_LIST:
         {
            bool accessibility_enable      = settings->bools.accessibility_enable;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_ACCESSIBILITY_ENABLED,                                             PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,                               PARSE_ONLY_UINT, false  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED:
                     if (accessibility_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_AI_SERVICE_SETTINGS_LIST:
         {
            bool ai_service_enable         = settings->bools.ai_service_enable;

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_AI_SERVICE_ENABLE,      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_AI_SERVICE_MODE,        PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_AI_SERVICE_URL,         PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_AI_SERVICE_PAUSE,       PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG, PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG, PARSE_ONLY_UINT,   false},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_AI_SERVICE_MODE:
                  case MENU_ENUM_LABEL_AI_SERVICE_URL:
                  case MENU_ENUM_LABEL_AI_SERVICE_PAUSE:
                  case MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG:
                  case MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG:
                     if (ai_service_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST),
                  msg_hash_to_str(MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST),
                  MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST,
                  MENU_SETTING_ACTION, 0, 0))
            count++;
         break;
      case DISPLAYLIST_NETWORK_INFO:
#if defined(HAVE_NETWORKING) && (!defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(SWITCH) && defined(HAVE_LIBNX)) || defined(GEKKO))
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
#ifdef HAVE_CHEATS
         if (cheat_manager_alloc_if_empty())
         {
            menu_search_terms_t *search_terms= menu_entries_search_get_terms();
            bool search_active               = search_terms && (search_terms->size > 0);
            unsigned num_cheats              = cheat_manager_get_size();
            unsigned num_cheats_shown        = 0;
            unsigned i;
            char on_string[32];
            char off_string[32];

            on_string[0]  = '\0';
            off_string[0] = '\0';

            /* If a search is active, all options are
             * omitted apart from 'apply changes' */
            if (search_active)
            {
               /* On/off key strings may be required,
                * so populate them... */
               snprintf(on_string, sizeof(on_string), ".%s",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON));

               snprintf(off_string, sizeof(off_string), ".%s",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF));
            }
            else
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
            }

            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES),
                     msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES),
                     MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            for (i = 0; i < num_cheats; i++)
            {
               const char *cheat_description = cheat_manager_get_desc(i);
               char cheat_label[128];

               cheat_label[0] = '\0';

               snprintf(cheat_label, sizeof(cheat_label),
                     "%s #%u: ", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT), i);
               if (!string_is_empty(cheat_description))
                  strlcat(cheat_label, cheat_description, sizeof(cheat_label));

               /* If a search is active, skip non-matching
                * entries */
               if (search_active)
               {
                  bool entry_valid = true;
                  size_t j;

                  for (j = 0; j < search_terms->size; j++)
                  {
                     const char *search_term = search_terms->terms[j];

                     if (!string_is_empty(search_term))
                     {
                        bool cheat_on = cheat_manager_get_code_state(i);

                        /* Check for 'on' keyword */
                        if (string_is_equal_noncase(search_term, on_string))
                        {
                           if (!cheat_on)
                              entry_valid = false;
                        }
                        /* Check for 'off' keyword */
                        else if (string_is_equal_noncase(search_term, off_string))
                        {
                           if (cheat_on)
                              entry_valid = false;
                        }
                        /* Normal label comparison */
                        else if (!strcasestr(cheat_label, search_term))
                           entry_valid = false;
                     }

                     if (!entry_valid)
                        break;
                  }

                  if (!entry_valid)
                     continue;
               }

               if (menu_entries_append_enum(list,
                     cheat_label, "", MSG_UNKNOWN,
                     MENU_SETTINGS_CHEAT_BEGIN + i, 0, 0))
               {
                  num_cheats_shown++;
                  count++;
               }
            }

            /* If a search is active and no results are
             * found, show a 'no entries available' item */
            if (search_active &&
                (num_cheats_shown < 1) &&
                menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                     MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                     FILE_TYPE_NONE, 0, 0))
               count++;
         }
#endif
         break;
      case DISPLAYLIST_DROPDOWN_LIST_RESOLUTION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
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
                  {
                     menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                     if (cbs)
                        cbs->checked = true;
                     menu_navigation_set_selection(i);
                  }
               }

               free(video_list);
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
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
               {
                  menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[0].actiondata;
                  if (cbs)
                     cbs->checked = true;
                  menu_navigation_set_selection(0);
               }

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
                     {
                        menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i + 1].actiondata;
                        if (cbs)
                           cbs->checked = true;
                        menu_navigation_set_selection(i + 1);
                     }
                  }
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
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
                  {
                     menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                     if (cbs)
                        cbs->checked = true;
                     menu_navigation_set_selection(i);
                  }
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         count = populate_playlist_thumbnail_mode_dropdown_list(list, PLAYLIST_THUMBNAIL_RIGHT);
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         count = populate_playlist_thumbnail_mode_dropdown_list(list, PLAYLIST_THUMBNAIL_LEFT);
         break;
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_SORT_MODE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         {
            playlist_t *playlist = playlist_get_cached();

            if (playlist)
            {
               size_t i;
               /* Get current sort mode */
               enum playlist_sort_mode current_sort_mode =
                  playlist_get_sort_mode(playlist);

               /* Loop over all defined sort modes */
               for (i = 0; i <= (unsigned)PLAYLIST_SORT_MODE_OFF; i++)
               {
                  enum msg_hash_enums label_value;
                  enum playlist_sort_mode sort_mode =
                     (enum playlist_sort_mode)i;

                  /* Get appropriate entry label */
                  switch (sort_mode)
                  {
                     case PLAYLIST_SORT_MODE_ALPHABETICAL:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL;
                        break;
                     case PLAYLIST_SORT_MODE_OFF:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF;
                        break;
                     case PLAYLIST_SORT_MODE_DEFAULT:
                     default:
                        label_value = MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT;
                        break;
                  }

                  /* Add entry */
                  if (menu_entries_append_enum(list,
                           msg_hash_to_str(label_value),
                           "",
                           MENU_ENUM_LABEL_NO_ITEMS,
                           MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_SORT_MODE,
                           0, 0))
                     count++;

                  /* Check whether current entry is checked */
                  if (current_sort_mode == sort_mode)
                  {
                     menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                     if (cbs)
                        cbs->checked = true;
                     menu_navigation_set_selection(i);
                  }
               }
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         /* Get system name list */
         {
            bool show_hidden_files            = settings->bools.show_hidden_files;
#ifdef HAVE_LIBRETRODB
            const char *path_content_database = settings->paths.path_content_database;
            struct string_list *system_name_list =
               manual_content_scan_get_menu_system_name_list(
                     path_content_database,
                     show_hidden_files);
#else
            struct string_list *system_name_list =
               manual_content_scan_get_menu_system_name_list(NULL,
                     show_hidden_files);
#endif

            if (system_name_list)
            {
               const char *current_system_name = NULL;
               unsigned i;

               /* Get currently selected system name */
               manual_content_scan_get_menu_system_name(&current_system_name);

               /* Loop through names */
               for (i = 0; i < system_name_list->size; i++)
               {
                  /* Note: manual_content_scan_get_system_name_list()
                   * ensures that system_name cannot be empty here */
                  const char *system_name = system_name_list->elems[i].data;

                  /* Add menu entry */
                  if (menu_entries_append_enum(list,
                           system_name,
                           "",
                           MENU_ENUM_LABEL_NO_ITEMS,
                           MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
                           i, 0))
                     count++;

                  /* Check whether current entry is checked */
                  if (string_is_equal(current_system_name, system_name))
                  {
                     menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                     if (cbs)
                        cbs->checked = true;
                     menu_navigation_set_selection(i);
                  }
               }

               /* Clean up */
               string_list_free(system_name_list);
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_CORE_NAME:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         {
            /* Get core name list */
            struct string_list *core_name_list =
               manual_content_scan_get_menu_core_name_list();

            if (core_name_list)
            {
               const char *current_core_name = NULL;
               unsigned i;

               /* Get currently selected core name */
               manual_content_scan_get_menu_core_name(&current_core_name);

               /* Loop through names */
               for (i = 0; i < core_name_list->size; i++)
               {
                  /* Note: manual_content_scan_get_core_name_list()
                   * ensures that core_name cannot be empty here */
                  const char *core_name = core_name_list->elems[i].data;

                  /* Add menu entry */
                  if (menu_entries_append_enum(list,
                           core_name,
                           "",
                           MENU_ENUM_LABEL_NO_ITEMS,
                           MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_CORE_NAME,
                           i, 0))
                     count++;

                  /* Check whether current entry is checked */
                  if (string_is_equal(current_core_name, core_name))
                  {
                     menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                     if (cbs)
                        cbs->checked = true;
                     menu_navigation_set_selection(i);
                  }
               }

               /* Clean up */
               string_list_free(core_name_list);
            }
         }
         break;
      case DISPLAYLIST_DROPDOWN_LIST_DISK_INDEX:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);
         {
            rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;

            if (sys_info)
            {
               if (disk_control_enabled(&sys_info->disk_control))
               {
                  unsigned num_images    =
                        disk_control_get_num_images(&sys_info->disk_control);
                  unsigned current_image =
                        disk_control_get_image_index(&sys_info->disk_control);
                  unsigned num_digits    = 0;
                  unsigned i;

                  /* If core supports labels, index value string
                   * should be padded to maximum width (otherwise
                   * labels will be misaligned/ugly) */
                  if (disk_control_image_label_enabled(&sys_info->disk_control))
                  {
                     unsigned digit_counter = num_images;
                     do
                     {
                        num_digits++;
                        digit_counter = digit_counter / 10;
                     }
                     while (digit_counter > 0);
                  }

                  /* Loop through disk images */
                  for (i = 0; i < num_images; i++)
                  {
                     char current_image_str[PATH_MAX_LENGTH];
                     char image_label[128];

                     current_image_str[0] = '\0';
                     image_label[0]       = '\0';

                     /* Get image label, if supported by core */
                     disk_control_get_image_label(
                           &sys_info->disk_control,
                           i, image_label, sizeof(image_label));

                     /* Get string representation of disk index
                      * > Note that displayed index starts at '1',
                      *   not '0' */
                     if (!string_is_empty(image_label))
                     {
                        /* Note: 2-space gap is intentional
                         * (for clarity) */
                        int n = snprintf(
                              current_image_str, sizeof(current_image_str),
                              "%0*u:  %s", num_digits, i + 1, image_label);

                        /* Suppress GCC warnings... */
                        if ((n < 0) || (n >= PATH_MAX_LENGTH))
                           n = 0;
                        (void)n;
                     }
                     else
                        snprintf(
                              current_image_str, sizeof(current_image_str),
                              "%0*u", num_digits, i + 1);

                     /* Add menu entry */
                     if (menu_entries_append_enum(list,
                              current_image_str,
                              "",
                              MENU_ENUM_LABEL_NO_ITEMS,
                              MENU_SETTING_DROPDOWN_ITEM_DISK_INDEX,
                              i, 0))
                        count++;

                     /* Check whether current disk is selected */
                     if (i == current_image)
                     {
                        menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)list->list[i].actiondata;
                        if (cbs)
                           cbs->checked = true;
                        menu_navigation_set_selection(i);
                     }
                  }
               }
            }
         }
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
            bool netplay_allow_slaves    = settings->bools.netplay_allow_slaves;
            bool netplay_use_mitm_server = settings->bools.netplay_use_mitm_server;
            bool network_cmd_enable      = settings->bools.network_cmd_enable;

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,                               PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,                               PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,                                   PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER,                            PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS,                                    PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT,                                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS,                               PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_MAX_PING,                                      PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_PASSWORD,                                      PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD,                             PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR,                            PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_FADE_CHAT,                                     PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_ALLOW_PAUSING,                                 PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES,                                  PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES,                                PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE,                                PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES,                                  PARSE_ONLY_INT,    true},
               {MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,                      PARSE_ONLY_INT,    true},
               {MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,                    PARSE_ONLY_INT,    true},
               {MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL,                                 PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL,                                 PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG,                                  PARSE_ONLY_UINT,   true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
                     if (netplay_allow_slaves)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
                     if (netplay_use_mitm_server)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER:
                     if (netplay_use_mitm_server)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }

            {
               unsigned user;
               for (user = 0; user < MAX_USERS; user++)
               {
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           (enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + user),
                           PARSE_ONLY_BOOL, false) != -1)
                     count++;
               }
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_NETWORK_CMD_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (network_cmd_enable)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_NETWORK_CMD_PORT,
                        PARSE_ONLY_UINT, false) != -1)
                  count++;
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_NETWORK_REMOTE_PORT,
                     PARSE_ONLY_UINT, false) != -1)
               count++;

            {
               unsigned user;
               unsigned max_users          = settings->uints.input_max_users;
               for (user = 0; user < max_users; user++)
               {
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           (enum msg_hash_enums)(
                              MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user),
                           PARSE_ONLY_BOOL, false) != -1)
                     count++;
               }
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_STDIN_CMD_ENABLE,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS,
                     PARSE_ONLY_BOOL, false) != -1)
               count++;

#ifdef HAVE_ONLINE_UPDATER
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_UPDATER_SETTINGS,
                     PARSE_ACTION, false) != -1)
               count++;
#endif
         }
         break;
      case DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST:
#ifdef HAVE_CHEATS
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_CHEAT_DELETE_MATCH,
                     PARSE_ONLY_UINT, false) != -1)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
#endif
         break;
      case DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST:
#ifdef HAVE_CHEATS
         {
            if (!cheat_manager_state.memory_initialized)
               cheat_manager_initialize_memory(NULL, 0, true);

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
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           build_list[i].enum_idx,  build_list[i].parse_type,
                           false) == 0)
                     count++;
               }
            }

            if (cheat_manager_state.working_cheat.handler == CHEAT_HANDLER_TYPE_EMU)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_CHEAT_CODE,
                        PARSE_ONLY_STRING, false) == 0)
                  count++;
            }
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
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
#endif
         break;
      case DISPLAYLIST_RECORDING_SETTINGS_LIST:
         {
            unsigned streaming_mode        = settings->uints.streaming_mode;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY,                                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_RECORD_CONFIG,                                         PARSE_ONLY_PATH,   true},
               {MENU_ENUM_LABEL_VIDEO_RECORD_THREADS,                                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD,                              PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_VIDEO_GPU_RECORD,                                      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_STREAMING_MODE,                                        PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY,                                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_STREAM_CONFIG,                                         PARSE_ONLY_PATH,   true},
               {MENU_ENUM_LABEL_STREAMING_TITLE,                                       PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_STREAMING_URL,                                         PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_UDP_STREAM_PORT,                                       PARSE_ONLY_UINT,   true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_UDP_STREAM_PORT:
                     if (streaming_mode == STREAMING_MODE_LOCAL)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (build_list[i].checked &&
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         {
            bool cheevos_enable       = settings->bools.cheevos_enable;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_CHEEVOS_ENABLE,                                        PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_CHEEVOS_USERNAME,                                      PARSE_ONLY_STRING, false },
               {MENU_ENUM_LABEL_CHEEVOS_PASSWORD,                                      PARSE_ONLY_STRING, false },
               {MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE,                          PARSE_ONLY_BOOL,   false  },
               {MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE,                           PARSE_ONLY_STRING_OPTIONS,   false  },
               {MENU_ENUM_LABEL_CHEEVOS_CHALLENGE_INDICATORS,                          PARSE_ONLY_BOOL,   false  },
               {MENU_ENUM_LABEL_CHEEVOS_RICHPRESENCE_ENABLE,                           PARSE_ONLY_BOOL,   false  },
#ifndef HAVE_GFX_WIDGETS
               {MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE,                                 PARSE_ONLY_BOOL,   false  },
#endif
               {MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL,                               PARSE_ONLY_BOOL,   false  },
#ifdef HAVE_AUDIOMIXER
               {MENU_ENUM_LABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,                           PARSE_ONLY_BOOL,   false  },
#endif
#ifdef HAVE_SCREENSHOTS
               {MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT,                               PARSE_ONLY_BOOL,   false  },
#endif
               {MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE,                                PARSE_ONLY_BOOL,   false  },
               {MENU_ENUM_LABEL_CHEEVOS_START_ACTIVE,                                  PARSE_ONLY_BOOL,   false  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (cheevos_enable)
                  build_list[i].checked = true;
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ACCOUNTS_TWITCH_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_TWITCH_STREAM_KEY,
                  PARSE_ONLY_STRING, false) == 0)
            count++;
         break;
      case DISPLAYLIST_ACCOUNTS_FACEBOOK_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_FACEBOOK_STREAM_KEY,
                  PARSE_ONLY_STRING, false) == 0)
            count++;
         break;
      case DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST:
         {
            bool kiosk_mode_enable                                  = settings->bools.kiosk_mode_enable;
            bool desktop_menu_enable                                = settings->bools.desktop_menu_enable;
            bool menu_screensaver_supported                         = menu_driver_screensaver_supported();
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
            enum menu_screensaver_effect menu_screensaver_animation =
                  (enum menu_screensaver_effect)settings->uints.menu_screensaver_animation;
#endif

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS,                                   PARSE_ACTION,      true},
               {MENU_ENUM_LABEL_MENU_SETTINGS,                                         PARSE_ACTION,      true},
               {MENU_ENUM_LABEL_MENU_DRIVER,                                           PARSE_ONLY_STRING_OPTIONS, true},
               {MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS,                                PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE,                                PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD,                              PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND,                                 PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_PAUSE_LIBRETRO,                                        PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_PAUSE_NONACTIVE,                                       PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_SAVESTATE_RESUME,                                 PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_INSERT_DISK_RESUME,                               PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_QUIT_ON_CLOSE_CONTENT,                                 PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_SCREENSAVER_TIMEOUT,                              PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION,                            PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION_SPEED,                      PARSE_ONLY_FLOAT,  false},
               {MENU_ENUM_LABEL_MOUSE_ENABLE,                                          PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_POINTER_ENABLE,                                        PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE,                          PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_UI_COMPANION_ENABLE,                                   PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT,                            PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_UI_MENUBAR_ENABLE,                                     PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE,                                   PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_UI_COMPANION_TOGGLE,                                   PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION,                             PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_VIDEO_3DS_DISPLAY_MODE,                                PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_VIDEO_3DS_LCD_BOTTOM,                                  PARSE_ONLY_BOOL,   true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD:
                     if (kiosk_mode_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_SCREENSAVER_TIMEOUT:
                     if (menu_screensaver_supported)
                        build_list[i].checked = true;
                     break;
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
                  case MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION:
                     if (menu_screensaver_supported)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_SCREENSAVER_ANIMATION_SPEED:
                     if (menu_screensaver_supported &&
                         (menu_screensaver_animation != MENU_SCREENSAVER_BLANK))
                        build_list[i].checked = true;
                     break;
#endif
                  case MENU_ENUM_LABEL_UI_COMPANION_TOGGLE:
                     if (desktop_menu_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;
               if (
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_OPTIONS_DISK:
         count = menu_displaylist_parse_disk_options(list);
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_VIDEO_WINDOWED_MODE_SETTINGS_LIST:
         {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
            bool window_custom_size_enable = settings->bools.video_window_save_positions;
#else
            bool window_custom_size_enable = settings->bools.video_window_custom_size_enable;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
               {MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION,      PARSE_ONLY_BOOL,  true },
#else
               {MENU_ENUM_LABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE, PARSE_ONLY_BOOL,  true },
#endif
               {MENU_ENUM_LABEL_VIDEO_SCALE,                     PARSE_ONLY_FLOAT, false},
               {MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH,              PARSE_ONLY_UINT,  false},
               {MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT,             PARSE_ONLY_UINT,  false},
               {MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,     PARSE_ONLY_UINT,  false},
               {MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,    PARSE_ONLY_UINT,  false},
               {MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY,            PARSE_ONLY_UINT,  true },
               {MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS,   PARSE_ONLY_BOOL,  true },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_VIDEO_SCALE:
                     if (!window_custom_size_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH:
                  case MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT:
                     if (window_custom_size_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX:
                  case MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX:
                     if (!window_custom_size_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx, build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST:
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_VIDEO_FULLSCREEN,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y,
                  PARSE_ONLY_UINT, false) == 0)
            count++;
#ifdef __WINRT__
         if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                  MENU_ENUM_LABEL_VIDEO_FORCE_RESOLUTION,
                  PARSE_ONLY_BOOL, false) == 0)
            count++;
#endif
         break;
      case DISPLAYLIST_VIDEO_OUTPUT_SETTINGS_LIST:
         {
            bool *threaded = video_driver_get_threaded();
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_DRIVER,
                     PARSE_ONLY_STRING_OPTIONS, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_GPU_INDEX,
                     PARSE_ONLY_INT, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX,
                     PARSE_ONLY_UINT, false) == 0)
               count++;

#if defined(WIIU)
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_WIIU_PREFER_DRC,
                     PARSE_ONLY_BOOL, false) == 0)
                     count++;
#endif

#if defined(GEKKO) || defined(PS2) || !defined(__PSL1GHT__) && defined(__PS3__)
            if (true)
#else
               if (video_display_server_has_resolution_list())
#endif
               {
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_SCREEN_RESOLUTION,
                           PARSE_ACTION, false) == 0)
                     count++;
               }
#if defined(HAVE_WINDOW_OFFSET)
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_X,
                     PARSE_ONLY_INT, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_WINDOW_OFFSET_Y,
                     PARSE_ONLY_INT, false) == 0)
               count++;
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_PAL60_ENABLE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_GAMMA,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_SOFT_FILTER,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_ROTATION,
                     PARSE_ONLY_UINT, false) == 0)
               count++;

            if (video_display_server_can_set_screen_orientation())
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_SCREEN_ORIENTATION,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
#if defined(DINGUX) && defined(DINGUX_BETA)
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_DINGUX_REFRESH_RATE,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
#endif
            if (threaded && !*threaded)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_REFRESH_RATE,
                        PARSE_ONLY_FLOAT, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO,
                        PARSE_ONLY_FLOAT, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED,
                        PARSE_ONLY_FLOAT, false) == 0)
                  count++;
            }
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
         }
         break;
      case DISPLAYLIST_VIDEO_SYNCHRONIZATION_SETTINGS_LIST:
         {
            bool video_vsync          = settings->bools.video_vsync;
            bool video_hard_sync      = settings->bools.video_hard_sync;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_VSYNC,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;

            if (video_vsync)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
            }

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_HARD_SYNC))
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (video_hard_sync)
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
                           PARSE_ONLY_UINT, false) == 0)
                     count++;
            }

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES))
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
         }
         break;
      case DISPLAYLIST_VIDEO_HDR_SETTINGS_LIST:
         {
            if (video_driver_supports_hdr())
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_HDR_ENABLE,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (settings->bools.video_hdr_enable)
               {
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_HDR_MAX_NITS,
                           PARSE_ONLY_FLOAT, false) == 0)
                     count++;
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS,
                           PARSE_ONLY_FLOAT, false) == 0)
                     count++;
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_HDR_CONTRAST,
                           PARSE_ONLY_FLOAT, false) == 0)
                     count++;
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT,
                           PARSE_ONLY_BOOL, false) == 0)
                     count++;
               }
            }
         }
         break;
      case DISPLAYLIST_VIDEO_SCALING_SETTINGS_LIST:
         {
#if defined(DINGUX)
            if (string_is_equal(settings->arrays.video_driver, "sdl_dingux") ||
                string_is_equal(settings->arrays.video_driver, "sdl_rs90"))
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
            }
            else
#endif
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
               if (settings->bools.video_scale_integer)
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                           MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
                           PARSE_ONLY_BOOL, false) == 0)
                     count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX,
                        PARSE_ONLY_UINT, false) == 0)
                  count++;
               switch (settings->uints.video_aspect_ratio_idx)
               {
                  case ASPECT_RATIO_CONFIG:
                     if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                              MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO,
                              PARSE_ONLY_FLOAT, false) == 0)
                        count++;
                     break;
                  case ASPECT_RATIO_CUSTOM:
                     if (!settings->bools.video_scale_integer)
                     {
                        if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                                 MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X,
                                 PARSE_ONLY_INT, false) == 0)
                           count++;
                        if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                                 MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y,
                                 PARSE_ONLY_INT, false) == 0)
                           count++;
                     }
                     if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                              MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
                              PARSE_ONLY_UINT, false) == 0)
                        count++;
                     if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                              MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
                              PARSE_ONLY_UINT, false) == 0)
                        count++;
                     break;
                  default:
                     break;
               }
            }

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_VI_WIDTH,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_VFILTER,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
                     PARSE_ONLY_UINT, false) == 0)
               count++;
         }
         break;
      case DISPLAYLIST_CRT_SWITCHRES_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION,                                 PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER,                           PARSE_ONLY_UINT},
               {MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING,                           PARSE_ONLY_INT },
               {MENU_ENUM_LABEL_CRT_SWITCH_PORCH_ADJUST,                               PARSE_ONLY_INT },
               {MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,         PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CRT_SWITCH_HIRES_MENU,         PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#ifdef HAVE_LAKKA
      case DISPLAYLIST_LAKKA_SERVICES_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SSH_ENABLE,                                            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SAMBA_ENABLE,                                          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_BLUETOOTH_ENABLE,                                      PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_LOCALAP_ENABLE,                                        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_TIMEZONE,                                              PARSE_ONLY_STRING_OPTIONS},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#endif
      case DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS,                             PARSE_ACTION, true     },
               {MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS,                             PARSE_ACTION, true     },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_DISC,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_DUMP_DISC,                                   PARSE_ONLY_BOOL, true  },
#ifdef HAVE_LAKKA
               {MENU_ENUM_LABEL_MENU_SHOW_EJECT_DISC,                                  PARSE_ONLY_BOOL, true  },
#endif
#ifdef HAVE_ONLINE_UPDATER
               {MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER,                              PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,                    PARSE_ONLY_BOOL, true  },
#endif
#ifdef HAVE_MIST
               {MENU_ENUM_LABEL_MENU_SHOW_CORE_MANAGER_STEAM,                          PARSE_ONLY_BOOL, true  },
#endif
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
               {MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE,                                  PARSE_ONLY_BOOL, true  },
#if defined(HAVE_DYNAMIC)
               {MENU_ENUM_LABEL_CONTENT_SHOW_CONTENTLESS_CORES,                        PARSE_ONLY_UINT, true },
#endif
               {MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC,                                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO,                                    PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_ADD,                                      PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_ADD_ENTRY,                                PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS,                                PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_TIMEDATE_ENABLE,                                       PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_TIMEDATE_STYLE,                                        PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_TIMEDATE_DATE_SEPARATOR,                               PARSE_ONLY_UINT, true  },
               {MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE,                                  PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_CORE_ENABLE,                                           PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS,                                   PARSE_ONLY_BOOL, true  },
               {MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN,                                PARSE_ONLY_BOOL, true  },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               {MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER,                              PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_FILTER_BY_CURRENT_CORE,                                PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_USE_LAST_START_DIRECTORY,                              PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               {MENU_ENUM_LABEL_ACCOUNTS_FACEBOOK,                      PARSE_ACTION},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#if defined(HAVE_OVERLAY)
      case DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST:
         {
            bool input_overlay_enable       = settings->bools.input_overlay_enable;
            bool input_overlay_auto_scale   = settings->bools.input_overlay_auto_scale;
            enum overlay_show_input_type
                  input_overlay_show_inputs = (enum overlay_show_input_type)
                        settings->uints.input_overlay_show_inputs;

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE,                      PARSE_ONLY_BOOL,  true  },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_BEHIND_MENU,                 PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU,                PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED, PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS,                 PARSE_ONLY_UINT,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,            PARSE_ONLY_UINT,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,           PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE,                 PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_SCALE,                  PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_OVERLAY_PRESET,                            PARSE_ONLY_PATH,  false },
               {MENU_ENUM_LABEL_OVERLAY_OPACITY,                           PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE,                   PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,           PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE,            PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,            PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE,                PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE,                PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT,                    PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,            PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT,             PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_PORTRAIT,             PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT,                 PARSE_ONLY_FLOAT, false },
               {MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT,                 PARSE_ONLY_FLOAT, false },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_BEHIND_MENU:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE:
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_SCALE:
                  case MENU_ENUM_LABEL_OVERLAY_PRESET:
                  case MENU_ENUM_LABEL_OVERLAY_OPACITY:
                     if (input_overlay_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT:
                     if (input_overlay_enable &&
                         (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_PHYSICAL))
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE:
                  case MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT:
                  case MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT:
                  case MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT:
                  case MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_PORTRAIT:
                  case MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT:
                  case MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT:
                     if (input_overlay_enable &&
                         !input_overlay_auto_scale)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#endif
#ifdef HAVE_VIDEO_LAYOUT
      case DISPLAYLIST_ONSCREEN_VIDEO_LAYOUT_SETTINGS_LIST:
         {
            bool video_layout_enable  = settings->bools.video_layout_enable;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE,                   PARSE_ONLY_BOOL,  true },
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH,                     PARSE_ONLY_PATH,  false},
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW,            PARSE_ONLY_UINT,  false},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
                  case MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW:
                     if (video_layout_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
#endif
      case DISPLAYLIST_LATENCY_SETTINGS_LIST:
         {
            bool video_hard_sync          = settings->bools.video_hard_sync;
#ifdef HAVE_RUNAHEAD
            bool runahead_supported       = true;
            bool runahead_enabled         = settings->bools.run_ahead_enabled;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_VIDEO_FRAME_DELAY,                     PARSE_ONLY_UINT, true },
               {MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO,                PARSE_ONLY_BOOL, true },
               {MENU_ENUM_LABEL_AUDIO_LATENCY,                         PARSE_ONLY_UINT, true },
               {MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR,              PARSE_ONLY_UINT, true },
               {MENU_ENUM_LABEL_INPUT_BLOCK_TIMEOUT,                   PARSE_ONLY_UINT, true },
#ifdef HAVE_RUNAHEAD
               {MENU_ENUM_LABEL_RUN_AHEAD_ENABLED,                     PARSE_ONLY_BOOL, false },
               {MENU_ENUM_LABEL_RUN_AHEAD_FRAMES,                      PARSE_ONLY_UINT, false },
               {MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE,          PARSE_ONLY_BOOL, false },
               {MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS,               PARSE_ONLY_BOOL, false },
#endif
            };

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
                     PARSE_ONLY_UINT, false);
               count++;
            }

            if (video_driver_test_all_flags(GFX_CTX_FLAGS_HARD_SYNC))
            {
               MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_VIDEO_HARD_SYNC,
                     PARSE_ONLY_BOOL, false);
               count++;
               if (video_hard_sync)
               {
                  MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES,
                        PARSE_ONLY_UINT, false);
                  count++;
               }
            }

#ifdef HAVE_RUNAHEAD
            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL) &&
                !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               runahead_supported = core_info_current_supports_runahead();

            if (runahead_supported)
            {
               for (i = 0; i < ARRAY_SIZE(build_list); i++)
               {
                  switch (build_list[i].enum_idx)
                  {
                     case MENU_ENUM_LABEL_RUN_AHEAD_ENABLED:
                        build_list[i].checked = true;
                        break;
                     case MENU_ENUM_LABEL_RUN_AHEAD_FRAMES:
                     case MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE:
                     case MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS:
                        if (runahead_enabled)
                           build_list[i].checked = true;
                        break;
                     default:
                        break;
                  }
               }
            }
#endif
            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }

#ifdef HAVE_RUNAHEAD
            if (!runahead_supported &&
                menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED),
                     msg_hash_to_str(MENU_ENUM_LABEL_RUN_AHEAD_UNSUPPORTED),
                     MENU_ENUM_LABEL_RUN_AHEAD_UNSUPPORTED,
                     FILE_TYPE_NONE, 0, 0))
               count++;
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_GAMEMODE_ENABLE, PARSE_ONLY_BOOL, false) == 0)
               count++;
         }
         break;
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
         {
            bool video_font_enable        = settings->bools.video_font_enable;
            bool video_msg_bgcolor_enable = settings->bools.video_msg_bgcolor_enable;
#ifdef HAVE_GFX_WIDGETS
            bool widgets_supported        = video_driver_has_widgets();
            bool widgets_active           = gfx_widgets_ready();
            bool menu_widget_scale_auto   = settings->bools.menu_widget_scale_auto;
#else
            bool widgets_active           = false;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS, PARSE_ACTION,      true  },
               {MENU_ENUM_LABEL_VIDEO_FONT_ENABLE,                     PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE,                   PARSE_ONLY_BOOL,   false },
               {MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO,                PARSE_ONLY_BOOL,   false },
               {MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR,              PARSE_ONLY_FLOAT,  false },
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
               {MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,     PARSE_ONLY_FLOAT,  false },
#endif
               {MENU_ENUM_LABEL_VIDEO_FONT_PATH,                       PARSE_ONLY_PATH,   false },
               {MENU_ENUM_LABEL_VIDEO_FONT_SIZE,                       PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X,                   PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y,                   PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,               PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,             PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE,              PARSE_ONLY_FLOAT,  false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,          PARSE_ONLY_BOOL,   false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,             PARSE_ONLY_UINT,   false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,           PARSE_ONLY_UINT,   false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,            PARSE_ONLY_UINT,   false },
               {MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,         PARSE_ONLY_FLOAT,  false },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
#ifdef HAVE_GFX_WIDGETS
                  case MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE:
                     if (video_font_enable && widgets_supported)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO:
                     if (widgets_active)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR:
                     if (widgets_active && !menu_widget_scale_auto)
                        build_list[i].checked = true;
                     break;
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
                  case MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED:
                     if (widgets_active && !menu_widget_scale_auto)
                        build_list[i].checked = true;
                     break;
#endif
#endif
                  case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
                  case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
                     if (video_font_enable || widgets_active)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE:
                     if (!widgets_active && video_font_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE:
                  case MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY:
                     if (!widgets_active &&
                         video_font_enable &&
                         video_msg_bgcolor_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST:
         {
            bool video_font_enable            = settings->bools.video_font_enable;
            bool video_fps_show               = settings->bools.video_fps_show;
            bool video_memory_show            = settings->bools.video_memory_show;
#ifdef HAVE_GFX_WIDGETS
            bool widgets_active               = gfx_widgets_ready();
            bool notifications_active         = video_font_enable || widgets_active;
#ifdef HAVE_SCREENSHOTS
            bool notification_show_screenshot = settings->bools.notification_show_screenshot;
#endif
#else
            bool notifications_active         = video_font_enable;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_FPS_SHOW,                                PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL,                     PARSE_ONLY_UINT,  false },
               {MENU_ENUM_LABEL_FRAMECOUNT_SHOW,                         PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_STATISTICS_SHOW,                         PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_MEMORY_SHOW,                             PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL,                  PARSE_ONLY_UINT,  false },
#if defined(HAVE_NETWORKING) && defined(HAVE_GFX_WIDGETS)
               {MENU_ENUM_LABEL_NETPLAY_PING_SHOW,                       PARSE_ONLY_BOOL,  false },
#endif
               {MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,        PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG,            PARSE_ONLY_BOOL,  false },
#ifdef HAVE_CHEATS
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,        PARSE_ONLY_BOOL,  false },
#endif
#ifdef HAVE_PATCH
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_PATCH_APPLIED,         PARSE_ONLY_BOOL,  true },
#endif
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_REMAP_LOAD,            PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,  PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,      PARSE_ONLY_BOOL,  false },
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_FAST_FORWARD,          PARSE_ONLY_BOOL,  false },
#ifdef HAVE_SCREENSHOTS
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT,            PARSE_ONLY_BOOL,  false },
#ifdef HAVE_GFX_WIDGETS
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,   PARSE_ONLY_UINT,  false },
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,      PARSE_ONLY_UINT,  false },
#endif
#endif
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_REFRESH_RATE,          PARSE_ONLY_BOOL,  false },
#ifdef HAVE_NETWORKING
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,         PARSE_ONLY_BOOL,  false },
#endif
               {MENU_ENUM_LABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,    PARSE_ONLY_BOOL,  false },
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL:
                     if (notifications_active && video_fps_show)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL:
                     if (notifications_active && video_memory_show)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_STATISTICS_SHOW:
                     if (notifications_active && video_font_enable)
                        build_list[i].checked = true;
                     break;
#ifdef HAVE_GFX_WIDGETS
#ifdef HAVE_NETWORKING
                  case MENU_ENUM_LABEL_NETPLAY_PING_SHOW:
                     if (widgets_active)
                        build_list[i].checked = true;
                     break;
#endif
                  case MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION:
                     if (widgets_active)
                        build_list[i].checked = true;
                     break;
#ifdef HAVE_SCREENSHOTS
                  case MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION:
                     if (widgets_active && notification_show_screenshot)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH:
                     if (widgets_active && notification_show_screenshot)
                        build_list[i].checked = true;
                     break;
#endif
#endif
                  default:
                     if (notifications_active)
                        build_list[i].checked = true;
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SAVING_SETTINGS_LIST:
         {
            bool savestate_auto_index = settings->bools.savestate_auto_index;

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE,              PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE,             PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,   PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,  PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE,               PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_AUTOSAVE_INTERVAL,                  PARSE_ONLY_UINT, true},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX,               PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP,                 PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE,                PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD,                PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE,         PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVE_FILE_COMPRESSION,              PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVESTATE_FILE_COMPRESSION,         PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE, PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,    PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,   PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,  PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,  PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG,                PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE,      PARSE_ONLY_BOOL, true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_SAVESTATE_MAX_KEEP:
                     build_list[i].checked = savestate_auto_index;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (build_list[i].checked &&
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_SETTINGS_ALL:
         {
#ifdef HAVE_TRANSLATE
            bool settings_show_ai_service = settings->bools.settings_show_ai_service;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_DRIVER_SETTINGS,             PARSE_ACTION, true},
               {MENU_ENUM_LABEL_VIDEO_SETTINGS,              PARSE_ACTION, true},
               {MENU_ENUM_LABEL_AUDIO_SETTINGS,              PARSE_ACTION, true},
               {MENU_ENUM_LABEL_INPUT_SETTINGS,              PARSE_ACTION, true},
               {MENU_ENUM_LABEL_LATENCY_SETTINGS,            PARSE_ACTION, true},
               {MENU_ENUM_LABEL_CORE_SETTINGS,               PARSE_ACTION, true},
               {MENU_ENUM_LABEL_CONFIGURATION_SETTINGS,      PARSE_ACTION, true},
               {MENU_ENUM_LABEL_SAVING_SETTINGS,             PARSE_ACTION, true},
               {MENU_ENUM_LABEL_LOGGING_SETTINGS,            PARSE_ACTION, true},
               {MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,  PARSE_ACTION, true},
               {MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS,     PARSE_ACTION, true},
               {MENU_ENUM_LABEL_RECORDING_SETTINGS,          PARSE_ACTION, true},
               {MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,   PARSE_ACTION, true},
               {MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,     PARSE_ACTION, true},
               {MENU_ENUM_LABEL_AI_SERVICE_SETTINGS,         PARSE_ACTION, true},
               {MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS,      PARSE_ACTION, true},
               {MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,   PARSE_ACTION, true},
#ifdef HAVE_CHEEVOS
               {MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS, PARSE_ACTION, true},
#endif
               {MENU_ENUM_LABEL_BLUETOOTH_SETTINGS,          PARSE_ACTION, true},
#ifdef HAVE_NETWORKING
               {MENU_ENUM_LABEL_WIFI_SETTINGS,               PARSE_ACTION, true},
               {MENU_ENUM_LABEL_NETWORK_SETTINGS,            PARSE_ACTION, true},
               {MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS,   PARSE_ACTION, true},
#endif
               {MENU_ENUM_LABEL_LAKKA_SERVICES,              PARSE_ACTION, true},
               {MENU_ENUM_LABEL_PLAYLIST_SETTINGS,           PARSE_ACTION, true},
               {MENU_ENUM_LABEL_USER_SETTINGS,               PARSE_ACTION, true},
               {MENU_ENUM_LABEL_DIRECTORY_SETTINGS,          PARSE_ACTION, true},
            };


            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_AI_SERVICE_SETTINGS:
#ifdef HAVE_TRANSLATE
                     build_list[i].checked = settings_show_ai_service;
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
                  case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_file_browser;
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
                  case MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS:
                     build_list[i].checked = settings->bools.settings_show_accessibility;
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
                      * MENU_ENUM_LABEL_BLUETOOTH_SETTINGS
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
                     MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               {MENU_ENUM_LABEL_CPU_PERFPOWER,              PARSE_ACTION},
               {MENU_ENUM_LABEL_GAMEMODE_ENABLE,            PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
#ifdef _3DS
            u8 device_model = 0xFF;
            CFGU_GetSystemModel(&device_model);
            if ((device_model == 2) || (device_model == 4) || (device_model == 5))
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        MENU_ENUM_LABEL_NEW3DS_SPEEDUP_ENABLE,
                        PARSE_ONLY_BOOL, false) == 0)
                  count++;
            }
#endif
         }
         break;
      case DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
#if defined(HAVE_OVERLAY)
               {MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS, PARSE_ACTION},
#endif
#ifdef HAVE_VIDEO_LAYOUT
               {MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS, PARSE_ACTION},
#endif
               {MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,  PARSE_ACTION},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               {MENU_ENUM_LABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,  PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP,              PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE, PARSE_ONLY_UINT},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               {MENU_ENUM_LABEL_SETTINGS_SHOW_FILE_BROWSER,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE,       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_ACCESSIBILITY,    PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK,          PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_USER,             PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY,        PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT,         PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,          PARSE_ONLY_BOOL},
#ifdef HAVE_SCREENSHOTS
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,        PARSE_ONLY_BOOL},
#endif
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,   PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION, PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS,                PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,     PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS,               PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS,                 PARSE_ONLY_BOOL},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         if (video_shader_any_supported())
         {
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                     MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS,
                     PARSE_ONLY_BOOL, false) == 0)
               count++;
         }
#endif

         {
            menu_displaylist_build_info_t build_list[] = {
#ifdef HAVE_REWIND
               {MENU_ENUM_LABEL_CONTENT_SHOW_REWIND,                    PARSE_ONLY_BOOL},
#endif
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_CORE_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_CORE_INFO_CACHE_ENABLE,            PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE,        PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_CORE_OPTION_CATEGORY_ENABLE,       PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_DRIVER_SWITCH_ENABLE,              PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE,                PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT,              PARSE_ONLY_BOOL},
               {MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN,            PARSE_ONLY_BOOL},
#ifndef HAVE_DYNAMIC
               {MENU_ENUM_LABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT, PARSE_ONLY_BOOL},
#endif
            };

            if (menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_CORE_MANAGER_LIST),
                     MENU_ENUM_LABEL_CORE_MANAGER_LIST,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_DIRECTORY_SETTINGS_LIST:
         {
            menu_displaylist_build_info_t build_list[] = {
               {MENU_ENUM_LABEL_SYSTEM_DIRECTORY,                PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_ASSETS_DIRECTORY,                PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY,    PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY,            PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY,          PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LIBRETRO_DIR_PATH,               PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LIBRETRO_INFO_PATH,              PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY,      PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CURSOR_DIRECTORY,                PARSE_ONLY_DIR},
#ifdef HAVE_CHEATS
               {MENU_ENUM_LABEL_CHEAT_DATABASE_PATH,             PARSE_ONLY_DIR},
#endif
#ifdef HAVE_VIDEO_FILTER
               {MENU_ENUM_LABEL_VIDEO_FILTER_DIR,                PARSE_ONLY_DIR},
#endif
#ifdef HAVE_DSP_FILTER
               {MENU_ENUM_LABEL_AUDIO_FILTER_DIR,                PARSE_ONLY_DIR},
#endif
               {MENU_ENUM_LABEL_VIDEO_SHADER_DIR,                PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY,      PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY,      PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_OVERLAY_DIRECTORY,               PARSE_ONLY_DIR},
#ifdef HAVE_VIDEO_LAYOUT
               {MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY,          PARSE_ONLY_DIR},
#endif
#ifdef HAVE_SCREENSHOTS
               {MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY,            PARSE_ONLY_DIR},
#endif
               {MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY,       PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_PLAYLIST_DIRECTORY,              PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_FAVORITES_DIRECTORY,     PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_HISTORY_DIRECTORY,       PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_IMAGE_HISTORY_DIRECTORY, PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_MUSIC_HISTORY_DIRECTORY, PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CONTENT_VIDEO_HISTORY_DIRECTORY, PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY,           PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_SAVEFILE_DIRECTORY,              PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_SAVESTATE_DIRECTORY,             PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_CACHE_DIRECTORY,                 PARSE_ONLY_DIR},
               {MENU_ENUM_LABEL_LOG_DIR,                         PARSE_ONLY_DIR},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
#ifdef HAVE_BLUETOOTH
               {MENU_ENUM_LABEL_BLUETOOTH_DRIVER,      PARSE_ONLY_STRING_OPTIONS},
#endif
#if defined(HAVE_LAKKA) || defined(HAVE_WIFI)
               {MENU_ENUM_LABEL_WIFI_DRIVER,           PARSE_ONLY_STRING_OPTIONS},
#endif
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_LOGGING_SETTINGS_LIST:
         {
            bool log_to_file              = settings->bools.log_to_file;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_LOG_VERBOSITY,         PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL,    PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL,    PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_LOG_TO_FILE,           PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP, PARSE_ONLY_BOOL, false},
               {MENU_ENUM_LABEL_PERFCNT_ENABLE,        PARSE_ONLY_BOOL, true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL:
                  case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
                     if (verbosity_is_enabled())
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP:
                     if (log_to_file)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
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
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_REWIND_SETTINGS_LIST:
         {
            bool rewind_enable            = settings->bools.rewind_enable;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_REWIND_ENABLE,           PARSE_ONLY_BOOL, true},
               {MENU_ENUM_LABEL_REWIND_GRANULARITY,      PARSE_ONLY_UINT, false},
               {MENU_ENUM_LABEL_REWIND_BUFFER_SIZE,      PARSE_ONLY_SIZE, false},
               {MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP, PARSE_ONLY_UINT, false},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_REWIND_GRANULARITY:
                  case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
                  case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
                     if (rewind_enable)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST:
         {
#ifdef HAVE_REWIND
            bool rewind_supported = true;
#endif
            menu_displaylist_build_info_selective_t build_list[] = {
#ifdef HAVE_REWIND
               {MENU_ENUM_LABEL_REWIND_SETTINGS,             PARSE_ACTION,     false},
#endif
               {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS, PARSE_ACTION,     true},
               {MENU_ENUM_LABEL_FASTFORWARD_RATIO,           PARSE_ONLY_FLOAT, true},
               {MENU_ENUM_LABEL_FASTFORWARD_FRAMESKIP,       PARSE_ONLY_BOOL,  true},
               {MENU_ENUM_LABEL_SLOWMOTION_RATIO,            PARSE_ONLY_FLOAT, true},
               {MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE,          PARSE_ONLY_BOOL,  true},
               {MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE,     PARSE_ONLY_BOOL , true},
            };

#ifdef HAVE_REWIND
            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL) &&
                !retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
               rewind_supported = core_info_current_supports_rewind();

            if (rewind_supported)
               for (i = 0; i < ARRAY_SIZE(build_list); i++)
               {
                  switch (build_list[i].enum_idx)
                  {
                     case MENU_ENUM_LABEL_REWIND_SETTINGS:
                        build_list[i].checked = true;
                        break;
                     default:
                        break;
                  }
               }
#endif
            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_MENU_SETTINGS_LIST:
         {
            bool menu_horizontal_animation             = settings->bools.menu_horizontal_animation;
            bool menu_materialui_icons_enable          = settings->bools.menu_materialui_icons_enable;
            bool menu_materialui_show_nav_bar          = settings->bools.menu_materialui_show_nav_bar;
            bool menu_use_preferred_system_color_theme = settings->bools.menu_use_preferred_system_color_theme;
            unsigned menu_rgui_color_theme             = settings->uints.menu_rgui_color_theme;
            unsigned menu_rgui_particle_effect         = settings->uints.menu_rgui_particle_effect;
            unsigned menu_screensaver_timeout          = settings->uints.menu_screensaver_timeout;

            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_MENU_SCALE_FACTOR,                            PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_WALLPAPER,                               PARSE_ONLY_PATH ,  true},
               {MENU_ENUM_LABEL_DYNAMIC_WALLPAPER,                            PARSE_ONLY_BOOL ,  true},
               {MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY,                       PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY,                     PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE, PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE,               PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,     PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,                  PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_LINEAR_FILTER,                           PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,             PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO,                       PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK,                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION,                    PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,      PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,              PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,         PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME,                        PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET,                       PARSE_ONLY_PATH,   false},
               {MENU_ENUM_LABEL_MENU_RGUI_TRANSPARENCY,                       PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_MENU_RGUI_SHADOWS,                            PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT,                    PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,              PARSE_ONLY_FLOAT,  false},
               {MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,        PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_MENU_XMB_VERTICAL_FADE_FACTOR,                PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_XMB_TITLE_MARGIN,                        PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_ALPHA_FACTOR,                             PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_FONT,                                     PARSE_ONLY_PATH,   true},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,                          PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,                        PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE,                         PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_LAYOUT,                                   PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_THEME,                                    PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE,                           PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_XMB_RIBBON_ENABLE,                            PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME,                         PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_OZONE_COLLAPSE_SIDEBAR,                       PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME,                 PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE,                      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,             PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,     PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MATERIALUI_SHOW_NAV_BAR,                      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,               PARSE_ONLY_BOOL,   false},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME,                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,         PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY,               PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY,               PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,        PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME,                       PARSE_ONLY_UINT,   false},
               {MENU_ENUM_LABEL_OZONE_THUMBNAIL_SCALE_FACTOR,                 PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_RGUI_INLINE_THUMBNAILS,                  PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,      PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,     PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,   PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,       PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_THUMBNAILS,                                   PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_LEFT_THUMBNAILS,                              PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS,                      PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,              PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,             PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_SWAP_THUMBNAILS,                    PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,               PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DELAY,                    PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_TICKER_TYPE,                             PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_MENU_TICKER_SPEED,                            PARSE_ONLY_FLOAT,  true},
               {MENU_ENUM_LABEL_MENU_TICKER_SMOOTH,                           PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_OZONE_SCROLL_CONTENT_METADATA,                PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_EXTENDED_ASCII,                     PARSE_ONLY_BOOL,   true},
               {MENU_ENUM_LABEL_MENU_RGUI_SWITCH_ICONS,                       PARSE_ONLY_BOOL,   true},
            };

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT:
                     if (menu_horizontal_animation)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET:
                     if (menu_rgui_color_theme == RGUI_THEME_CUSTOM)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_RGUI_TRANSPARENCY:
                     if ((menu_rgui_color_theme != RGUI_THEME_CUSTOM) &&
                         (menu_rgui_color_theme != RGUI_THEME_DYNAMIC))
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED:
                     if (menu_rgui_particle_effect != RGUI_PARTICLE_EFFECT_NONE)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER:
                     if ((menu_screensaver_timeout != 0) &&
                         (menu_rgui_particle_effect != RGUI_PARTICLE_EFFECT_NONE))
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE:
                     if (menu_materialui_icons_enable)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR:
                     if (menu_materialui_show_nav_bar)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME:
                     if (!menu_use_preferred_system_color_theme)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
         break;
      case DISPLAYLIST_BROWSE_URL_LIST:
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_URL),
                  msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_URL),
                  MENU_ENUM_LABEL_BROWSE_URL,
                  0, 0, 0))
            count++;
         if (menu_entries_append_enum(list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BROWSE_START),
                  msg_hash_to_str(MENU_ENUM_LABEL_BROWSE_START),
                  MENU_ENUM_LABEL_BROWSE_START,
                  0, 0, 0))
            count++;
         break;
      case DISPLAYLIST_DISC_INFO:
#ifdef HAVE_CDROM
         count = menu_displaylist_parse_disc_info(list,
               MENU_SET_CDROM_INFO);
#endif
         break;
      case DISPLAYLIST_DUMP_DISC:
#ifdef HAVE_CDROM
         count = menu_displaylist_parse_disc_info(list,
               MENU_SET_CDROM_LIST);
#endif
         break;
#ifdef HAVE_LAKKA
      case DISPLAYLIST_EJECT_DISC:
#ifdef HAVE_CDROM
         count = menu_displaylist_parse_disc_info(list,
               MENU_SET_EJECT_DISC);
#endif /* HAVE_CDROM */
         break;
#endif /* HAVE_LAKKA */
      default:
         break;
   }

   return count;
}

/* Returns true if selection pointer should be reset
 * to zero when viewing specified history playlist */
static bool history_needs_navigation_clear(
      menu_handle_t *menu, playlist_t *playlist)
{
   if (!menu)
      return false;

   /* If content is running, compare last selected path
    * with current content path */
   if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return string_is_equal(menu->deferred_path, path_get(RARCH_PATH_CONTENT));

   /* If content is not running, have to examine the
    * playlist... */
   if (playlist)
   {
      if (menu->rpl_entry_selection_ptr < playlist_size(playlist))
      {
         const struct playlist_entry *entry = NULL;

         playlist_get_index(playlist, menu->rpl_entry_selection_ptr, &entry);
         return !string_is_equal(menu->deferred_path, entry->path);
      }
   }

   return false;
}

static unsigned menu_displaylist_build_shader_parameter(
      menu_displaylist_info_t *info,
      file_list_t *list,
      unsigned entry_type, unsigned _offset,
      unsigned setting_type)
{
   video_shader_ctx_t shader_info;
   float    current_value               = 0.0f;
   float    original_value              = 0.0f;
   unsigned count                       = 0;
   float    min                         = 0.0f;
   float    max                         = 0.0f;
   float    half_step                   = 0.0f;
   unsigned i                           = 0;
   unsigned checked                     = 0;
   bool     checked_found               = false;
   struct video_shader_parameter *param = NULL;
   unsigned offset                      = entry_type - _offset;

   video_shader_driver_get_current_shader(&shader_info);

   param = &shader_info.data->parameters[offset];
   if (!param)
      return 0;

   min                         = param->minimum;
   max                         = param->maximum;
   half_step                   = param->step * 0.5f;
   current_value               = min;
   original_value              = param->current;

   for (i = 0; current_value < (max + 0.0001f); i++)
   {
      char val_s[16], val_d[16];
      snprintf(val_s, sizeof(val_s), "%.2f", current_value);
      snprintf(val_d, sizeof(val_d), "%d", i);

      if (menu_entries_append_enum(list,
               val_s,
               val_d,
               MENU_ENUM_LABEL_NO_ITEMS,
               setting_type,
               i, entry_type))
      {
         if (!checked_found &&
             (fabs(current_value - original_value) < half_step))
         {
            checked       = count;
            checked_found = true;
         }

         count++;
      }

      current_value += param->step;
   }

   if (checked_found)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
      if (cbs)
         cbs->checked = true;
      menu_navigation_set_selection(checked);
   }

   return count;
}

#ifdef HAVE_NETWORKING
unsigned menu_displaylist_netplay_refresh_rooms(file_list_t *list)
{
   int i;
   unsigned count             = 0;
   settings_t *settings       = config_get_ptr();
   net_driver_state_t *net_st = networking_state_get_ptr();

   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);

   if (menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS),
         msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS),
         MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS,
         MENU_SETTING_ACTION, 0, 0))
      count++;

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
      !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) &&
      netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
   {
      if (menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_DISCONNECT),
            MENU_ENUM_LABEL_NETPLAY_DISCONNECT,
            MENU_SETTING_ACTION, 0, 0))
         count++;
   }

   if (menu_entries_append_enum(list,
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT),
      msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT),
      MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT,
      MENU_SETTING_ACTION, 0, 0))
      count++;

   if (menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS),
         msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS),
         MENU_ENUM_LABEL_NETWORK_SETTINGS,
         MENU_SETTING_ACTION, 0, 0))
      count++;

   if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
         MENU_ENUM_LABEL_NETPLAY_SHOW_ONLY_CONNECTABLE,
         PARSE_ONLY_BOOL, false) == 0)
      count++;

   if (menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS),
         msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS),
         MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS,
         MENU_SETTING_ACTION, 0, 0))
      count++;

#ifdef HAVE_NETPLAYDISCOVERY
   if (menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN),
         msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN),
         MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN,
         MENU_SETTING_ACTION, 0, 0))
      count++;
#endif

   for (i = 0; i < net_st->room_count; i++)
   {
      char buf[8192];
      char passworded[64];
      char country[8];
      const char *room_type;
      struct netplay_room *room = &net_st->room_list[i];

      /* Get rid of any room that is not running RetroArch. */
      if (!room->is_retroarch)
         continue;

      /* Get rid of any room that is not connectable,
         if the user opt-in. */
      if (!room->connectable && settings->bools.netplay_show_only_connectable)
         continue;

      if (room->has_password || room->has_spectate_password)
         snprintf(passworded, sizeof(passworded), "[%s] ",
            msg_hash_to_str(MSG_ROOM_PASSWORDED));
      else
         *passworded = '\0';

      if (!room->lan && !string_is_empty(room->country))
         snprintf(country, sizeof(country), " (%s)",
            room->country);
      else
         *country = '\0';

      if (room->lan)
         room_type = msg_hash_to_str(MSG_LOCAL);
      else if (room->host_method == NETPLAY_HOST_METHOD_MITM)
         room_type = msg_hash_to_str(MSG_INTERNET_RELAY);
      else if (room->connectable)
         room_type = msg_hash_to_str(MSG_INTERNET);
      else
         room_type = msg_hash_to_str(MSG_INTERNET_NOT_CONNECTABLE);

      snprintf(buf, sizeof(buf), "%s%s: %s%s",
         passworded, room_type,
         room->nickname, country);

      if (menu_entries_append_enum(list,
            buf,
            msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM),
            MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM,
            (unsigned)(MENU_SETTINGS_NETPLAY_ROOMS_START + i), 0, 0))
         count++;

      /* Uncomment this to debug mismatched room parameters*/
#if 0
      RARCH_LOG("[Lobby]: Room Data: %d\n"
         "Nickname:         %s\n"
         "Address:          %s\n"
         "Port:             %d\n"
         "Core:             %s\n"
         "Core Version:     %s\n"
         "Game:             %s\n"
         "Game CRC:         %08x\n"
         "Timestamp:        %d\n", room_data->elems[j + 6].data,
         room->nickname,
         room->address,
         room->port,
         room->corename,
         room->coreversion,
         room->gamename,
         room->gamecrc,
         room->timestamp);
#endif
   }

   netplay_rooms_free();

   return count;
}

static unsigned print_buf_lines(file_list_t *list, char *buf,
      const char *label, int buf_size,
      enum msg_file_type type, bool append, bool extended)
{
   char c;
   unsigned count   = 0;
   int i            = 0;
   char *line_start = buf;

   if (!buf || !buf_size)
      return 0;

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;
      const char *core_date        = NULL;
      const char *core_crc         = NULL;
      const char *core_pathname    = NULL;
      struct string_list str_list  = {0};

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

      string_list_initialize(&str_list);
      string_split_noalloc(&str_list, line_start, " ");

      if (str_list.elems[0].data)
         core_date     = str_list.elems[0].data;
      if (str_list.elems[1].data)
         core_crc      = str_list.elems[1].data;
      if (str_list.elems[2].data)
         core_pathname = str_list.elems[2].data;

      (void)core_date;
      (void)core_crc;

      if (extended)
      {
         if (append)
         {
            if (menu_entries_append_enum(list, core_pathname, "",
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0))
               count++;
         }
         else
         {
            menu_entries_prepend(list, core_pathname, "",
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
            count++;
         }
      }
      else
      {
         if (append)
         {
            if (menu_entries_append_enum(list, line_start, label,
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0))
               count++;
         }
         else
         {
            menu_entries_prepend(list, line_start, label,
                  MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
            count++;
         }
      }

      string_list_deinitialize(&str_list);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start     = buf + i + 1;
   }

   if (append && type != FILE_TYPE_DOWNLOAD_LAKKA)
      file_list_sort_on_alt(list);
   /* If the buffer was completely full, and didn't end
    * with a newline, just ignore the partial last line. */

   return count;
}
#endif

bool menu_displaylist_has_subsystems(void)
{
   runloop_state_t *runloop_st                  = runloop_state_get_ptr();
   const struct retro_subsystem_info* subsystem = runloop_st->subsystem_data;
   rarch_system_info_t *sys_info                = &runloop_st->system;
   /* Core not loaded completely, use the data we
    * peeked on load core */
   /* Core fully loaded, use the subsystem data */
   if (sys_info && sys_info->subsystem.data)
      subsystem = sys_info->subsystem.data;
   return (subsystem && runloop_st->subsystem_current_count > 0);
}

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type,
      menu_displaylist_info_t *info,
      settings_t *settings)
{
   menu_ctx_displaylist_t disp_list;
   bool load_content             = true;
   bool use_filebrowser          = false;
   static bool core_selected     = false;
   unsigned count                = 0;
   int ret                       = 0;
   menu_handle_t *menu           = menu_state_get_ptr()->driver_data;

   disp_list.info                = info;
   disp_list.type                = type;

   if (menu_driver_push_list(&disp_list))
      return true;

   switch (type)
   {
      case DISPLAYLIST_NETWORK_HOSTING_SETTINGS_LIST:
#ifdef HAVE_NETWORKING
         {
            size_t i;
            bool include_everything        = false;
            file_list_t *list              = info->list;
            menu_displaylist_build_info_selective_t build_list[] = {
               {MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT,                                  PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS,                               PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_MAX_PING,                                      PARSE_ONLY_UINT,   true},
               {MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE,                               PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER,                               PARSE_ONLY_BOOL,   true  },
               {MENU_ENUM_LABEL_NETPLAY_MITM_SERVER,                                   PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER,                            PARSE_ONLY_STRING, false},
               {MENU_ENUM_LABEL_NETPLAY_PASSWORD,                                      PARSE_ONLY_STRING, true},
               {MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD,                             PARSE_ONLY_STRING, true},
            };

		    menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);

            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL))
            {
               menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST),
                     msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_DISCONNECT),
                     MENU_ENUM_LABEL_NETPLAY_DISCONNECT,
                     MENU_SETTING_ACTION, 0, 0);
            }
            else if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
                  !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) &&
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            {
            }
            else
            {
               menu_entries_append_enum(list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST),
                     msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST),
                     MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST,
                     MENU_SETTING_ACTION, 0, 0);
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               switch (build_list[i].enum_idx)
               {
                  case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
                     if (settings->bools.netplay_use_mitm_server)
                        build_list[i].checked = true;
                     break;
                  case MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER:
                     if (settings->bools.netplay_use_mitm_server)
                        build_list[i].checked = true;
                     break;
                  default:
                     break;
               }
            }

            for (i = 0; i < ARRAY_SIZE(build_list); i++)
            {
               if (!build_list[i].checked && !include_everything)
                  continue;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,
                        build_list[i].enum_idx,  build_list[i].parse_type,
                        false) == 0)
                  count++;
            }
         }
#endif
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_OPTIONS_REMAPPINGS_PORT:
         {
            unsigned max_users          = settings->uints.input_max_users;
            const char *menu_driver     = menu_driver_ident();
            bool is_rgui                = string_is_equal(menu_driver, "rgui");
            file_list_t *list           = info->list;
            unsigned port               = string_to_unsigned(info->path);
            unsigned mapped_port        = settings->uints.input_remap_ports[port];
            size_t selection            = menu_navigation_get_selection();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            {
               char key_type[64];
               char key_analog[64];
               char key_port[64];

               key_type[0]   = '\0';
               key_analog[0] = '\0';
               key_port[0]   = '\0';

               snprintf(key_type, sizeof(key_type),
                     msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE), mapped_port + 1);
               snprintf(key_analog, sizeof(key_analog),
                     msg_hash_to_str(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE), port + 1);
               snprintf(key_port, sizeof(key_port),
                     msg_hash_to_str(MENU_ENUM_LABEL_INPUT_REMAP_PORT), port + 1);

               if (MENU_DISPLAYLIST_PARSE_SETTINGS(list,
                        key_type, PARSE_ONLY_UINT, true, MENU_SETTINGS_INPUT_LIBRETRO_DEVICE) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS(list,
                        key_analog, PARSE_ONLY_UINT, true, MENU_SETTINGS_INPUT_ANALOG_DPAD_MODE) == 0)
                  count++;
               if (MENU_DISPLAYLIST_PARSE_SETTINGS(list,
                        key_port, PARSE_ONLY_UINT, true, MENU_SETTINGS_INPUT_INPUT_REMAP_PORT) == 0)
                  count++;
            }

            {
               unsigned retro_id, j;
               unsigned device  = settings->uints.input_libretro_device[mapped_port];
               device          &= RETRO_DEVICE_MASK;

               if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
               {
                  for (j = 0; j < RARCH_ANALOG_BIND_LIST_END; j++)
                  {
                     char desc_label[400];
                     char descriptor[300];
                     const struct retro_keybind *keybind;
                     const struct retro_keybind *auto_bind;

                     retro_id                              =
                        (j < RARCH_ANALOG_BIND_LIST_END)
                        ? input_config_bind_order[j]
                        : j;
                     keybind                               =
                        &input_config_binds[port][retro_id];
                     auto_bind                             =
                        (const struct retro_keybind*)
                        input_config_get_bind_auto(port, retro_id);

                     input_config_get_bind_string(settings, descriptor,
                           keybind, auto_bind, sizeof(descriptor));

                     if (!strstr(descriptor, "Auto"))
                     {
                        const struct retro_keybind *keyptr =
                           &input_config_binds[port][retro_id];

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
                               "%s [%s %u]", descriptor,
                               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT), port + 1);
                        strlcpy(descriptor, desc_label, sizeof(descriptor));
                     }

                     /* Note: 'physical' port is passed as label */
                     if (menu_entries_append_enum(list, descriptor, info->path,
                              MSG_UNKNOWN,
                              MENU_SETTINGS_INPUT_DESC_BEGIN +
                              (port * (RARCH_FIRST_CUSTOM_BIND + 8)) + retro_id, 0, 0))
                        count++;
                  }
               }
               else if (device == RETRO_DEVICE_KEYBOARD)
               {
                  for (j = 0; j < RARCH_ANALOG_BIND_LIST_END; j++)
                  {
                     char desc_label[400];
                     char descriptor[300];
                     const struct retro_keybind *keybind;
                     const struct retro_keybind *auto_bind;

                     retro_id                              =
                        (j < RARCH_ANALOG_BIND_LIST_END)
                        ? input_config_bind_order[j]
                        : j;
                     keybind                               =
                        &input_config_binds[port][retro_id];
                     auto_bind                             =
                        (const struct retro_keybind*)
                        input_config_get_bind_auto(port, retro_id);

                     input_config_get_bind_string(settings, descriptor,
                           keybind, auto_bind, sizeof(descriptor));

                     if (!strstr(descriptor, "Auto"))
                     {
                        const struct retro_keybind *keyptr =
                           &input_config_binds[port][retro_id];

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
                        snprintf(desc_label, sizeof(desc_label), "%s [%s %u]",
                              descriptor,
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
                              port + 1);
                        strlcpy(descriptor, desc_label, sizeof(descriptor));
                     }

                     /* Note: 'physical' port is passed as label */
                     if (menu_entries_append_enum(list, descriptor, info->path,
                              MSG_UNKNOWN,
                              MENU_SETTINGS_INPUT_DESC_KBD_BEGIN +
                              (port * RARCH_ANALOG_BIND_LIST_END) + retro_id, 0, 0))
                        count++;
                  }
               }
            }

            info->need_push     = true;
            info->need_refresh  = true;
            if (selection >= count)
               info->need_clear = true;
         }
         break;
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
                     char title[sizeof("Title: ")+sizeof(cd_info.title)];
                     snprintf(title, sizeof(title), "Title: %s", cd_info.title);

                     if (menu_entries_append_enum(info->list,
                           title,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  if (!string_is_empty(cd_info.system))
                  {
                     char system[256];
                     snprintf(system, sizeof(system), "System: %s", cd_info.system);

                     if (menu_entries_append_enum(info->list,
                           system,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  if (!string_is_empty(cd_info.serial))
                  {
                     char serial[256];
                     snprintf(serial, sizeof(serial),
                           "%s#: %s",
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL),
                           cd_info.serial
                           );

                     if (menu_entries_append_enum(info->list,
                           serial,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  if (!string_is_empty(cd_info.version))
                  {
                     char version[256];
                     snprintf(version, sizeof(version),
                           "%s: %s", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION), cd_info.version);

                     if (menu_entries_append_enum(info->list,
                           version,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  if (!string_is_empty(cd_info.release_date))
                  {
                     char release_date[256];
                     snprintf(release_date, sizeof(release_date),
                           "Release Date: %s", cd_info.release_date);

                     if (menu_entries_append_enum(info->list,
                           release_date,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  if (atip)
                  {
                     const char *atip_string = "Genuine Disc: No";
                     if (menu_entries_append_enum(info->list,
                              atip_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0))
                        count++;
                  }
                  else
                  {
                     const char *atip_string = "Genuine Disc: Yes";
                     if (menu_entries_append_enum(info->list,
                              atip_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0))
                        count++;
                  }

                  {
                     char tracks_string[32] = {"Number of tracks: "};

                     snprintf(tracks_string + strlen(tracks_string), sizeof(tracks_string) - strlen(tracks_string), "%d", toc->num_tracks);

                     if (menu_entries_append_enum(info->list,
                           tracks_string,
                           "",
                           MSG_UNKNOWN,
                           FILE_TYPE_NONE, 0, 0))
                        count++;
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

                        if (menu_entries_append_enum(info->list,
                              track_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0))
                           count++;

                        if (toc->track[i].audio)
                           snprintf(mode_string + strlen(mode_string), sizeof(mode_string) - strlen(mode_string), "Audio");
                        else
                           snprintf(mode_string + strlen(mode_string), sizeof(mode_string) - strlen(mode_string), "Mode %d", toc->track[i].mode);

                        if (menu_entries_append_enum(info->list,
                              mode_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0))
                           count++;

                        snprintf(size_string + strlen(size_string), sizeof(size_string) - strlen(size_string), "%.1f MB", toc->track[i].track_bytes / 1000.0 / 1000.0);

                        if (menu_entries_append_enum(info->list,
                              size_string,
                              "",
                              MSG_UNKNOWN,
                              FILE_TYPE_NONE, 0, 0))
                           count++;

                        {
                           unsigned char min = 0;
                           unsigned char sec = 0;
                           unsigned char frame = 0;

                           cdrom_lba_to_msf(toc->track[i].track_size, &min, &sec, &frame);

                           snprintf(length_string + strlen(length_string), sizeof(length_string) - strlen(length_string), "%02d:%02d.%02d", min, sec, frame);

                           if (menu_entries_append_enum(info->list,
                                 length_string,
                                 "",
                                 MSG_UNKNOWN,
                                 FILE_TYPE_NONE, 0, 0))
                              count++;
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
      case DISPLAYLIST_LOAD_DISC:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_disc_info(info->list,
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
      case DISPLAYLIST_LOAD_DISC:
         /* No-op */
         break;
#endif
#ifndef HAVE_LAKKA_SWITCH
#ifdef HAVE_LAKKA
      case DISPLAYLIST_CPU_POLICY_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         menu_entries_append_enum(info->list,
            info->path,
            info->path,
            MENU_ENUM_LABEL_CPU_POLICY_MIN_FREQ,
            MENU_SETTINGS_CPU_POLICY_SET_MINFREQ, 0, 0);

         menu_entries_append_enum(info->list,
            info->path,
            info->path,
            MENU_ENUM_LABEL_CPU_POLICY_MAX_FREQ,
            MENU_SETTINGS_CPU_POLICY_SET_MAXFREQ, 0, 0);

         menu_entries_append_enum(info->list,
            info->path,
            info->path,
            MENU_ENUM_LABEL_CPU_POLICY_GOVERNOR,
            MENU_SETTINGS_CPU_POLICY_SET_GOVERNOR, 0, 0);

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;

         break;
      case DISPLAYLIST_CPU_PERFPOWER_LIST:
      {
         cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(true);
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         if (drivers)
         {
            int count = 0;

            menu_entries_append_enum(info->list,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE),
               msg_hash_to_str(MENU_ENUM_LABEL_CPU_PERF_MODE),
               MENU_ENUM_LABEL_CPU_PERF_MODE,
               0, 0, 0);

            switch (get_cpu_scaling_mode(NULL))
            {
            case CPUSCALING_MANUAL:
               while (*drivers)
               {
                  char policyid[16];
                  snprintf(policyid, sizeof(policyid), "%u", count++);
                  menu_entries_append_enum(info->list,
                     policyid,
                     policyid,
                     MENU_ENUM_LABEL_CPU_POLICY_ENTRY,
                     0, 0, 0);
                  drivers++;
               }
               break;
            case CPUSCALING_MANAGED_PER_CONTEXT:
               /* Allows user to pick two governors */
               menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR),
                  "0",
                  MENU_ENUM_LABEL_CPU_POLICY_CORE_GOVERNOR,
                  0, 0, 0);

               menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR),
                  "1",
                  MENU_ENUM_LABEL_CPU_POLICY_MENU_GOVERNOR,
                  0, 0, 0);

               /* fallthrough */
            case CPUSCALING_MANAGED_PERFORMANCE:
               /* Allow users to choose max/min frequencies */
               menu_entries_append_enum(info->list,
                  "0",
                  "0",
                  MENU_ENUM_LABEL_CPU_MANAGED_MIN_FREQ,
                  MENU_SETTINGS_CPU_MANAGED_SET_MINFREQ,
                  0, 0);

               menu_entries_append_enum(info->list,
                  "1",
                  "1",
                  MENU_ENUM_LABEL_CPU_MANAGED_MAX_FREQ,
                  MENU_SETTINGS_CPU_MANAGED_SET_MAXFREQ,
                  0, 0);

               break;
            case CPUSCALING_MAX_PERFORMANCE:
            case CPUSCALING_MIN_POWER:
            case CPUSCALING_BALANCED:
               /* No settings for these modes */
               break;
            };
         }

         info->need_push    = true;
         info->need_refresh = true;
         info->need_clear   = true;
         break;
      }
#endif
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case DISPLAYLIST_SWITCH_CPU_PROFILE:
      {
         unsigned i;
         char text[PATH_MAX_LENGTH];
#ifdef HAVE_LAKKA_SWITCH
         char current_profile[PATH_MAX_LENGTH];
         FILE               *profile = NULL;
#endif
         const size_t profiles_count = sizeof(SWITCH_CPU_PROFILES)/sizeof(SWITCH_CPU_PROFILES[1]);

         runloop_msg_queue_push("Warning : extended overclocking can damage the Switch", 1, 90, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

#ifdef HAVE_LAKKA_SWITCH
         profile = popen("cpu-profile get", "r");
         fgets(current_profile, PATH_MAX_LENGTH, profile);
         pclose(profile);

         snprintf(text, sizeof(text),
               "Current profile: %s", current_profile);
#else
         u32 currentClock = 0;
         if (hosversionBefore(8, 0, 0))
            pcvGetClockRate(PcvModule_CpuBus, &currentClock);
         else
         {
            ClkrstSession session = {0};
            clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
            clkrstGetClockRate(&session, &currentClock);
            clkrstCloseSession(&session);
         }
         snprintf(text, sizeof(text), "Current Clock : %i", currentClock);
#endif
         if (menu_entries_append_enum(info->list,
            text,
            "",
            0,
            MENU_INFO_MESSAGE, 0, 0))
            count++;

         for (i = 0; i < profiles_count; i++)
         {
            char title[PATH_MAX_LENGTH];
            char* profile               = SWITCH_CPU_PROFILES[i];
            char* speed                 = SWITCH_CPU_SPEEDS[i];

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

         snprintf(text, sizeof(text),
               "Current profile : %s", current_profile);

         if (menu_entries_append_enum(info->list, text, "", 0, MENU_INFO_MESSAGE, 0, 0))
            count++;

         for (i = 0; i < profiles_count; i++)
         {
            char title[PATH_MAX_LENGTH];
            char* profile               = SWITCH_GPU_PROFILES[i];
            char* speed                 = SWITCH_GPU_SPEEDS[i];

            snprintf(title, sizeof(title), "%s (%s)", profile, speed);

            if (menu_entries_append_enum(info->list, title, "", 0, MENU_SET_SWITCH_GPU_PROFILE, 0, i))
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
               bool multimedia_builtin_mediaplayer_enable = settings->bools.multimedia_builtin_mediaplayer_enable;
               if (multimedia_builtin_mediaplayer_enable)
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
         if (settings)
            count = menu_displaylist_parse_content_information(menu,
                  settings, info);

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
               ret = menu_displaylist_parse_database_entry(menu, settings,
                     info);
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
            bool video_shader_enable    = settings->bools.video_shader_enable;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                  MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE,
                  PARSE_ONLY_BOOL, false) == 0)
               count++;

            if (video_shader_enable)
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
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR),
                        msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR),
                        MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
                        0, 0, 0))
                  count++;

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
            struct string_list str_list = {0};

            new_label[0] = '\0';

            string_list_initialize(&str_list);
            string_split_noalloc(&str_list, info->path, ";");

            if (str_list.elems[0].data)
               strlcpy(new_label, str_list.elems[0].data, sizeof(new_label));
            if (str_list.elems[1].data)
               strlcpy(menu->core_buf, str_list.elems[1].data, menu->core_len);

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

            string_list_deinitialize(&str_list);
#endif
         }
         break;
      case DISPLAYLIST_CORE_CONTENT_DIRS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
#ifdef HAVE_NETWORKING
            char new_label[PATH_MAX_LENGTH];
            const char *
               network_buildbot_assets_url = settings->paths.network_buildbot_assets_url;

            new_label[0] = '\0';

            fill_pathname_join(new_label,
                  network_buildbot_assets_url,
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
      case DISPLAYLIST_CORE_SYSTEM_FILES:
#ifdef HAVE_NETWORKING
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = print_buf_lines(info->list, menu->core_buf, "",
               (int)menu->core_len, FILE_TYPE_DOWNLOAD_CORE_SYSTEM_FILES,
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
      case DISPLAYLIST_CORES_UPDATER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#ifdef HAVE_NETWORKING
         {
            core_updater_list_t *core_list   = core_updater_list_get_cached();
            menu_search_terms_t *search_terms= menu_entries_search_get_terms();
            bool show_experimental_cores     = settings->bools.network_buildbot_show_experimental_cores;
            size_t selection                 = menu_navigation_get_selection();

            if (core_list)
            {
               size_t menu_index = 0;
               size_t i;

               for (i = 0; i < core_updater_list_size(core_list); i++)
               {
                  const core_updater_list_entry_t *entry = NULL;

                  if (core_updater_list_get_index(core_list, i, &entry))
                  {
                     /* Skip 'experimental' cores, if required
                      * > Note: We always show cores that are already
                      *   installed, regardless of status (a user should
                      *   always have the option to update existing cores) */
                     if (!show_experimental_cores &&
                         (entry->is_experimental &&
                              !path_is_valid(entry->local_core_path)))
                        continue;

                     /* If a search is active, skip non-matching
                      * entries */
                     if (search_terms)
                     {
                        bool entry_valid = true;
                        size_t j;

                        for (j = 0; j < search_terms->size; j++)
                        {
                           const char *search_term = search_terms->terms[j];

                           if (!string_is_empty(search_term) &&
                               !string_is_empty(entry->display_name) &&
                               !strcasestr(entry->display_name, search_term))
                           {
                              entry_valid = false;
                              break;
                           }
                        }

                        if (!entry_valid)
                           continue;
                     }

                     if (menu_entries_append_enum(info->list,
                           entry->remote_filename,
                           "",
                           MENU_ENUM_LABEL_CORE_UPDATER_ENTRY,
                           FILE_TYPE_DOWNLOAD_CORE, 0, 0))
                     {
                        file_list_set_alt_at_offset(
                              info->list, menu_index, entry->display_name);

                        menu_index++;
                        count++;
                     }
                  }
               }
            }

            if (selection >= count)
               info->need_clear = true;
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
         count = menu_displaylist_parse_pl_thumbnail_download_list(info,
               settings);

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

         if (     string_starts_with(info->path, "content_")
               && string_ends_with(info->path, ".lpl"))
         {
            if (string_is_equal(info->path,
                     FILE_PATH_CONTENT_HISTORY))
            {
               if (menu_displaylist_ctl(DISPLAYLIST_HISTORY, info, settings))
                  return menu_displaylist_process(info);
               return false;
            }

            if (string_is_equal(info->path,
                     FILE_PATH_CONTENT_FAVORITES))
            {
               if (menu_displaylist_ctl(DISPLAYLIST_FAVORITES, info, settings))
                  return menu_displaylist_process(info);
               return false;
            }
         }

         {
            char path_playlist[PATH_MAX_LENGTH];
            playlist_t *playlist            = NULL;
            const char *dir_playlist        = settings->paths.directory_playlist;

            path_playlist[0] = '\0';

            fill_pathname_join(
                  path_playlist,
                  dir_playlist,
                  info->path,
                  sizeof(path_playlist));

            menu_displaylist_set_new_playlist(menu,
                  settings, path_playlist, true);

            strlcpy(path_playlist,
                  msg_hash_to_str(MENU_ENUM_LABEL_COLLECTION),
                  sizeof(path_playlist));

            playlist = playlist_get_cached();

            if (playlist)
               ret = menu_displaylist_parse_playlist(info,
                     playlist, settings, path_playlist, true);

            if (ret == 0)
            {
               /* Playlists themselves are sorted
                * > Display lists generated from playlists
                *   must never be sorted */
               info->need_sort    = false;
               info->need_refresh = true;
               info->need_push    = true;
            }
         }
         break;
      case DISPLAYLIST_HISTORY:
         {
            bool history_list_enable         = settings->bools.history_list_enable;
            const char *path_content_history = settings->paths.path_content_history;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            if (history_list_enable)
               menu_displaylist_parse_playlist_generic(
                     menu, info,
                     settings,
                     "history",
                     path_content_history,
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
         /* Playlists themselves are sorted
          * > Display lists generated from playlists
          *   must never be sorted */
         info->need_sort             = false;
         info->need_refresh          = true;
         info->need_push             = true;
         info->need_navigation_clear =
               history_needs_navigation_clear(menu, g_defaults.content_history);
         break;
      case DISPLAYLIST_FAVORITES:
         {
            const char *path_content_favorites = settings->paths.path_content_favorites;

            info->count                   = 0;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            menu_displaylist_parse_playlist_generic(menu, info,
                  settings,
                  "favorites",
                  path_content_favorites,
                  false, /* Not a conventional collection */
                  true,  /* Enable sorting (if allowed by user config) */
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
            /* Playlists themselves are sorted
             * > Display lists generated from playlists
             *   must never be sorted */
            info->need_sort       = false;
            info->need_refresh    = true;
            info->need_push       = true;
         }
         break;
      case DISPLAYLIST_MUSIC_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            const char *
               path_content_music_history = settings->paths.path_content_music_history;
            info->count                   = 0;

            if (settings->bools.history_list_enable)
               menu_displaylist_parse_playlist_generic(menu, info,
                     settings,
                     "music_history",
                     path_content_music_history,
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
            /* Playlists themselves are sorted
             * > Display lists generated from playlists
             *   must never be sorted */
            info->need_sort             = false;
            info->need_refresh          = true;
            info->need_push             = true;
            info->need_navigation_clear =
                  history_needs_navigation_clear(menu, g_defaults.music_history);
         }
         break;
      case DISPLAYLIST_VIDEO_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->count           = 0;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         {
            bool history_list_enable      = settings->bools.history_list_enable;
            const char *
               path_content_video_history = settings->paths.path_content_video_history;
            if (history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     settings,
                     "video_history",
                     path_content_video_history,
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
            /* Playlists themselves are sorted
             * > Display lists generated from playlists
             *   must never be sorted */
            info->need_sort             = false;
            info->need_refresh          = true;
            info->need_push             = true;
#if (defined(HAVE_FFMPEG) || defined(HAVE_MPV))
            info->need_navigation_clear =
                  history_needs_navigation_clear(menu, g_defaults.video_history);
#endif
         }
         break;
      case DISPLAYLIST_ACHIEVEMENT_PAUSE_MENU:
#ifdef HAVE_CHEEVOS
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         rcheevos_menu_populate_hardcore_pause_submenu(info);
#endif
         info->need_push = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_ACHIEVEMENT_LIST:
#ifdef HAVE_CHEEVOS
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         rcheevos_menu_populate(info);
#endif
         info->need_push    = true;
         info->need_refresh = true;
         break;
      case DISPLAYLIST_CORES_SUPPORTED:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         count = menu_displaylist_parse_supported_cores(info,
               settings, menu->deferred_path,
               MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,
               MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE);

         info->need_sort    = false;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CORES_COLLECTION_SUPPORTED:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         count = menu_displaylist_parse_supported_cores(info,
               settings, menu->deferred_path,
               MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
               MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION_CURRENT_CORE);

         info->need_sort    = false;
         info->need_refresh = true;
         info->need_push    = true;
         core_selected      = true;
         break;
      case DISPLAYLIST_CORE_INFO:
         {
            /* The number of items in the core info menu:
             * - *May* (possibly) change after performing a
             *   core restore operation (i.e. the core info
             *   files are reloaded, and if an unknown error
             *   occurs then info entries may not be available
             *   upon popping the stack)
             * - *Will* change when toggling the core lock
             *   status
             * To prevent the menu selection from going out
             * of bounds, we therefore have to check that the
             * current selection index is less than the current
             * number of menu entries - if not, we reset the
             * navigation pointer */
            size_t selection     = menu_navigation_get_selection();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count            = menu_displaylist_parse_core_info(info, settings);

            if (selection >= count)
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
            }
            info->need_push                = true;
         }
         break;
      case DISPLAYLIST_CORE_RESTORE_BACKUP_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            count              = menu_displaylist_parse_core_backup_list(
                  info, settings, true);
         }
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_CORE_DELETE_BACKUP_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            count                  = menu_displaylist_parse_core_backup_list(
                  info, settings, false);
         }
         info->need_navigation_clear = true;
         info->need_refresh          = true;
         info->need_push             = true;
         break;
      case DISPLAYLIST_CORE_MANAGER_LIST:
         {
            /* When a core is deleted, the number of items in
             * the core manager list will change. We therefore
             * have to cache the last set menu size, and reset
             * the navigation pointer if the current size is
             * different */
            static size_t prev_count = 0;
            size_t selection         = menu_navigation_get_selection();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count           = menu_displaylist_parse_core_manager_list(info,
                  settings);

            if (count == 0)
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                     msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                     MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                     FILE_TYPE_NONE, 0, 0);

            if ((count != prev_count) ||
                (selection >= count))
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
               prev_count                  = count;
            }
            info->need_push = true;
         }
         break;
#ifdef HAVE_MIST
      case DISPLAYLIST_CORE_MANAGER_STEAM_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_core_manager_steam_list(info, settings);
         info->need_navigation_clear = true;
         info->need_refresh          = false;
         info->need_push             = true;
         
         /* No core dlcs were found */
         if (count == 0)
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0))
               count++;
         
         break;
      case DISPLAYLIST_CORE_INFORMATION_STEAM_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
      
         info->need_navigation_clear = true;
         info->need_refresh          = false;
         info->need_push             = true;
               count = menu_displaylist_parse_core_information_steam(info, settings);
      
         if (count == 0)
            if (menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0))
               count++;
      
         break;
#endif
      case DISPLAYLIST_CONTENTLESS_CORES:
         {
            size_t contentless_core_ptr =
                  menu_state_get_ptr()->contentless_core_ptr;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count = menu_displaylist_contentless_cores(info->list, settings);

            /* TODO/FIXME: Selecting an entry in the
             * contentless cores list will cause the
             * quick menu to be pushed on the subsequent
             * frame via the RARCH_MENU_CTL_SET_PENDING_QUICK_MENU
             * command. The way this is implemented 'breaks' the
             * menu stack record, so when leaving the quick
             * menu via a 'cancel' operation, the last selected
             * menu index is lost. We therefore have to apply
             * a cached index value after rebuilding the list... */
            if (contentless_core_ptr < count)
               menu_navigation_set_selection(contentless_core_ptr);

            info->need_sort = false;
            info->need_push = true;
         }
         break;
      case DISPLAYLIST_CORE_OPTIONS:
         {
            /* Number of displayed options is dynamic. If user opens
             * 'Quick Menu > Core Options', toggles something
             * that changes the number of displayed items, then
             * toggles the Quick Menu off and on again (returning
             * to the Core Options menu) the menu must be refreshed
             * (or undefined behaviour occurs).
             * To prevent the menu selection from going out of bounds,
             * we therefore have to check that the current selection
             * index is less than the current number of menu entries
             * - if not, we reset the navigation pointer */
            size_t selection = menu_navigation_get_selection();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            if (retroarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
            {
               bool game_specific_options      = settings->bools.game_specific_options;
               const char *category            = info->path;
               bool is_category                = !string_is_empty(category);
               core_option_manager_t *coreopts = NULL;

               if (game_specific_options && !is_category)
                  if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST),
                        msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST),
                        MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST,
                        MENU_SETTING_ACTION_CORE_OPTION_OVERRIDE_LIST, 0, 0))
                     count++;

               if (retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts))
               {
                  nested_list_item_t *category_item = NULL;
                  nested_list_t *option_list        = NULL;
                  nested_list_item_t *option_item   = NULL;
                  const struct core_option *option  = NULL;
                  size_t i;

                  /* Empty 'category' string signifies top
                   * level core options menu */
                  if (!is_category)
                     option_list = coreopts->option_map;
                  else
                  {
                     category_item  = nested_list_get_item(coreopts->option_map,
                           category, NULL);
                     if (category_item)
                        option_list = nested_list_item_get_children(category_item);
                  }

                  if (option_list)
                  {
                     /* Loop over child options */
                     for (i = 0; i < nested_list_get_size(option_list); i++)
                     {
                        option_item = nested_list_get_item_idx(option_list, i);
                        option      = (const struct core_option *)
                              nested_list_item_get_value(option_item);

                        /* Check whether this is an option or a
                         * subcategory */
                        if (option)
                        {
                           /* This is a regular option */
                           size_t opt_idx = option->opt_idx;

                           if (core_option_manager_get_visible(coreopts, opt_idx))
                              if (menu_entries_append_enum(info->list,
                                    core_option_manager_get_desc(coreopts, opt_idx, true),
                                    "", MENU_ENUM_LABEL_CORE_OPTION_ENTRY,
                                    (unsigned)(MENU_SETTINGS_CORE_OPTION_START + opt_idx),
                                    0, 0))
                                 count++;
                        }
                        else if (option_item)
                        {
                           /* This is a subcategory */
                           const char *catgory_id = nested_list_item_get_id(option_item);
                           bool category_visible  = core_option_manager_get_category_visible(
                                 coreopts, catgory_id);

                           /* Note: We use nested_list_item_get_id() because we
                            * guarantee that the list can only be two levels
                            * deep. If we supported arbitrary nesting, would
                            * have to use nested_list_item_get_address() here */

                           if (category_visible &&
                               !string_is_empty(catgory_id))
                           {
                              if (menu_entries_append_enum(info->list,
                                    catgory_id,
                                    msg_hash_to_str(MENU_ENUM_LABEL_CORE_OPTIONS),
                                    MENU_ENUM_LABEL_CORE_OPTIONS,
                                    MENU_SETTING_ACTION_CORE_OPTIONS, 0, 0))
                                 count++;
                           }
                        }
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

            if (selection >= count)
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
            }
            info->need_push                = true;
         }
         break;
      case DISPLAYLIST_CORE_OPTION_OVERRIDE_LIST:
         {
            /* The number of items in the core option override
             * list will vary depending upon whether game or
             * content directory overrides are currently active.
             * To prevent the menu selection from going out
             * of bounds, we therefore have to check that the
             * current selection index is less than the current
             * number of menu entries - if not, we reset the
             * navigation pointer */
            size_t selection = menu_navigation_get_selection();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count = menu_displaylist_parse_core_option_override_list(info, settings);

            if (selection >= count)
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
            }
            info->need_push                = true;
         }
         break;
      case DISPLAYLIST_REMAP_FILE_MANAGER:
         {
            /* The number of items in the remap file manager
             * list will vary depending upon which remap type
             * is currently active (if any).
             * To prevent the menu selection from going out
             * of bounds, we therefore have to check that the
             * current selection index is less than the current
             * number of menu entries - if not, we reset the
             * navigation pointer */
            size_t selection = menu_navigation_get_selection();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count = menu_displaylist_parse_remap_file_manager_list(info, settings);

            if (selection >= count)
            {
               info->need_refresh          = true;
               info->need_navigation_clear = true;
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

      case DISPLAYLIST_PLAYLIST_MANAGER_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_parse_playlist_manager_list(info, settings);

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
         if (!menu_displaylist_parse_playlist_manager_settings(menu,
                  settings,
                  info, info->path))
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;

         break;
      case DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PARAMETER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         count = menu_displaylist_build_shader_parameter(
               info, info->list, info->type,
               MENU_SETTINGS_SHADER_PARAMETER_0,
               MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PARAM);

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_PRESET_PARAMETER:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         count = menu_displaylist_build_shader_parameter(
               info, info->list, info->type,
               MENU_SETTINGS_SHADER_PRESET_PARAMETER_0,
               MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PRESET_PARAM);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST_INPUT_DEVICE_TYPE:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count              = menu_displaylist_parse_input_device_type_list(info, settings);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST_INPUT_DEVICE_INDEX:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count              = menu_displaylist_parse_input_device_index_list(info, settings);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST_INPUT_DESCRIPTION:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count              = menu_displaylist_parse_input_description_list(info, settings);
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST_INPUT_DESCRIPTION_KBD:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count              = menu_displaylist_parse_input_description_kbd_list(info, settings);
         info->need_refresh = true;
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
      case DISPLAYLIST_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST:
      case DISPLAYLIST_LATENCY_SETTINGS_LIST:
#if defined(HAVE_OVERLAY)
      case DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST:
#endif
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
      case DISPLAYLIST_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST:
      case DISPLAYLIST_VIDEO_WINDOWED_MODE_SETTINGS_LIST:
      case DISPLAYLIST_VIDEO_OUTPUT_SETTINGS_LIST:
      case DISPLAYLIST_VIDEO_HDR_SETTINGS_LIST:
      case DISPLAYLIST_VIDEO_SYNCHRONIZATION_SETTINGS_LIST:
      case DISPLAYLIST_VIDEO_SCALING_SETTINGS_LIST:
      case DISPLAYLIST_OPTIONS_DISK:
      case DISPLAYLIST_AI_SERVICE_SETTINGS_LIST:
      case DISPLAYLIST_ACCESSIBILITY_SETTINGS_LIST:
      case DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST:
      case DISPLAYLIST_ACCOUNTS_TWITCH_LIST:
      case DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
      case DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST:
      case DISPLAYLIST_ACCOUNTS_FACEBOOK_LIST:
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
      case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_SORT_MODE:
      case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
      case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_CORE_NAME:
      case DISPLAYLIST_DROPDOWN_LIST_DISK_INDEX:
      case DISPLAYLIST_PERFCOUNTERS_CORE:
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
      case DISPLAYLIST_MENU_SETTINGS_LIST:
      case DISPLAYLIST_ADD_CONTENT_LIST:
      case DISPLAYLIST_INPUT_SETTINGS_LIST:
      case DISPLAYLIST_INPUT_MENU_SETTINGS_LIST:
      case DISPLAYLIST_FRAME_TIME_COUNTER_SETTINGS_LIST:
      case DISPLAYLIST_BROWSE_URL_LIST:
      case DISPLAYLIST_DISC_INFO:
      case DISPLAYLIST_DUMP_DISC:
#ifdef HAVE_LAKKA
      case DISPLAYLIST_EJECT_DISC:
#endif
      case DISPLAYLIST_LOAD_CONTENT_LIST:
      case DISPLAYLIST_LOAD_CONTENT_SPECIAL:
      case DISPLAYLIST_OPTIONS_REMAPPINGS:
      case DISPLAYLIST_VIDEO_SETTINGS_LIST:
      case DISPLAYLIST_AUDIO_SETTINGS_LIST:
      case DISPLAYLIST_AUDIO_RESAMPLER_SETTINGS_LIST:
      case DISPLAYLIST_AUDIO_OUTPUT_SETTINGS_LIST:
      case DISPLAYLIST_AUDIO_SYNCHRONIZATION_SETTINGS_LIST:
      case DISPLAYLIST_HELP_SCREEN_LIST:
      case DISPLAYLIST_INFORMATION_LIST:
      case DISPLAYLIST_EXPLORE:
      case DISPLAYLIST_SCAN_DIRECTORY_LIST:
      case DISPLAYLIST_SYSTEM_INFO:
      case DISPLAYLIST_BLUETOOTH_SETTINGS_LIST:
      case DISPLAYLIST_WIFI_SETTINGS_LIST:
      case DISPLAYLIST_WIFI_NETWORKS_LIST:
      case DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST:
      case DISPLAYLIST_BROWSE_URL_START:
      case DISPLAYLIST_CONTENT_SETTINGS:
      case DISPLAYLIST_NETPLAY_ROOM_LIST:
      case DISPLAYLIST_SHADER_PRESET_SAVE:
      case DISPLAYLIST_SHADER_PRESET_REMOVE:
      case DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST:
      case DISPLAYLIST_INPUT_TURBO_FIRE_SETTINGS_LIST:
      case DISPLAYLIST_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST:
      case DISPLAYLIST_PLAYLIST_SETTINGS_LIST:
      case DISPLAYLIST_SUBSYSTEM_SETTINGS_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         count = menu_displaylist_build_list(info->list, settings, type, false);

         if (count == 0)
         {
            switch (type)
            {
               case DISPLAYLIST_SHADER_PRESET_REMOVE:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_PRESETS_FOUND),
                        MENU_ENUM_LABEL_NO_PRESETS_FOUND,
                        0, 0, 0);
                  break;
               case DISPLAYLIST_DISC_INFO:
               case DISPLAYLIST_DUMP_DISC:
#ifdef HAVE_LAKKA
               case DISPLAYLIST_EJECT_DISC:
#endif
               case DISPLAYLIST_MENU_SETTINGS_LIST:
               case DISPLAYLIST_ADD_CONTENT_LIST:
               case DISPLAYLIST_DROPDOWN_LIST_RESOLUTION:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_DEFAULT_CORE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_PLAYLIST_SORT_MODE:
               case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
               case DISPLAYLIST_DROPDOWN_LIST_MANUAL_CONTENT_SCAN_CORE_NAME:
               case DISPLAYLIST_DROPDOWN_LIST_DISK_INDEX:
               case DISPLAYLIST_INFORMATION_LIST:
               case DISPLAYLIST_SCAN_DIRECTORY_LIST:
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
               case DISPLAYLIST_BLUETOOTH_SETTINGS_LIST:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_BT_DEVICES_FOUND),
                        MENU_ENUM_LABEL_NO_BT_DEVICES_FOUND,
                        0, 0, 0);
                  break;
               case DISPLAYLIST_WIFI_NETWORKS_LIST:
                  menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND),
                        msg_hash_to_str(MENU_ENUM_LABEL_NO_NETWORKS_FOUND),
                        MENU_ENUM_LABEL_NO_NETWORKS_FOUND,
                        0, 0, 0);
                  break;
               case DISPLAYLIST_NETPLAY_ROOM_LIST:
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

         /* Special pass */
         switch (type)
         {
            case DISPLAYLIST_DISC_INFO:
            case DISPLAYLIST_DUMP_DISC:
#ifdef HAVE_LAKKA
            case DISPLAYLIST_EJECT_DISC:
#endif
               info->need_clear   = true;
               break;
            default:
               break;
         }

         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_list(menu, settings, info);

         /* Playlists themselves are sorted
          * > Display lists generated from playlists
          *   must never be sorted */
         info->need_sort    = false;
         info->need_refresh = true;
         info->need_push    = true;
         break;
      case DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         ret = menu_displaylist_parse_horizontal_content_actions(menu,
               settings, info);
         info->need_refresh  = true;
         info->need_push     = true;

         if (core_selected)
         {
            info->need_clear = true;
            core_selected    = false;
         }

         break;
      case DISPLAYLIST_OPTIONS:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
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

#ifdef HAVE_COMPRESSION
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES),
                     MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
#endif
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS),
                     MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,
                     MENU_SETTING_ACTION, 0, 0))
               count++;

#elif defined(HAVE_NETWORKING)
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#else
#ifdef HAVE_UPDATE_CORES
            if (settings->bools.menu_show_core_updater)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                        msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_LIST),
                        MENU_ENUM_LABEL_CORE_UPDATER_LIST,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;

               /* Only show 'update installed cores' if
                * one or more cores are installed */
               if (core_info_count() > 0)
               {
#if defined(ANDROID)
                  /* When using Play Store builds, cores are
                   * updated automatically - the 'update
                   * installed cores' option is therefore
                   * irrelevant/useless, so we instead present
                   * an option for switching any existing buildbot
                   * or sideloaded cores to the latest Play Store
                   * version */
                  if (play_feature_delivery_enabled())
                  {
                     if (menu_entries_append_enum(info->list,
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD),
                              msg_hash_to_str(MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD),
                              MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD,
                              MENU_SETTING_ACTION, 0, 0))
                        count++;
                  }
                  else
#endif
                     if (menu_entries_append_enum(info->list,
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES),
                              msg_hash_to_str(MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES),
                              MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES,
                              MENU_SETTING_ACTION, 0, 0))
                        count++;
               }
            }
#endif
#endif

#if defined(HAVE_COMPRESSION) && !defined(HAVE_MIST)
            if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES),
                     MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES,
                     MENU_SETTING_ACTION, 0, 0))
               count++;
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
#if !defined(_3DS)
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
#endif
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
#if !defined(_3DS)
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
#endif

#ifdef HAVE_NETWORKING
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
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
      case DISPLAYLIST_OPTIONS_OVERRIDES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            bool has_content = !string_is_empty(path_get(RARCH_PATH_CONTENT));

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

            if (has_content
                  && settings->bools.quick_menu_show_save_content_dir_overrides
                  && !settings->bools.kiosk_mode_enable)
            {
               if (menu_entries_append_enum(info->list,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                        msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR),
                        MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
                        MENU_SETTING_ACTION, 0, 0))
                  count++;
            }

            if (has_content
                  && settings->bools.quick_menu_show_save_game_overrides
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
            rarch_system_info_t *sys_info  = &runloop_state_get_ptr()->system;
            bool show_add_content          = false;
#if defined(HAVE_RGUI) || defined(HAVE_MATERIALUI) || defined(HAVE_OZONE) || defined(HAVE_XMB)
            const char *menu_ident         = menu_driver_ident();
#endif

            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
            {
               if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                           MENU_ENUM_LABEL_CONTENT_SETTINGS,
                           PARSE_ACTION, false) == 0)
                     count++;
            }
            else
            {
               if (sys_info && sys_info->load_no_content)
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                           MENU_ENUM_LABEL_START_CORE, PARSE_ACTION, false) == 0)
                     count++;

#ifndef HAVE_DYNAMIC
               if (frontend_driver_has_fork())
#endif
               {
                  if (settings->bools.menu_show_load_core)
                  {
                     if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                              MENU_ENUM_LABEL_CORE_LIST, PARSE_ACTION, false) == 0)
                        count++;
                  }
               }
            }

            if (settings->bools.menu_show_load_content)
            {
               /* Core not loaded completely, use the data we
                * peeked on load core */

               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;

               if (menu_displaylist_has_subsystems())
               {
                  if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                           MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,
                           PARSE_ACTION, false) == 0)
                     count++;
               }
            }

            if (settings->bools.menu_content_show_history)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,
                     PARSE_ACTION, false) == 0)
                  count++;

#ifdef HAVE_CDROM
            if (settings->bools.menu_show_load_disc)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                        MENU_ENUM_LABEL_LOAD_DISC,
                        PARSE_ACTION, false) == 0)
                  count++;
            }

            if (settings->bools.menu_show_dump_disc)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                        MENU_ENUM_LABEL_DUMP_DISC,
                        PARSE_ACTION, false) == 0)
                  count++;
            }

#ifdef HAVE_LAKKA
            if (settings->bools.menu_show_eject_disc)
            {
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                        MENU_ENUM_LABEL_EJECT_DISC,
                        PARSE_ACTION, false) == 0)
                  count++;
            }
#endif /* HAVE_LAKKA */
#endif

#if defined(HAVE_RGUI) || defined(HAVE_MATERIALUI)
            if ((string_is_equal(menu_ident, "rgui") ||
                 string_is_equal(menu_ident, "glui")) &&
                  settings->bools.menu_content_show_playlists)
               if (menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                     msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                     MENU_ENUM_LABEL_PLAYLISTS_TAB,
                     MENU_SETTING_ACTION, 0, 0))
                  count++;
#endif

#if defined(HAVE_XMB) || defined(HAVE_OZONE)
            if (string_is_equal(menu_ident, "xmb") ||
                string_is_equal(menu_ident, "ozone"))
               show_add_content = settings->bools.menu_content_show_add;
            else
#endif
               show_add_content = (settings->uints.menu_content_show_add_entry ==
                     MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB);

            if (show_add_content)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_ADD_CONTENT_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;

#ifdef HAVE_NETWORKING
            if (settings->bools.menu_content_show_netplay)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_NETPLAY,
                     PARSE_ACTION, false) == 0)
                  count++;
#endif
#ifdef HAVE_ONLINE_UPDATER
            if (settings->bools.menu_show_online_updater)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_ONLINE_UPDATER,
                     PARSE_ACTION, false) == 0)
                  count++;
#endif
#ifdef HAVE_MIST
            if (settings->bools.menu_show_core_manager_steam)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;
#endif
            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                  MENU_ENUM_LABEL_SETTINGS, PARSE_ACTION, false) == 0)
               count++;
            if (settings->bools.menu_show_information)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_INFORMATION_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;
#ifdef HAVE_CONFIGFILE
            if (settings->bools.menu_show_configurations)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                        MENU_ENUM_LABEL_CONFIGURATIONS_LIST,
                        PARSE_ACTION, false) == 0)
                  count++;
#endif
            if (settings->bools.menu_show_help)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_HELP_LIST,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (settings->bools.menu_show_restart_retroarch)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_RESTART_RETROARCH,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (settings->bools.menu_show_quit_retroarch)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_QUIT_RETROARCH,
                     PARSE_ACTION, false) == 0)
                  count++;

            if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
               MENU_ENUM_LABEL_SWITCH_GPU_PROFILE,
               PARSE_ACTION, false) == 0)
               count++;

            if (settings->bools.menu_show_reboot)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
                     MENU_ENUM_LABEL_REBOOT,
                     PARSE_ACTION, false) == 0)
                  count++;
            if (settings->bools.menu_show_shutdown)
               if (MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(info->list,
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
      case DISPLAYLIST_INFO:
         if (menu_entries_append_enum(info->list, info->path,
               info->label, MSG_UNKNOWN, info->type, info->directory_ptr, 0))
            count++;
         break;
      case DISPLAYLIST_FILE_BROWSER_SCAN_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_DIR:
      case DISPLAYLIST_FILE_BROWSER_SELECT_FILE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_CORE:
      case DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION:
      case DISPLAYLIST_GENERIC:
         info->need_navigation_clear = true;
         /* fall-through */
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
            ret = MENU_DISPLAYLIST_PARSE_SETTINGS(info->list,
                  lbl, PARSE_NONE, true, MENU_SETTINGS_INPUT_BEGIN);
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_DATABASES:
         {
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
            struct string_list str_list = {0};
            char new_exts[PATH_MAX_LENGTH];
            union string_list_elem_attr attr;

            attr.i = 0;

            new_exts[0] = '\0';

            string_list_initialize(&str_list);

            filebrowser_clear_type();

            switch (type)
            {
               case DISPLAYLIST_SHADER_PRESET:
                  info->type_default = FILE_TYPE_SHADER_PRESET;
                  if (video_shader_is_supported(RARCH_SHADER_CG))
                     string_list_append(&str_list, "cgp", attr);
                  if (video_shader_is_supported(RARCH_SHADER_GLSL))
                     string_list_append(&str_list, "glslp", attr);
                  if (video_shader_is_supported(RARCH_SHADER_SLANG))
                     string_list_append(&str_list, "slangp", attr);
                  break;
               case DISPLAYLIST_SHADER_PASS:
                  info->type_default = FILE_TYPE_SHADER;
                  if (video_shader_is_supported(RARCH_SHADER_CG))
                     string_list_append(&str_list, "cg", attr);
                  if (video_shader_is_supported(RARCH_SHADER_GLSL))
                     string_list_append(&str_list, "glsl", attr);
                  if (video_shader_is_supported(RARCH_SHADER_SLANG))
                     string_list_append(&str_list, "slang", attr);
                  break;
               default:
                  break;
            }

            string_list_join_concat(new_exts, sizeof(new_exts), &str_list, "|");
            if (!string_is_empty(info->exts))
               free(info->exts);
            info->exts = strdup(new_exts);
            string_list_deinitialize(&str_list);
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
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            menu_displaylist_parse_playlist_generic(menu, info,
                  settings,
                  path_basename_nocompression(info->path),
                  info->path,
                  true, /* Is a collection */
                  true, /* Enable sorting (if allowed by user config) */
                  &ret);
         }
         ret = 0; /* Why do we do this...? */

         if (ret == 0)
         {
            /* Playlists themselves are sorted
             * > Display lists generated from playlists
             *   must never be sorted */
            info->need_sort    = false;
            info->need_refresh = true;
            info->need_push    = true;
         }
         break;
      case DISPLAYLIST_IMAGES_HISTORY:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         info->count           = 0;
#ifdef HAVE_IMAGEVIEWER
         {
            bool history_list_enable  = settings->bools.history_list_enable;
            const char *path_content_image_history = settings->paths.path_content_image_history;

            if (history_list_enable)
            {
               menu_displaylist_parse_playlist_generic(menu, info,
                     settings,
                     "images_history",
                     path_content_image_history,
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
         /* Playlists themselves are sorted
          * > Display lists generated from playlists
          *   must never be sorted */
         info->need_sort             = false;
         info->need_refresh          = true;
         info->need_push             = true;
#if defined(HAVE_IMAGEVIEWER)
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
      case DISPLAYLIST_VIDEO_FONTS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_MANUAL_CONTENT_SCAN_DAT_FILES:
      case DISPLAYLIST_FILE_BROWSER_SELECT_SIDELOAD_CORE:
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
            case DISPLAYLIST_VIDEO_FONTS:
               info->type_default = FILE_TYPE_VIDEO_FONT;
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
            case DISPLAYLIST_MANUAL_CONTENT_SCAN_DAT_FILES:
               info->type_default = FILE_TYPE_MANUAL_SCAN_DAT;
               info->exts         = strdup("dat|xml");
               break;
            case DISPLAYLIST_FILE_BROWSER_SELECT_SIDELOAD_CORE:
               {
                  char ext_names[255];
                  ext_names[0] = '\0';

                  info->type_default = FILE_TYPE_SIDELOAD_CORE;

                  if (frontend_driver_get_core_extension(ext_names, sizeof(ext_names)))
                  {
                     strlcat(ext_names, "|", sizeof(ext_names));
                     strlcat(ext_names, FILE_PATH_CORE_BACKUP_EXTENSION_NO_DOT, sizeof(ext_names));
                  }
                  else
                     strcpy_literal(ext_names, FILE_PATH_CORE_BACKUP_EXTENSION_NO_DOT);

                  info->exts      = strdup(ext_names);
               }
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
            bool is_horizontal   =
               (type == DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL);

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
            count =  menu_displaylist_parse_playlists(info,
                  settings, is_horizontal);

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

         count = menu_displaylist_parse_cores(menu, settings, info);

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
      case DISPLAYLIST_MANUAL_CONTENT_SCAN_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (!menu_displaylist_parse_manual_content_scan_list(info))
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
                  msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
                  MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
                  FILE_TYPE_NONE, 0, 0);

         info->need_push    = true;
         break;
      case DISPLAYLIST_DROPDOWN_LIST:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
         {
            if (string_starts_with_size(info->path, "core_option_",
                  STRLEN_CONST("core_option_")))
               count = menu_displaylist_parse_core_option_dropdown_list(info);
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
                           struct string_list tmp_str_list = {0};
                           string_list_initialize(&tmp_str_list);
                           string_split_noalloc(&tmp_str_list,
                                 setting->values, "|");

                           if (tmp_str_list.size > 0)
                           {
                              unsigned i;
                              unsigned size        = (unsigned)
                                 tmp_str_list.size;
                              bool checked_found   = false;
                              unsigned checked     = 0;

                              char* orig_val = setting->get_string_representation ?
                                 strdup(setting->value.target.string) : setting->value.target.string;
                              char val_s[256], val_d[32];
                              snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);

                              for (i = 0; i < size; i++)
                              {
                                 const char* val = tmp_str_list.elems[i].data;
                                 if (setting->get_string_representation)
                                 {
                                    strlcpy(setting->value.target.string, val, setting->size);
                                    setting->get_string_representation(setting,
                                       val_s, sizeof(val_s));
                                    val = val_s;
                                 }

                                 if (menu_entries_append_enum(info->list,
                                       val,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM, i, 0))
                                    count++;

                                 if (!checked_found && string_is_equal(
                                          tmp_str_list.elems[i].data,
                                          orig_val))
                                 {
                                    checked = i;
                                    checked_found = true;
                                 }
                              }

                              if (setting->get_string_representation)
                              {
                                 strlcpy(setting->value.target.string, orig_val, setting->size);
                                 free(orig_val);
                              }

                              if (checked_found)
                              {
                                 menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                                 if (cbs)
                                    cbs->checked = true;
                                 menu_navigation_set_selection(checked);
                              }
                           }

                           string_list_deinitialize(&tmp_str_list);
                        }
                        break;
                     case ST_INT:
                        {
                           float i;
                           int32_t orig_value     = *setting->value.target.integer;
                           unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_INT_ITEM;
                           float step             = setting->step;
                           double min             = setting->enforce_minrange ? setting->min : 0.00;
                           double max             = setting->enforce_maxrange ? setting->max : 99999.00;
                           bool checked_found     = false;
                           unsigned checked       = 0;
                           unsigned entry_index   = 0;

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
                                 if (menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0))
                                    count++;

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
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

                                 if (menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0))
                                    count++;

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
                              }
                           }

                           if (checked_found)
                           {
                              menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                              if (cbs)
                                 cbs->checked = true;
                              menu_navigation_set_selection(checked);
                           }
                        }
                        break;
                     case ST_FLOAT:
                        {
                           float i;
                           float orig_value       = *setting->value.target.fraction;
                           unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM;
                           float step             = setting->step;
                           float half_step        = step * 0.5f;
                           double min             = setting->enforce_minrange ? setting->min : 0.00;
                           double max             = setting->enforce_maxrange ? setting->max : 9999.00;
                           bool checked_found     = false;
                           unsigned checked       = 0;
                           unsigned entry_index   = 0;

                           if (setting->get_string_representation)
                           {
                              for (i = min; i <= max; i += step)
                              {
                                 char val_s[256], val_d[256];

                                 *setting->value.target.fraction = i;

                                 setting->get_string_representation(setting,
                                       val_s, sizeof(val_s));
                                 snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                                 if (menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, 0, 0))
                                    count++;

                                 if (!checked_found && (fabs(i - orig_value) < half_step))
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
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

                                 if (menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, 0, 0))
                                    count++;

                                 if (!checked_found && (fabs(i - orig_value) < half_step))
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
                              }
                           }

                           if (checked_found)
                           {
                              menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                              if (cbs)
                                 cbs->checked = true;
                              menu_navigation_set_selection(checked);
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
                           double max             = setting->enforce_maxrange ? setting->max : 9999.00;
                           bool checked_found     = false;
                           unsigned checked       = 0;
                           unsigned entry_index   = 0;

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
                                 if (menu_entries_append_enum(info->list,
                                          val_s,
                                          val_d,
                                          MENU_ENUM_LABEL_NO_ITEMS,
                                          setting_type, val, 0))
                                    count++;

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
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

                                 if (menu_entries_append_enum(info->list,
                                       val_s,
                                       val_d,
                                       MENU_ENUM_LABEL_NO_ITEMS,
                                       setting_type, val, 0))
                                    count++;

                                 if (!checked_found && val == orig_value)
                                 {
                                    checked       = entry_index;
                                    checked_found = true;
                                 }

                                 entry_index++;
                              }
                           }

                           if (checked_found)
                           {
                              menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                              if (cbs)
                                 cbs->checked = true;
                              menu_navigation_set_selection(checked);
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
      case DISPLAYLIST_DROPDOWN_LIST_VIDEO_SHADER_NUM_PASSES:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            unsigned i;
            struct video_shader *shader = menu_shader_get();
            unsigned pass_count         = shader ? shader->passes : 0;

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            for (i = 0; i < GFX_MAX_SHADERS+1; i++)
            {
               char val_d[256];
               snprintf(val_d, sizeof(val_d), "%d", i);
               if (menu_entries_append_enum(info->list,
                        val_d,
                        val_d,
                        MENU_ENUM_LABEL_NO_ITEMS,
                        MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_NUM_PASS, i, 0))
                  count++;

               if (i == pass_count)
               {
                  menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[i].actiondata;
                  if (cbs)
                     cbs->checked = true;
                  menu_navigation_set_selection(i);
               }
            }

            info->need_refresh       = true;
            info->need_push          = true;
         }
#endif
         break;
      case DISPLAYLIST_DROPDOWN_LIST_SPECIAL:
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

         if (string_starts_with_size(info->path, "core_option_",
               STRLEN_CONST("core_option_")))
            count = menu_displaylist_parse_core_option_dropdown_list(info);
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
                        struct string_list tmp_str_list = {0};

                        string_list_initialize(&tmp_str_list);
                        string_split_noalloc(&tmp_str_list,
                              setting->values, "|");

                        if (tmp_str_list.size > 0)
                        {
                           unsigned i;
                           unsigned size        = (unsigned)tmp_str_list.size;
                           bool checked_found   = false;
                           unsigned checked     = 0;

                           for (i = 0; i < size; i++)
                           {
                              char val_d[256];
                              snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                              if (menu_entries_append_enum(info->list,
                                    tmp_str_list.elems[i].data,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM_SPECIAL, i, 0))
                                 count++;

                              if (!checked_found &&
                                    string_is_equal(tmp_str_list.elems[i].data,
                                       setting->value.target.string))
                              {
                                 checked = i;
                                 checked_found = true;
                              }
                           }

                           if (checked_found)
                           {
                              menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                              if (cbs)
                                 cbs->checked = true;
                              menu_navigation_set_selection(checked);
                           }
                        }

                        string_list_deinitialize(&tmp_str_list);
                     }
                     break;
                  case ST_INT:
                     {
                        float i;
                        int32_t orig_value     = *setting->value.target.integer;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_INT_ITEM_SPECIAL;
                        float step             = setting->step;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 9999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;
                        unsigned entry_index   = 0;

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
                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, val, 0))
                                 count++;

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
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

                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, val, 0))
                                 count++;

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
                           }
                        }

                        if (checked_found)
                        {
                           menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                           if (cbs)
                              cbs->checked = true;
                           menu_navigation_set_selection(checked);
                        }
                     }
                     break;
                  case ST_FLOAT:
                     {
                        float i;
                        float orig_value       = *setting->value.target.fraction;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM_SPECIAL;
                        float step             = setting->step;
                        float half_step        = step * 0.5f;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 9999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;
                        unsigned entry_index   = 0;

                        if (setting->get_string_representation)
                        {
                           for (i = min; i <= max; i += step)
                           {
                              char val_s[256], val_d[256];

                              *setting->value.target.fraction = i;

                              setting->get_string_representation(setting,
                                    val_s, sizeof(val_s));
                              snprintf(val_d, sizeof(val_d), "%d", setting->enum_idx);
                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, 0, 0))
                                 count++;

                              if (!checked_found && (fabs(i - orig_value) < half_step))
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
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

                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, 0, 0))
                                 count++;

                              if (!checked_found && (fabs(i - orig_value) < half_step))
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
                           }
                        }

                        if (checked_found)
                        {
                           menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                           if (cbs)
                              cbs->checked = true;
                           menu_navigation_set_selection(checked);
                        }
                     }
                     break;
                  case ST_UINT:
                     {
                        float i;
                        unsigned orig_value    = *setting->value.target.unsigned_integer;
                        unsigned setting_type  = MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL;
                        float step             = setting->step;
                        double min             = setting->enforce_minrange ? setting->min : 0.00;
                        double max             = setting->enforce_maxrange ? setting->max : 9999.00;
                        bool checked_found     = false;
                        unsigned checked       = 0;
                        unsigned entry_index   = 0;

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
                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, val, 0))
                                 count++;

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
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

                              if (menu_entries_append_enum(info->list,
                                    val_s,
                                    val_d,
                                    MENU_ENUM_LABEL_NO_ITEMS,
                                    setting_type, val, 0))
                                 count++;

                              if (!checked_found && val == orig_value)
                              {
                                 checked       = entry_index;
                                 checked_found = true;
                              }

                              entry_index++;
                           }
                        }

                        if (checked_found)
                        {
                           menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)info->list->list[checked].actiondata;
                           if (cbs)
                              cbs->checked = true;
                           menu_navigation_set_selection(checked);
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
         break;
      case DISPLAYLIST_NONE:
         break;
   }

   if (use_filebrowser)
   {
      if (string_is_empty(info->path))
      {
         if (frontend_driver_parse_drive_list(info->list, load_content) != 0)
            if (menu_entries_append_enum(info->list, "/", "",
                  load_content ?
                        MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
                        MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY,
                  FILE_TYPE_DIRECTORY, 0, 0))
               count++;
      }
      else
      {
         const char *pending_selection              = menu_driver_get_pending_selection();
         bool show_hidden_files                     = settings->bools.show_hidden_files;
         bool multimedia_builtin_mediaplayer_enable = settings->bools.multimedia_builtin_mediaplayer_enable;
         bool multimedia_builtin_imageviewer_enable = settings->bools.multimedia_builtin_imageviewer_enable;
         bool filter_supported_extensions_enable    = settings->bools.menu_navigation_browser_filter_supported_extensions_enable;

         filebrowser_parse(info, type,
               show_hidden_files,
               multimedia_builtin_mediaplayer_enable,
               multimedia_builtin_imageviewer_enable,
               filter_supported_extensions_enable
               );

         /* Apply pending selection */
         if (!string_is_empty(pending_selection))
         {
            size_t selection_idx = 0;

            if (menu_entries_list_search(pending_selection, &selection_idx))
            {
               menu_navigation_set_selection(selection_idx);
               menu_driver_navigation_set(true);
            }

            menu_driver_set_pending_selection(NULL);
         }
      }

      info->need_refresh = true;
      info->need_push    = true;
   }

   if (ret != 0)
      return false;

   return true;
}
