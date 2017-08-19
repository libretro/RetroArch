/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <file/file_path.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../file_path_special.h"

#ifndef BIND_ACTION_LABEL
#define BIND_ACTION_LABEL(cbs, name) \
   cbs->action_label = name; \
   cbs->action_label_ident = #name;
#endif

static int action_bind_label_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

#define fill_label_macro(func, lbl) \
static int (func)(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 0; \
}

fill_label_macro(action_bind_label_information,              MENU_ENUM_LABEL_VALUE_INFORMATION)
fill_label_macro(action_bind_label_internal_memory,          MSG_INTERNAL_STORAGE)
fill_label_macro(action_bind_label_removable_storage,        MSG_REMOVABLE_STORAGE)
fill_label_macro(action_bind_label_external_application_dir, MSG_EXTERNAL_APPLICATION_DIR)
fill_label_macro(action_bind_label_application_dir,          MSG_APPLICATION_DIR)

static int action_bind_label_playlist_collection_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   if (strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
   {
      char path_base[PATH_MAX_LENGTH];
      path_base[0] = '\0';

      fill_short_pathname_representation_noext(path_base, path,
            sizeof(path_base));

      strlcpy(s, path_base, len);
   }
   return 0;
}

int menu_cbs_init_bind_label(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_LABEL(cbs, action_bind_label_generic);

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY:
            BIND_ACTION_LABEL(cbs, action_bind_label_playlist_collection_entry);
            break;
         case MSG_INTERNAL_STORAGE:
            BIND_ACTION_LABEL(cbs, action_bind_label_internal_memory);
            break;
         case MSG_REMOVABLE_STORAGE:
            BIND_ACTION_LABEL(cbs, action_bind_label_removable_storage);
            break;
         case MSG_APPLICATION_DIR:
            BIND_ACTION_LABEL(cbs, action_bind_label_application_dir);
            break;
         case MSG_EXTERNAL_APPLICATION_DIR:
            BIND_ACTION_LABEL(cbs, action_bind_label_external_application_dir);
            break;
         case MENU_ENUM_LABEL_INFORMATION:
            BIND_ACTION_LABEL(cbs, action_bind_label_information);
            break;
         default:
            break;
      }
   }

   return -1;
}
