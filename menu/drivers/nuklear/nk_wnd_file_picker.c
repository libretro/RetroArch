/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
 *  Copyright (C) 2016      - Andrés Suárez
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

#include "nk_menu.h"

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>

#include "../../menu_driver.h"
#include "../../menu_hash.h"
#include "../../frontend/frontend_driver.h"

static bool assets_loaded;
static char path[PATH_MAX_LENGTH];

static file_list_t *drives = NULL;
struct string_list *files  = NULL;

struct icon_list {
    struct nk_image disk;
    struct nk_image folder;
    struct nk_image file;
};
struct icon_list icons;

void load_icons(nk_menu_handle_t *nk)
{
   char buf[PATH_MAX_LENGTH];
   fill_pathname_join(buf, nk->assets_directory,
         "harddisk.png", sizeof(buf));
   icons.disk = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory,
         "folder.png", sizeof(buf));
   icons.folder = nk_common_image_load(buf);
   fill_pathname_join(buf, nk->assets_directory,
         "file.png", sizeof(buf));
   icons.file = nk_common_image_load(buf);

   assets_loaded = true;
}

void nk_wnd_file_picker(nk_menu_handle_t *nk)
{
   struct nk_panel layout;
   struct nk_context *ctx = &nk->ctx;
   const int id           = NK_WND_FILE_PICKER;
   settings_t *settings   = config_get_ptr();

   int i = 0;
   char buf[PATH_MAX_LENGTH];

   if (!drives)
   {
      drives = (file_list_t*)calloc(1, sizeof(file_list_t));
      frontend_driver_parse_drive_list(drives);
      /* RARCH_LOG ("Drives: %s\n",drives->list[0].path); */
   }

   if (!assets_loaded)
      load_icons(nk);

   if (nk_begin(ctx, &layout, "Select File", nk_rect(440, 10, 330, 400),
         NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_MOVABLE|
         NK_WINDOW_BORDER))
   {
      nk_layout_row_dynamic(ctx, 30, 3);
      for (i = 0; i < drives->size; i++)
      {
         if(nk_button_image_label(ctx, icons.disk, drives->list[i].path, 
            NK_TEXT_CENTERED, NK_BUTTON_DEFAULT))
         {
            fill_pathname_join(path, drives->list[i].path,
                  "", sizeof(path));
            /* RARCH_LOG("Current Path: %s\n", path); */
            files = dir_list_new(path, NULL, true, true);
         }
      }
      nk_layout_row_dynamic(ctx, 30, 1);
      if (files)
      for (i = 0; i < files->size; i++)
      {
         /* RARCH_LOG ("Drives: %s\n",files->elems[i].data); */
         if (nk_button_image_label(ctx, path_is_directory(files->elems[i].data) ? 
            icons.folder : icons.file, path_basename(files->elems[i].data), 
            NK_TEXT_RIGHT, NK_BUTTON_DEFAULT))
         {
            fill_pathname_join(path, files->elems[i].data,
                  "", sizeof(path));
            if (path_is_directory (path))
               files = dir_list_new(path, NULL, true, true);
            else
               RARCH_LOG ("File: %s selected\n", path);
         }
      }

   }
   /* save position and size to restore after context reset */
   nk_wnd_set_state(nk, id, nk_window_get_position(ctx), nk_window_get_size(ctx));
   nk_end(ctx);
}