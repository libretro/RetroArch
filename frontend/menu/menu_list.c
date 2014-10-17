/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../../driver.h"
#include "menu_list.h"
#include <string.h>

void menu_list_free(file_list_t *list)
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
      if (driver.menu_ctx->backend->list_delete)
         driver.menu_ctx->backend->list_delete(list, i, list->size);
   }

end:
   file_list_free(list);
}

void menu_list_pop_stack(file_list_t *list)
{
   if (!list)
      return;

   if (file_list_get_size(list) > 1)
   {
      menu_list_pop(list, &driver.menu->selection_ptr);
      driver.menu->need_refresh = true;
   }
}

void menu_list_pop_stack_by_needle(file_list_t *list,
      const char *needle)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu || !list)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(list, &path, &label, &type);
   while (strcmp(needle, label) == 0)
   {
      menu_list_pop(list, &driver.menu->selection_ptr);
      file_list_get_last(list, &path, &label, &type);
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
      if (driver.menu_ctx->backend->list_delete)
         driver.menu_ctx->backend->list_delete(list, list_size, list_size);
   }

end:
   file_list_pop(list, directory_ptr);

   if (!driver.menu_ctx)
      return;

   if (driver.menu_ctx->list_set_selection)
      driver.menu_ctx->list_set_selection(list);

   if (driver.menu_ctx->backend->list_set_selection)
      driver.menu_ctx->backend->list_set_selection(list);
}

void menu_list_clear(file_list_t *list)
{
   if (!driver.menu_ctx)
      goto end;

   if (driver.menu_ctx->list_clear)
      driver.menu_ctx->list_clear(list);

   if (driver.menu_ctx->backend->list_clear)
      driver.menu_ctx->backend->list_clear(list);

end:

   file_list_clear(list);
}


void menu_list_push(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!list)
      return;
   file_list_push(list, path, label, type, directory_ptr);

   if (!driver.menu_ctx)
      return;

   if (driver.menu_ctx->list_insert)
      driver.menu_ctx->list_insert(list, path, label, list->size - 1);

   if (driver.menu_ctx->backend->list_insert)
      driver.menu_ctx->backend->list_insert(list, path,
            label, type, list->size - 1);
}

void menu_list_push_stack(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   menu_list_push(list, path, label, type, directory_ptr);
}
