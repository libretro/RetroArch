/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include <string.h>

#include <retro_inline.h>

#include "menu.h"
#include "menu_cbs.h"
#include "menu_list.h"

size_t menu_list_get_size(menu_list_t *list)
{
   if (!list)
      return 0;
   return file_list_get_size(list->selection_buf);
}

static void menu_list_free_list(file_list_t *list)
{
   unsigned i;

   for (i = 0; i < list->size; i++)
      menu_driver_list_free(list, i, list->size);

   if (list)
      file_list_free(list);
}

void menu_list_free(menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   menu_list_free_list(menu_list->menu_stack);
   menu_list_free_list(menu_list->selection_buf);

   menu_list->menu_stack    = NULL;
   menu_list->selection_buf = NULL;

   free(menu_list);
}

menu_list_t *menu_list_new(void)
{
   menu_list_t *list = (menu_list_t*)calloc(1, sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack    = (file_list_t*)calloc(1, sizeof(file_list_t));
   list->selection_buf = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!list->menu_stack || !list->selection_buf)
      goto error;

   return list;

error:
   menu_list_free(list);
   return NULL;
}

size_t menu_list_get_stack_size(menu_list_t *list)
{
   if (!list)
      return 0;
   return file_list_get_size(list->menu_stack);
}

void menu_list_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type,
      size_t *entry_idx)
{
   file_list_get_at_offset(list, idx, path, label, file_type, entry_idx);
}

void menu_list_get_last(const file_list_t *list,
      const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx)
{
   if (list)
      file_list_get_last(list, path, label, file_type, entry_idx);
}

void menu_list_get_last_stack(const menu_list_t *list,
      const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx)
{
   menu_list_get_last(list->menu_stack, path, label, file_type, entry_idx);
}

void *menu_list_get_userdata_at_offset(const file_list_t *list, size_t idx)
{
   if (!list)
      return NULL;
   return (menu_file_list_cbs_t*)file_list_get_userdata_at_offset(list, idx);
}

menu_file_list_cbs_t *menu_list_get_actiondata_at_offset(
      const file_list_t *list, size_t idx)
{
   if (!list)
      return NULL;
   return (menu_file_list_cbs_t*)
      file_list_get_actiondata_at_offset(list, idx);
}

menu_file_list_cbs_t *menu_list_get_last_stack_actiondata(const menu_list_t *list)
{
   if (!list)
      return NULL;
   return (menu_file_list_cbs_t*)file_list_get_last_actiondata(list->menu_stack);
}

static INLINE int menu_list_flush_stack_type(
      const char *needle, const char *label,
      unsigned type, unsigned final_type)
{
   return needle ? strcmp(needle, label) : (type != final_type);
}

static void menu_list_pop(file_list_t *list, size_t *directory_ptr)
{
   if (list->size != 0)
      menu_driver_list_free(list, list->size - 1, list->size - 1);

   file_list_pop(list, directory_ptr);

   menu_driver_list_set_selection(list);
}

void menu_list_flush_stack(menu_list_t *list,
      const char *needle, unsigned final_type)
{
   const char *path       = NULL;
   const char *label      = NULL;
   unsigned type          = 0;
   size_t entry_idx       = 0;
   if (!list)
      return;

   menu_entries_set_refresh(false);
   menu_list_get_last(list->menu_stack,
         &path, &label, &type, &entry_idx);

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      size_t new_selection_ptr;

      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &new_selection_ptr);

      if (menu_list_pop_stack(list, &new_selection_ptr))
      {
         menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &new_selection_ptr);

         menu_list_get_last(list->menu_stack,
               &path, &label, &type, &entry_idx);
      }
   }
}

bool menu_list_pop_stack(menu_list_t *list, size_t *directory_ptr)
{
   if (!list)
      return false;

   if (menu_list_get_stack_size(list) <= 1)
      return false;

   menu_driver_list_cache(MENU_LIST_PLAIN, 0);

   menu_list_pop(list->menu_stack, directory_ptr);
   menu_entries_set_refresh(false);

   return true;
}


void menu_list_clear(file_list_t *list)
{
   unsigned i;
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();

   if (driver->list_clear)
      driver->list_clear(list);

   for (i = 0; i < list->size; i++)
      file_list_free_actiondata(list, i);

   if (list)
      file_list_clear(list);
}

void menu_list_push(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx)
{
   size_t idx;
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_push(list, path, label, type, directory_ptr, entry_idx);

   idx = list->size - 1;

   if (driver->list_insert)
      driver->list_insert(list, path, label, idx);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);
   menu_cbs_init(list, path, label, type, idx);
}

void menu_list_set_alt_at_offset(file_list_t *list, size_t idx,
      const char *alt)
{
   file_list_set_alt_at_offset(list, idx, alt);
}

void menu_list_get_alt_at_offset(const file_list_t *list, size_t idx,
      const char **alt)
{
   file_list_get_alt_at_offset(list, idx, alt);
}

/**
 * menu_list_elem_is_dir:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Is the current entry at offset @offset a directory?
 *
 * Returns: true (1) if entry is a directory, otherwise false (0).
 **/
static bool menu_list_elem_is_dir(file_list_t *list,
      unsigned offset)
{
   unsigned type     = 0;

   menu_list_get_at_offset(list, offset, NULL, NULL, &type, NULL);

   return type == MENU_FILE_DIRECTORY;
}

/**
 * menu_list_elem_get_first_char:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Gets the first character of an element in the
 * file list.
 *
 * Returns: first character of element in file list.
 **/
static int menu_list_elem_get_first_char(
      file_list_t *list, unsigned offset)
{
   int ret;
   const char *path = NULL;

   menu_list_get_alt_at_offset(list, offset, &path);
   ret = tolower((int)*path);

   /* "Normalize" non-alphabetical entries so they
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static void menu_list_build_scroll_indices(file_list_t *list)
{
   int current;
   bool current_is_dir;
   size_t i, scroll_value   = 0;

   if (!list || !list->size)
      return;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES, NULL);
   menu_navigation_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &scroll_value);

   current        = menu_list_elem_get_first_char(list, 0);
   current_is_dir = menu_list_elem_is_dir(list, 0);

   for (i = 1; i < list->size; i++)
   {
      int first   = menu_list_elem_get_first_char(list, i);
      bool is_dir = menu_list_elem_is_dir(list, i);

      if ((current_is_dir && !is_dir) || (first > current))
         menu_navigation_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &i);

      current        = first;
      current_is_dir = is_dir;
   }


   scroll_value = list->size - 1;
   menu_navigation_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &scroll_value);
}

/**
 * Before a refresh, we could have deleted a
 * file on disk, causing selection_ptr to
 * suddendly be out of range.
 *
 * Ensure it doesn't overflow.
 **/
void menu_list_refresh(file_list_t *list)
{
   size_t list_size, selection;
   menu_list_t   *menu_list = menu_list_get_ptr();
   if (!menu_list || !list)
      return;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   menu_list_build_scroll_indices(list);

   list_size = menu_list_get_size(menu_list);

   if ((selection >= list_size) && list_size)
   {
      size_t idx  = list_size - 1;
      bool scroll = true;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }
   else if (!list_size)
   {
      bool pending_push = true;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }
}
