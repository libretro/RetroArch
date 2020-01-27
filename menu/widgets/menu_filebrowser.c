/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2017-2019 - Andrés Suárez
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

#include <compat/strl.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <file/archive_file.h>

#include <lists/dir_list.h>

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "menu_filebrowser.h"

#include "../menu_driver.h"
#include "../menu_displaylist.h"

#include "../../configuration.h"
#include "../../paths.h"

#include "../../retroarch.h"
#include "../../core.h"
#include "../../content.h"
#include "../../verbosity.h"
#include "../../dynamic.h"

static enum filebrowser_enums filebrowser_types = FILEBROWSER_NONE;

enum filebrowser_enums filebrowser_get_type(void)
{
   return filebrowser_types;
}

void filebrowser_clear_type(void)
{
   filebrowser_types = FILEBROWSER_NONE;
}

void filebrowser_set_type(enum filebrowser_enums type)
{
   filebrowser_types = type;
}

void filebrowser_parse(menu_displaylist_info_t *info, unsigned type_data)
{
   size_t i, list_size;
   struct string_list *str_list         = NULL;
   unsigned items_found                 = 0;
   unsigned files_count                 = 0;
   unsigned dirs_count                  = 0;
   settings_t *settings                 = config_get_ptr();
   enum menu_displaylist_ctl_state type = (enum menu_displaylist_ctl_state)
                                          type_data;
   const char *path                     = info ? info->path : NULL;
   bool path_is_compressed              = !string_is_empty(path)
      ? path_is_compressed_file(path) : false;
   bool filter_ext                      =
      settings->bools.menu_navigation_browser_filter_supported_extensions_enable;

   rarch_system_info_t *system = runloop_get_system_info();
   const struct retro_subsystem_info *subsystem;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data + content_get_subsystem();
   /* Core not loaded completely, use the data we peeked on load core */
   else
      subsystem = subsystem_data + content_get_subsystem();

   if (info && (info->type_default == FILE_TYPE_SHADER_PRESET ||
                info->type_default == FILE_TYPE_SHADER))
      filter_ext = true;

   if (info && string_is_equal(info->label,
            msg_hash_to_str(MENU_ENUM_LABEL_SCAN_FILE)))
      filter_ext = false;

   if (info && path_is_compressed)
   {
      if (filebrowser_types != FILEBROWSER_SELECT_FILE_SUBSYSTEM)
         str_list = file_archive_get_file_list(path, info->exts);
      else if (subsystem && subsystem_current_count > 0)
         str_list  = file_archive_get_file_list(path, subsystem->roms[content_get_subsystem_rom_id()].valid_extensions);
   }
   else if (!string_is_empty(path))
   {
      if (filebrowser_types == FILEBROWSER_SELECT_FILE_SUBSYSTEM)
      {
         if (subsystem && subsystem_current_count > 0 && content_get_subsystem_rom_id() < subsystem->num_roms)
            str_list = dir_list_new(path,
                  (filter_ext && info) ? subsystem->roms[content_get_subsystem_rom_id()].valid_extensions : NULL,
                  true, settings->bools.show_hidden_files, true, false);
      }
      else if (info && ((info->type_default == FILE_TYPE_MANUAL_SCAN_DAT) || (info->type_default == FILE_TYPE_SIDELOAD_CORE)))
         str_list = dir_list_new(path,
               info->exts, true, settings->bools.show_hidden_files, false, false);
      else
         str_list = dir_list_new(path,
               (filter_ext && info) ? info->exts : NULL,
               true, settings->bools.show_hidden_files, true, false);
   }

   switch (filebrowser_types)
   {
      case FILEBROWSER_SCAN_DIR:
#ifdef HAVE_LIBRETRODB
         if (info)
            menu_entries_prepend(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY),
                  MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY,
                  FILE_TYPE_SCAN_DIRECTORY, 0 ,0);
#endif
         break;
      case FILEBROWSER_MANUAL_SCAN_DIR:
         if (info)
            menu_entries_prepend(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY),
                  MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY,
                  FILE_TYPE_MANUAL_SCAN_DIRECTORY, 0 ,0);
         break;
      case FILEBROWSER_SELECT_DIR:
         if (info)
            menu_entries_prepend(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_USE_THIS_DIRECTORY),
                  MENU_ENUM_LABEL_USE_THIS_DIRECTORY,
                  FILE_TYPE_USE_DIRECTORY, 0 ,0);
         break;
      default:
         break;
   }

   if (!str_list)
   {
      const char *str = path_is_compressed
         ? msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE)
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND);

      if (info)
         menu_entries_append_enum(info->list, str, "",
               MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND, 0, 0, 0);
      goto end;
   }

   dir_list_sort(str_list, true);

   list_size = str_list->size;

   if (list_size == 0)
   {
      string_list_free(str_list);
      str_list = NULL;
   }
   else
   {
      for (i = 0; i < list_size; i++)
      {
         char label[64];
         bool is_dir                   = false;
         enum msg_hash_enums enum_idx  = MSG_UNKNOWN;
         enum msg_file_type file_type  = FILE_TYPE_NONE;
         const char *path              = str_list->elems[i].data;

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
               if (filebrowser_types == FILEBROWSER_SELECT_FONT)
                  file_type = FILE_TYPE_FONT;
               else
                  file_type = (enum msg_file_type)info->type_default;
               switch (type)
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  case DISPLAYLIST_CORES_DETECTED:
                     if (path_is_compressed_file(path))
                        file_type = FILE_TYPE_CARCHIVE;
                     break;
                  default:
                     break;
               }
               break;
         }

         is_dir = (file_type == FILE_TYPE_DIRECTORY);

         if (!is_dir)
         {
            if (filebrowser_types == FILEBROWSER_SELECT_DIR)
               continue;
            if (filebrowser_types == FILEBROWSER_SCAN_DIR)
               continue;
            if (filebrowser_types == FILEBROWSER_MANUAL_SCAN_DIR)
               continue;
         }

         /* Need to preserve slash first time. */

         if (!string_is_empty(path) && !path_is_compressed)
            path = path_basename(path);

         if (filebrowser_types == FILEBROWSER_SELECT_COLLECTION)
         {
            if (is_dir)
               file_type = FILE_TYPE_DIRECTORY;
            else
               file_type = FILE_TYPE_PLAYLIST_COLLECTION;
         }

         if (!is_dir && path_is_media_type(path) == RARCH_CONTENT_MUSIC)
            file_type = FILE_TYPE_MUSIC;
         else if (!is_dir &&
               (settings->bools.multimedia_builtin_mediaplayer_enable ||
                settings->bools.multimedia_builtin_imageviewer_enable))
         {
            switch (path_is_media_type(path))
            {
               case RARCH_CONTENT_MOVIE:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
                  if (settings->bools.multimedia_builtin_mediaplayer_enable)
                     file_type = FILE_TYPE_MOVIE;
#endif
                  break;
               case RARCH_CONTENT_IMAGE:
#ifdef HAVE_IMAGEVIEWER
                  if (settings->bools.multimedia_builtin_imageviewer_enable
                        && type != DISPLAYLIST_IMAGES)
                     file_type = FILE_TYPE_IMAGEVIEWER;
                  else
                     file_type = FILE_TYPE_IMAGE;
#endif
                  if (filebrowser_types == FILEBROWSER_SELECT_IMAGE)
                     file_type = FILE_TYPE_IMAGE;
                  break;
               default:
                  break;
            }
         }

         switch (file_type)
         {
            case FILE_TYPE_PLAIN:
#if 0
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE;
#endif
               files_count++;
               break;
            case FILE_TYPE_MOVIE:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN;
               files_count++;
               break;
            case FILE_TYPE_MUSIC:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN;
               files_count++;
               break;
            case FILE_TYPE_IMAGE:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_IMAGE;
               files_count++;
               break;
            case FILE_TYPE_IMAGEVIEWER:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER;
               files_count++;
               break;
            case FILE_TYPE_DIRECTORY:
               enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
               dirs_count++;
               break;
            default:
               break;
         }

         items_found++;
         menu_entries_append_enum(info->list, path, label,
               enum_idx,
               file_type, 0, 0);
      }
   }

   if (str_list && str_list->size > 0)
      string_list_free(str_list);

   if (items_found == 0)
   {
      menu_entries_append_enum(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ITEMS),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ITEMS),
            MENU_ENUM_LABEL_NO_ITEMS,
            MENU_SETTING_NO_ITEM, 0, 0);
   }

end:
   if (info && !path_is_compressed)
      menu_entries_prepend(info->list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY),
            path,
            MENU_ENUM_LABEL_PARENT_DIRECTORY,
            FILE_TYPE_PARENT_DIRECTORY, 0, 0);
}
