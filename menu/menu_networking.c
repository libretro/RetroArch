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
#include <lists/string_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <streams/file_stream.h>
#include <net/net_http.h>
#include <string/stdstring.h>

#ifdef HAVE_NETWORKING
#include <net/net_http_parse.h>
#endif

#include "menu_cbs.h"
#include "menu_entries.h"
#include "widgets/menu_list.h"

#include "../core_info.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../tasks/tasks_internal.h"

static char http_buf[PATH_MAX_LENGTH]   = {0};

static char *core_buf                   = NULL;
static size_t core_len                  = 0;

void parse_index_lines(file_list_t *list, char *buf,
      const char *label, enum msg_file_type type, bool append, bool extended)
{
   char c;
   int i, j = 0;
   char *line_start;
   bool newline = false;
   bool end = false;

   if (buf)
      strlcpy(core_buf, buf, core_len);

   buf = core_buf;
   line_start = buf;

   if (!buf || !core_len)
   {
      menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY),
            msg_hash_to_str(MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY),
            MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY,
            FILE_TYPE_NONE, 0, 0);
      return;
   }

   for (i = 0; i < core_len; i++)
   {
      size_t ln, name_len;
      const char *core_date        = NULL;
      const char *core_crc         = NULL;
      const char *core_pathname    = NULL;
      char *link = NULL;
      char *name = NULL;
      int len;

      if (*(buf + i) == '\n')
      {
         /* Found a line ending, print the line and compute new line_start */
         newline = true;

         /* Save the next char  */
         c = *(buf + i + 1);
         /* replace with \0 */
         *(buf + i + 1) = '\0';

         /* We need to strip the newline. */
         ln = strlen(line_start) - 1;

         if (line_start[ln] == '\n')
            line_start[ln] = '\0';
      }
      else if (*(buf + i + 1) == '\0')
      {
         /* Found end of buffer, but there may still be data left to read */
         newline = false;
      }
      else
         continue;

      while (!string_is_empty(line_start))
      {
         link = (char*)calloc(1, ln + 1);
         name = (char*)calloc(1, ln + 1);

         len = string_parse_html_anchor(line_start, link, name, ln + 1, ln + 1);

         core_pathname = link;

         (void)core_date;
         (void)core_crc;

         if (string_is_empty(core_pathname))
           goto next;

         name_len = strlen(name);

         if (name[name_len - 1] == '/')
            name[name_len - 1] = '\0';

         if (!string_is_empty(name))
         {
            if (string_is_equal(name, ".") ||
                string_is_equal(name, ".."))
               goto next;
         }

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
               menu_entries_append_enum(list, core_pathname, label,
                     MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
            else
               menu_entries_prepend(list, core_pathname, label,
                     MENU_ENUM_LABEL_URL_ENTRY, type, 0, 0);
         }

         file_list_set_alt_at_offset(list, j, name);

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
                           path_basename(core_pathname),
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
                              filestream_exists(core_path)
                           && core_info_get_display_name(
                              core_path, display_name, sizeof(display_name)))
                        file_list_set_alt_at_offset(list, j, display_name);
                  }
               }
               break;
            case FILE_TYPE_NONE:
            default:
               break;
         }

         j++;

         /* advance past the link we parsed and keep looking for more */
         if (len > 0)
         {
            line_start += len;
            free(link);
            free(name);
            link = NULL;
            name = NULL;
         }
      }
next:
      if (link)
         free(link);
      if (name)
         free(name);

      /* The end of the buffer, we're done */
      if (end)
         break;

      if (!newline && *(buf + i) == '\0')
      {
         end = true;
      }

      if (newline && !end)
      {
         /* Restore the saved char */
         *(buf + i + 1) = c;
         line_start     = buf + i + 1;
      }
   }

   if (append)
      file_list_sort_on_alt(list);
}

void cb_net_generic_subdir(void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_NETWORKING
   char subdir_path[PATH_MAX_LENGTH];
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state       = (menu_file_transfer_t*)user_data;
   size_t buf_size;

   subdir_path[0] = '\0';

   if (!data || data->len == 0 || err)
      goto finish;

   buf_size = MIN(sizeof(subdir_path), data->len * sizeof(char));

   if (!string_is_empty(data->data))
      memcpy(subdir_path, data->data, buf_size - 1);
   subdir_path[buf_size - 1] = '\0';

finish:
   if (!err && !strstr(subdir_path, ".index-dirs"))
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

   if (!err)
   {
      char *parent_dir                 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      parent_dir[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, PATH_MAX_LENGTH * sizeof(char));
   }

   /* no need to make a second request */
#if 0
   if (!err && !strstr(state->path, ".index-dirs"))
   {
      char *parent_dir                 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      menu_file_transfer_t *transf     = NULL;

      parent_dir[0] = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, PATH_MAX_LENGTH * sizeof(char));
      /*strlcat(parent_dir,
            ".index-dirs",
            PATH_MAX_LENGTH * sizeof(char));*/

      transf           = (menu_file_transfer_t*)malloc(sizeof(*transf));

      transf->enum_idx = MSG_UNKNOWN;
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      task_push_http_transfer(parent_dir, true,
            "index_dirs", cb_net_generic_subdir, transf);

      free(parent_dir);
   }
#endif

   if (state)
      free(state);
#endif
}

void menu_networking_push_http_request(
      bool suppress_msg,
      const char *str,
      const char *enum_str,
      const char *path,
      enum msg_hash_enums enum_idx,
      retro_task_callback_t cb
      )
{
   menu_file_transfer_t *transf = (menu_file_transfer_t*)
      calloc(1, sizeof(*transf));

   if (!transf)
      return;

   transf->enum_idx = enum_idx;

   if (!string_is_empty(path))
      strlcpy(transf->path, path, sizeof(transf->path));

   strlcpy(http_buf, str, sizeof(http_buf));

   task_push_http_transfer(str, suppress_msg, enum_str, cb, transf);
}
