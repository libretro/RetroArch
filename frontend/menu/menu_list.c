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
#include "menu_navigation.h"
#include <string.h>

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
      if (driver.menu_ctx->backend->list_delete)
         driver.menu_ctx->backend->list_delete(list, i, list->size);
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
   list->menu_stack_old = (file_list_t*)calloc(1, sizeof(file_list_t));
   list->selection_buf_old = (file_list_t*)calloc(1, sizeof(file_list_t));

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
   if (list)
      return file_list_get_actiondata_at_offset(list, idx);
   return NULL;
}

void *menu_list_get_last_stack_actiondata(const menu_list_t *list)
{
   if (list)
      return file_list_get_last_actiondata(list->menu_stack);
   return NULL;
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

   if (file_list_get_size(list->menu_stack) > 1)
   {
      menu_list_pop(list->menu_stack, &driver.menu->selection_ptr);
      driver.menu->need_refresh = true;
   }
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
   if (!list)
      return;
   menu_list_push(list->menu_stack, path, label, type, directory_ptr);
}

void menu_list_push_stack_refresh(menu_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr)
{
   if (!list)
      return;
   menu_list_push_stack(list, path, label, type, directory_ptr);
   menu_navigation_clear(driver.menu, true);
   driver.menu->need_refresh = true;
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
