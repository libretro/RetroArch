/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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
#include <stdlib.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <formats/image.h>
#include "menu_media_browser.h"
#include "../configuration.h"
#include "../defaults.h"
#include "../playlist.h"

struct menu_media_queue
{
   char **names;
   char *directory;
   size_t count, position;
};

static char *menu_media_name_copy(const char *name)
{
   size_t size = strlen(name) + 1;
   char *copy = (char *)malloc(size);
   if (copy)
      memcpy(copy, name, size);
   return copy;
}

static int menu_media_name_compare(const void *left, const void *right)
{
   const char *a = *(const char *const *)left;
   const char *b = *(const char *const *)right;
   const unsigned char *x = (const unsigned char *)a;
   const unsigned char *y = (const unsigned char *)b;
   while (*x && *y)
   {
      if (*x >= '0' && *x <= '9' && *y >= '0' && *y <= '9')
      {
         const unsigned char *xe, *ye;
         size_t nx, ny;
         int order;
         while (*x == '0')
            x++;
         while (*y == '0')
            y++;
         xe = x;
         ye = y;
         while (*xe >= '0' && *xe <= '9')
            xe++;
         while (*ye >= '0' && *ye <= '9')
            ye++;
         nx = (size_t)(xe - x);
         ny = (size_t)(ye - y);
         if (nx != ny)
            return nx < ny ? -1 : 1;
         order = memcmp(x, y, nx);
         if (order)
            return order;
         x = xe;
         y = ye;
      }
      else
      {
         unsigned cx = *x++, cy = *y++;
         if (cx >= 'A' && cx <= 'Z')
            cx += 'a' - 'A';
         if (cy >= 'A' && cy <= 'Z')
            cy += 'a' - 'A';
         if (cx != cy)
            return cx < cy ? -1 : 1;
      }
   }
   if (*x != *y)
      return *x ? 1 : -1;
   return strcmp(a, b);
}

static bool menu_media_movie(const struct item_file *item)
{
   enum image_type_enum type;
   if (!item->path || item->type == FILE_TYPE_DIRECTORY)
      return false;
   type = image_texture_get_type(item->path);
   return type == IMAGE_TYPE_MP4 || type == IMAGE_TYPE_WEBM;
}

void menu_media_queue_free(menu_media_queue_t *queue)
{
   size_t i;
   if (!queue)
      return;
   for (i = 0; i < queue->count; i++)
      free(queue->names[i]);
   free(queue->names);
   free(queue->directory);
   free(queue);
}

menu_media_queue_t *menu_media_queue_new(struct menu_state *menu_st)
{
   menu_media_queue_t *queue;
   file_list_t *list, *stack;
   const char *selected, *directory;
   size_t i;
   if (!menu_st || !menu_st->entries.list)
      return NULL;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!list || !stack || !stack->size || menu_st->selection_ptr >= list->size
         || !menu_media_movie(&list->list[menu_st->selection_ptr]))
      return NULL;
   selected = list->list[menu_st->selection_ptr].path;
   directory = stack->list[stack->size - 1].path;
   if (!directory || !*directory || list->size > (size_t)-1 / sizeof(char *))
      return NULL;
   queue = (menu_media_queue_t *)calloc(1, sizeof(*queue));
   if (!queue)
      return NULL;
   queue->names = (char **)calloc(list->size, sizeof(char *));
   queue->directory = menu_media_name_copy(directory);
   if (!queue->names || !queue->directory)
      goto failed;
   for (i = 0; i < list->size; i++)
      if (menu_media_movie(&list->list[i]))
      {
         const char *name = list->list[i].path;
         char *copy = menu_media_name_copy(name);
         if (!copy)
            goto failed;
         queue->names[queue->count++] = copy;
      }
   qsort(queue->names, queue->count, sizeof(char *), menu_media_name_compare);
   for (i = 0; i < queue->count; i++)
      if (string_is_equal(selected, queue->names[i]))
      {
         queue->position = i;
         return queue;
      }
failed:
   menu_media_queue_free(queue);
   return NULL;
}

unsigned menu_media_queue_directions(const menu_media_queue_t *queue)
{
   if (!queue)
      return 0;
   return (queue->position ? 1u : 0u)
      | (queue->position + 1 < queue->count ? 2u : 0u);
}

bool menu_media_queue_step(
      menu_media_queue_t *queue, struct menu_state *menu_st, int direction)
{
   file_list_t *list, *stack;
   size_t next, i;
   if (!queue || !menu_st || !menu_st->entries.list || !direction)
      return false;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   stack = MENU_LIST_GET(menu_st->entries.list, 0);
   if (!list || !stack || !stack->size || menu_st->selection_ptr >= list->size
         || !string_is_equal(
               stack->list[stack->size - 1].path, queue->directory)
         || !string_is_equal(list->list[menu_st->selection_ptr].path,
               queue->names[queue->position]))
      return false;
   if (direction < 0)
   {
      if (!queue->position)
         return false;
      next = queue->position - 1;
   }
   else
   {
      next = queue->position + 1;
      if (next >= queue->count)
         return false;
   }
   for (i = 0; i < list->size; i++)
      if (menu_media_movie(&list->list[i])
            && string_is_equal(list->list[i].path, queue->names[next]))
      {
         queue->position = next;
         menu_st->selection_ptr = i;
         if (menu_st->driver_ctx && menu_st->driver_ctx->navigation_set)
            menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
         return true;
      }
   return false;
}

bool menu_media_selected_is_viewable(struct menu_state *menu_st)
{
   file_list_t *list;
   unsigned type;
   if (!menu_st || !menu_st->entries.list)
      return false;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   if (!list || menu_st->selection_ptr >= list->size)
      return false;
   type = list->list[menu_st->selection_ptr].type;
   return type != FILE_TYPE_DIRECTORY && type != FILE_TYPE_IMAGE;
}

const char *menu_media_selected_history_image(struct menu_state *menu_st)
{
#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_MENU)
   const struct playlist_entry *entry = NULL;
   playlist_t *playlist = playlist_get_cached();
   const struct item_file *item;
   file_list_t *list;
   enum image_type_enum type;
   if (!menu_st || !menu_st->entries.list || !playlist
         || !g_defaults.image_history
         || !config_get_ptr()->bools.multimedia_builtin_imageviewer_enable)
      return NULL;
   list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
   if (!list || menu_st->selection_ptr >= list->size)
      return NULL;
   item = &list->list[menu_st->selection_ptr];
   if (item->type != FILE_TYPE_RPL_ENTRY
         || !string_is_equal(playlist_get_conf_path(playlist),
            playlist_get_conf_path(g_defaults.image_history)))
      return NULL;
   playlist_get_index(playlist, item->entry_idx, &entry);
   if (!entry || !entry->path || !*entry->path)
      return NULL;
   type = image_texture_get_type(entry->path);
   if (type == IMAGE_TYPE_NONE || type == IMAGE_TYPE_DDS
         || type == IMAGE_TYPE_MP4 || type == IMAGE_TYPE_WEBM
         || strstr(entry->path, "://")
         || path_contains_compressed_file(entry->path))
      return NULL;
   return entry->path;
#else
   return NULL;
#endif
}

bool menu_media_selected_in_history(struct menu_state *menu_st)
{
#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_MENU)
   const char *path = menu_media_selected_history_image(menu_st);
   return path && path_is_valid(path);
#else
   return false;
#endif
}

bool menu_media_open_selected_file(struct menu_state *menu_st,
      gfx_thumbnail_t *primary, gfx_thumbnail_t *secondary)
{
#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_MENU)
   char history_dir[DIR_MAX_LENGTH];
#endif
   file_list_t *list, *stack;
   const char *name, *directory;
   enum image_type_enum type;
   gfx_thumbnail_path_data_t *paths = menu_st ? menu_st->thumbnail_path_data : NULL;
   if (!menu_st || !menu_st->entries.list || !paths || !primary || !secondary) return false;
#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_MENU)
   if ((name = menu_media_selected_history_image(menu_st)))
   {
      fill_pathname_basedir(history_dir, name, sizeof(history_dir));
      directory = history_dir;
      name = path_basename(name);
   }
   else
#endif
   {
      list = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
      stack = MENU_LIST_GET(menu_st->entries.list, 0);
      if (!list || menu_st->selection_ptr >= list->size || !stack || !stack->size) return false;
      if (!menu_media_selected_is_viewable(menu_st)) return false;
      name = list->list[menu_st->selection_ptr].path;
      directory = stack->list[stack->size - 1].path;
   }
   type = image_texture_get_type(name);
   if (type == IMAGE_TYPE_NONE || type == IMAGE_TYPE_DDS) return false;
   if (!gfx_thumbnail_set_content_image(paths, directory, name)) return false;
   if (gfx_thumbnail_has_file(primary, paths->content_path))
      gfx_thumbnail_reset(secondary);
   else if (gfx_thumbnail_has_file(secondary, paths->content_path))
      gfx_thumbnail_reset(primary);
   else
   {
      gfx_thumbnail_reset(primary);
      gfx_thumbnail_reset(secondary);
      gfx_thumbnail_request_file(paths->content_path, primary,
            config_get_ptr()->uints.gfx_thumbnail_upscale_threshold);
   }
   return true;
}
