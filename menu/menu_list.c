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

#include "../driver.h"
#include "menu_common_list.h"
#include "menu_list.h"
#include "menu_navigation.h"
#include "menu_entries.h"
#include <string.h>

/**
 * Before a refresh, we could have deleted a 
 * file on disk, causing selection_ptr to 
 * suddendly be out of range.
 *
 * Ensure it doesn't overflow.
 **/
static void menu_entries_refresh(menu_handle_t *menu, file_list_t *list)
{
   if (!list)
      return;

   if (menu->selection_ptr >= menu_list_get_size(menu->menu_list)
         && menu_list_get_size(menu->menu_list))
      menu_navigation_set(menu, menu_list_get_size(menu->menu_list) - 1, true);
   else if (!menu_list_get_size(menu->menu_list))
      menu_navigation_clear(menu, true);
}

/**
 * menu_entries_list_elem_is_dir:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Is the current entry at offset @offset a directory?
 *
 * Returns: true (1) if entry is a directory, otherwise false (0).
 **/
static inline bool menu_entries_list_elem_is_dir(file_list_t *list,
      unsigned offset)
{
   const char *path  = NULL;
   const char *label = NULL;
   unsigned type     = 0;

   menu_list_get_at_offset(list, offset, &path, &label, &type);

   return type != MENU_FILE_PLAIN;
}

/**
 * menu_entries_list_get_first_char:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Gets the first character of an element in the
 * file list.
 *
 * Returns: first character of element in file list.
 **/
static inline int menu_entries_list_get_first_char(
      file_list_t *list, unsigned offset)
{
   int ret;
   const char *path = NULL;

   menu_list_get_alt_at_offset(list, offset, &path);
   ret = tolower(*path);

   /* "Normalize" non-alphabetical entries so they 
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static void menu_entries_build_scroll_indices(file_list_t *list)
{
   size_t i;
   int current;
   bool current_is_dir;

   if (!driver.menu || !list)
      return;

   driver.menu->scroll.indices.size = 0;
   if (!list->size)
      return;

   driver.menu->scroll.indices.list[driver.menu->scroll.indices.size++] = 0;

   current        = menu_entries_list_get_first_char(list, 0);
   current_is_dir = menu_entries_list_elem_is_dir(list, 0);

   for (i = 1; i < list->size; i++)
   {
      int first   = menu_entries_list_get_first_char(list, i);
      bool is_dir = menu_entries_list_elem_is_dir(list, i);

      if ((current_is_dir && !is_dir) || (first > current))
         driver.menu->scroll.indices.list[driver.menu->scroll.indices.size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   driver.menu->scroll.indices.list[driver.menu->scroll.indices.size++] = 
      list->size - 1;
}

void menu_list_destroy(file_list_t *list)
{
   unsigned i;

   if (!list)
      return;

   if (!driver.menu_ctx)
      goto end;

   for (i = 0; i < list->size; i++)
   {
      if (driver.menu_ctx->list_delete)
         driver.menu_ctx->list_delete(list, i, list->size);
      menu_common_list_delete(list, i, list->size);
   }

end:
   file_list_free(list);
}

void menu_list_free(menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   menu_list_destroy(menu_list->menu_stack);
   menu_list_destroy(menu_list->selection_buf);
}

void *menu_list_new(void)
{
   menu_list_t *list = (menu_list_t*)calloc(1, sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack    = (file_list_t*)calloc(1, sizeof(file_list_t));
   list->selection_buf = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!list->menu_stack || !list->selection_buf)
   {
      free(list);
      return NULL;
   }

   return list;
}

size_t menu_list_get_stack_size(menu_list_t *list)
{
   if (!list)
      return 0;
   return file_list_get_size(list->menu_stack);
}

void menu_list_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type)
{
   file_list_get_at_offset(list, idx, path, label, file_type);
}

size_t menu_list_get_size(menu_list_t *list)
{
   if (!list)
      return 0;
   return file_list_get_size(list->selection_buf);
}

void menu_list_get_last(const file_list_t *list,
      const char **path, const char **label,
      unsigned *file_type)
{
   if (list)
      file_list_get_last(list, path, label, file_type);
}

void menu_list_get_last_stack(const menu_list_t *list,
      const char **path, const char **label,
      unsigned *file_type)
{
   if (list)
      file_list_get_last(list->menu_stack, path, label, file_type);
}

void *menu_list_get_actiondata_at_offset(const file_list_t *list, size_t idx)
{
   if (!list)
      return NULL;
   return file_list_get_actiondata_at_offset(list, idx);
}

void *menu_list_get_last_stack_actiondata(const menu_list_t *list)
{
   if (!list)
      return NULL;
   return file_list_get_last_actiondata(list->menu_stack);
}

void menu_list_flush_stack(menu_list_t *list,
      unsigned final_type)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu || !list)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list->menu_stack, &path, &label, &type);

   while (type != final_type)
   {
      menu_list_pop(list->menu_stack, &driver.menu->selection_ptr);
      file_list_get_last(list->menu_stack, &path, &label, &type);
   }
}

void menu_list_flush_stack_by_needle(menu_list_t *list,
      const char *needle)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu || !list)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list->menu_stack, &path, &label, &type);

   while (strcmp(needle, label) != 0)
   {
      menu_list_pop(list->menu_stack, &driver.menu->selection_ptr);
      file_list_get_last(list->menu_stack, &path, &label, &type);
   }
}

void menu_list_pop_stack(menu_list_t *list)
{
   if (!list)
      return;

   if (file_list_get_size(list->menu_stack) <= 1)
      return;

   if (driver.menu_ctx->list_cache)
      driver.menu_ctx->list_cache(false, 0);

   menu_list_pop(list->menu_stack, &driver.menu->selection_ptr);
   driver.menu->need_refresh = true;
}

void menu_list_pop_stack_by_needle(menu_list_t *list,
      const char *needle)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu || !list)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list->menu_stack, &path, &label, &type);

   while (strcmp(needle, label) == 0)
   {
      menu_list_pop(list->menu_stack, &driver.menu->selection_ptr);
      file_list_get_last(list->menu_stack, &path, &label, &type);
   }
}

void menu_list_pop(file_list_t *list, size_t *directory_ptr)
{
   if (!driver.menu_ctx)
      goto end;

   if (list->size != 0)
   {
      size_t list_size = list->size - 1;

      if (driver.menu_ctx->list_delete)
         driver.menu_ctx->list_delete(list, list_size, list_size);
      menu_common_list_delete(list, list_size, list_size);
   }

end:
   file_list_pop(list, directory_ptr);

   if (!driver.menu_ctx)
      return;

   if (driver.menu_ctx->list_set_selection)
      driver.menu_ctx->list_set_selection(list);

   menu_common_list_set_selection(list);
}

void menu_list_clear(file_list_t *list)
{
   if (!driver.menu_ctx)
      goto end;

   if (driver.menu_ctx->list_clear)
      driver.menu_ctx->list_clear(list);

end:
   menu_common_list_clear(list);
}

static void menu_list_insert(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!driver.menu_ctx)
      return;

   if (driver.menu_ctx->list_insert)
      driver.menu_ctx->list_insert(list, path, label, list->size - 1);

   menu_common_list_insert(list, path, label, type, list->size - 1);
}

void menu_list_push(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!list || !label)
      return;

   file_list_push(list, path, label, type, directory_ptr);
   menu_list_insert(list, path, label, type, directory_ptr);
}

void menu_list_push_refresh(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!list)
      return;

   menu_list_push(list, path, label, type, directory_ptr);
   menu_navigation_clear(driver.menu, true);
   driver.menu->need_refresh = true;
}

void menu_list_push_stack(menu_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (list)
      menu_list_push(list->menu_stack, path, label, type, directory_ptr);
}

int menu_list_push_stack_refresh(menu_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!list)
      return -1;

   if (driver.menu_ctx->list_cache)
      driver.menu_ctx->list_cache(false, 0);

   menu_list_push_stack(list, path, label, type, directory_ptr);
   menu_navigation_clear(driver.menu, true);
   driver.menu->need_refresh = true;

   return 0;
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

void menu_list_sort_on_alt(file_list_t *list)
{
   file_list_sort_on_alt(list);
}

int menu_list_populate_generic(void *data,
      file_list_t *list, const char *path,
      const char *label, unsigned type)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return -1;

   driver.menu->scroll.indices.size = 0;
   menu_entries_build_scroll_indices(list);
   menu_entries_refresh(menu, list);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(path, label, type);

   return 0;
}
