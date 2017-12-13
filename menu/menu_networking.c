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

#include <stdint.h>

#include <retro_miscellaneous.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_NETWORKING
#include <net/net_http_parse.h>
#endif

#include "menu_cbs.h"
#include "menu_entries.h"
#include "widgets/menu_list.h"

#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../tasks/tasks_internal.h"

char *core_buf                   = NULL;
size_t core_len                  = 0;

void cb_net_generic_subdir(void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_NETWORKING
   char subdir_path[PATH_MAX_LENGTH];
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state       = (menu_file_transfer_t*)user_data;

   subdir_path[0] = '\0';

   if (!data || err)
      goto finish;

   if (!string_is_empty(data->data))
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
#endif
}

void cb_net_generic(void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_NETWORKING
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

   if (!string_is_empty(data->data))
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
      char *parent_dir                 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      menu_file_transfer_t *transf     = NULL;

      parent_dir[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, PATH_MAX_LENGTH * sizeof(char));
      strlcat(parent_dir,
            file_path_str(FILE_PATH_INDEX_DIRS_URL),
            PATH_MAX_LENGTH * sizeof(char));

      transf           = (menu_file_transfer_t*)malloc(sizeof(*transf));

      transf->enum_idx = MSG_UNKNOWN;
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      task_push_http_transfer(parent_dir, true,
            "index_dirs", cb_net_generic_subdir, transf);

      free(parent_dir);
   }

   if (state)
      free(state);
#endif
}
