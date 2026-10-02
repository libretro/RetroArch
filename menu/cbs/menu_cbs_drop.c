/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Daniel De Matteis
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

#ifndef BIND_ACTION_DROP
#define BIND_ACTION_DROP(cbs, name) (cbs)->action_drop = (name)
#endif

static int action_drop_load_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      const struct string_list *payload)
{
   menu_handle_t *menu = menu_state_get_ptr()->driver_data;
   const char *file    = payload->elems[0].data;

   if (     !menu
         || !path_is_valid(file)
         || path_is_directory(file))
      return -1;

   fill_pathname_basedir(menu->scratch2_buf, file,
         sizeof(menu->scratch2_buf));
   strlcpy(menu->scratch_buf, path_basename(file),
         sizeof(menu->scratch_buf));

   return action_ok_load_archive_detect_core(path, label, type,
         idx, entry_idx);
}

int menu_cbs_init_bind_drop(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   if (cbs->enum_idx == MENU_ENUM_LABEL_LOAD_CONTENT_LIST)
   {
      BIND_ACTION_DROP(cbs, action_drop_load_content);
      return 0;
   }

   BIND_ACTION_DROP(cbs, NULL);

   return -1;
}
