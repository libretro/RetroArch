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

#include <string.h>

#include <string/stdstring.h>

#include "menu_list.h"

#include "../menu_driver.h"

struct menu_list
{
   file_list_t **menu_stack;
   size_t menu_stack_size;
   file_list_t **selection_buf;
   size_t selection_buf_size;
};

void menu_list_free_list(file_list_t *list)
{
   unsigned i;

   for (i = 0; i < list->size; i++)
   {
      menu_ctx_list_t list_info;

      list_info.list      = list;
      list_info.idx       = i;
      list_info.list_size = list->size;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_FREE, &list_info);
   }

   file_list_free(list);
}

void menu_list_free(menu_list_t *menu_list)
{
   unsigned i;
   if (!menu_list)
      return;

   for (i = 0; i < menu_list->menu_stack_size; i++)
   {
      if (!menu_list->menu_stack[i])
         continue;

      menu_list_free_list(menu_list->menu_stack[i]);
      menu_list->menu_stack[i]    = NULL;
   }
   for (i = 0; i < menu_list->selection_buf_size; i++)
   {
      if (!menu_list->selection_buf[i])
         continue;

      menu_list_free_list(menu_list->selection_buf[i]);
      menu_list->selection_buf[i] = NULL;
   }

   free(menu_list->menu_stack);
   free(menu_list->selection_buf);

   free(menu_list);
}

menu_list_t *menu_list_new(void)
{
   unsigned i;
   menu_list_t           *list = (menu_list_t*)calloc(1, sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack          = (file_list_t**)calloc(1, sizeof(*list->menu_stack));

   if (!list->menu_stack)
      goto error;

   list->selection_buf       = (file_list_t**)calloc(1, sizeof(*list->selection_buf));

   if (!list->selection_buf)
      goto error;

   list->menu_stack_size      = 1;
   list->selection_buf_size   = 1;

   for (i = 0; i < list->menu_stack_size; i++)
      list->menu_stack[i]    = (file_list_t*)calloc(1, sizeof(*list->menu_stack[i]));

   for (i = 0; i < list->selection_buf_size; i++)
      list->selection_buf[i] = (file_list_t*)calloc(1, sizeof(*list->selection_buf[i]));

   return list;

error:
   menu_list_free(list);
   return NULL;
}

file_list_t *menu_list_get(menu_list_t *list, unsigned idx)
{
   if (!list)
      return NULL;
   return list->menu_stack[idx];
}

file_list_t *menu_list_get_selection(menu_list_t *list, unsigned idx)
{
   if (!list)
      return NULL;
   return list->selection_buf[idx];
}

size_t menu_list_get_stack_size(menu_list_t *list, size_t idx)
{
   if (!list)
      return 0;
   return file_list_get_size(list->menu_stack[idx]);
}

static int menu_list_flush_stack_type(const char *needle, const char *label,
      unsigned type, unsigned final_type)
{
   return needle ? !string_is_equal(needle, label) : (type != final_type);
}

void menu_list_flush_stack(menu_list_t *list,
      size_t idx, const char *needle, unsigned final_type)
{
   bool refresh           = false;
   const char *path       = NULL;
   const char *label      = NULL;
   unsigned type          = 0;
   size_t entry_idx       = 0;
   file_list_t *menu_list = menu_list_get(list, (unsigned)idx);
   if (!list)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_entries_get_last(menu_list,
         &path, &label, &type, &entry_idx);

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      size_t new_selection_ptr = menu_navigation_get_selection();

      if (!menu_list_pop_stack(list, idx, &new_selection_ptr, 1))
         break;

      menu_navigation_set_selection(new_selection_ptr);

      menu_list = menu_list_get(list, (unsigned)idx);

      menu_entries_get_last(menu_list,
            &path, &label, &type, &entry_idx);
   }
}

bool menu_list_pop_stack(menu_list_t *list,
      size_t idx, size_t *directory_ptr, bool animate)
{
   menu_ctx_list_t list_info;
   bool refresh           = false;
   file_list_t *menu_list = menu_list_get(list, (unsigned)idx);
   if (!list)
      return false;

   if (menu_list_get_stack_size(list, idx) <= 1)
      return false;

   list_info.type   = MENU_LIST_PLAIN;
   list_info.action = 0;

   if (animate)
      menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

   if (menu_list->size != 0)
   {
      menu_ctx_list_t list_info;

      list_info.list      = menu_list;
      list_info.idx       = menu_list->size - 1;
      list_info.list_size = menu_list->size - 1;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_FREE, &list_info);
   }

   file_list_pop(menu_list, directory_ptr);
   menu_driver_ctl(RARCH_MENU_CTL_LIST_SET_SELECTION, menu_list);
   if (animate)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   return true;
}
