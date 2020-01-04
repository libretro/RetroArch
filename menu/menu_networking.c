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

#include <stdint.h>

#include <retro_miscellaneous.h>
#include <lists/file_list.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_NETWORKING
#include <net/net_http.h>
#endif

#include "menu_driver.h"
#include "menu_networking.h"
#include "menu_cbs.h"
#include "menu_entries.h"

#include "../msg_hash.h"
#include "../tasks/task_file_transfer.h"
#include "../tasks/tasks_internal.h"

unsigned print_buf_lines(file_list_t *list, char *buf,
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

      str_list         = string_split(line_start, " ");

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

      string_list_free(str_list);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start     = buf + i + 1;
   }

   if (append)
      file_list_sort_on_alt(list);
   /* If the buffer was completely full, and didn't end
    * with a newline, just ignore the partial last line. */

   return count;
}

void cb_net_generic_subdir(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_NETWORKING
   char subdir_path[PATH_MAX_LENGTH];
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   file_transfer_t *state       = (file_transfer_t*)user_data;

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

void cb_net_generic(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_NETWORKING
   bool refresh                   = false;
   http_transfer_data_t *data     = (http_transfer_data_t*)task_data;
   file_transfer_t *state         = (file_transfer_t*)user_data;
   menu_handle_t            *menu = menu_driver_get_ptr();

   if (!menu)
      goto finish;

   if (menu->core_buf)
      free(menu->core_buf);

   menu->core_buf = NULL;
   menu->core_len = 0;

   if (!data || err)
      goto finish;

   menu->core_buf = (char*)malloc((data->len+1) * sizeof(char));

   if (!menu->core_buf)
      goto finish;

   if (!string_is_empty(data->data))
      memcpy(menu->core_buf, data->data, data->len * sizeof(char));
   menu->core_buf[data->len] = '\0';
   menu->core_len            = data->len;

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
      char *parent_dir_encoded         = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      file_transfer_t *transf     = NULL;

      parent_dir[0]         = '\0';
      parent_dir_encoded[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, PATH_MAX_LENGTH * sizeof(char));
      strlcat(parent_dir,
            file_path_str(FILE_PATH_INDEX_DIRS_URL),
            PATH_MAX_LENGTH * sizeof(char));

      transf           = (file_transfer_t*)malloc(sizeof(*transf));

      transf->enum_idx = MSG_UNKNOWN;
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      net_http_urlencode_full(parent_dir_encoded, parent_dir, PATH_MAX_LENGTH * sizeof(char));
      task_push_http_transfer(parent_dir_encoded, true,
            "index_dirs", cb_net_generic_subdir, transf);

      free(parent_dir);
      free(parent_dir_encoded);
   }

   if (state)
      free(state);
#endif
}
