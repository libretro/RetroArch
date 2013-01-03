/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef FILEBROWSER_H_
#define FILEBROWSER_H_

#include <stdint.h>
#include <stdlib.h>
#include "../../general.h"

typedef struct
{
   unsigned char directory_stack_size;
   char directory_path[PATH_MAX]; 
   struct {
      struct string_list *list;
      size_t ptr;
   } current_dir;
   char root_dir[PATH_MAX];
   char extensions[PATH_MAX];
} filebrowser_t;

typedef enum
{
   FILEBROWSER_ACTION_UP,
   FILEBROWSER_ACTION_DOWN,
   FILEBROWSER_ACTION_LEFT,
   FILEBROWSER_ACTION_RIGHT,
   FILEBROWSER_ACTION_OK,
   FILEBROWSER_ACTION_CANCEL,
   FILEBROWSER_ACTION_SCROLL_UP,
   FILEBROWSER_ACTION_SCROLL_DOWN,
   FILEBROWSER_ACTION_RESET,
   FILEBROWSER_ACTION_NOOP
} filebrowser_action_t;

const char * filebrowser_get_current_dir (void *data);
const char * filebrowser_get_current_path (void *data);
bool filebrowser_get_current_path_isdir (void *data);
size_t filebrowser_get_current_index (void *data);
void filebrowser_set_root(void *data, const char *root_dir);
void filebrowser_set_root_and_ext(void *data, const char *ext, const char *root_dir);
void filebrowser_free(void *data);
void filebrowser_set_current_at (void *dat, size_t pos);
bool filebrowser_iterate(void *data, unsigned action);

#endif /* FILEBROWSER_H_ */
