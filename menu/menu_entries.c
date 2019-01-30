/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <retro_inline.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#include "menu_driver.h"
#include "menu_cbs.h"

#include "../core.h"
#include "../retroarch.h"
#include "../version.h"

/* Flagged when menu entries need to be refreshed */
static bool menu_entries_need_refresh              = false;
static bool menu_entries_nonblocking_refresh       = false;
static size_t menu_entries_begin                   = 0;
static rarch_setting_t *menu_entries_list_settings = NULL;
static menu_list_t *menu_entries_list              = NULL;

struct menu_list
{
   size_t menu_stack_size;
   size_t selection_buf_size;
   file_list_t **menu_stack;
   file_list_t **selection_buf;
};

static void menu_list_free_list(file_list_t *list)
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

static void menu_list_free(menu_list_t *menu_list)
{
   if (!menu_list)
      return;

   if (menu_list->menu_stack)
   {
      unsigned i;

      for (i = 0; i < menu_list->menu_stack_size; i++)
      {
         if (!menu_list->menu_stack[i])
            continue;

         menu_list_free_list(menu_list->menu_stack[i]);
         menu_list->menu_stack[i]    = NULL;
      }

      free(menu_list->menu_stack);
   }

   if (menu_list->selection_buf)
   {
      unsigned i;

      for (i = 0; i < menu_list->selection_buf_size; i++)
      {
         if (!menu_list->selection_buf[i])
            continue;

         menu_list_free_list(menu_list->selection_buf[i]);
         menu_list->selection_buf[i] = NULL;
      }

      free(menu_list->selection_buf);
   }

   free(menu_list);
}

static menu_list_t *menu_list_new(void)
{
   unsigned i;
   menu_list_t           *list = (menu_list_t*)malloc(sizeof(*list));

   if (!list)
      return NULL;

   list->menu_stack_size       = 1;
   list->selection_buf_size    = 1;
   list->selection_buf         = NULL;
   list->menu_stack            = (file_list_t**)
      calloc(list->menu_stack_size, sizeof(*list->menu_stack));

   if (!list->menu_stack)
      goto error;

   list->selection_buf         = (file_list_t**)
      calloc(list->selection_buf_size, sizeof(*list->selection_buf));

   if (!list->selection_buf)
      goto error;

   for (i = 0; i < list->menu_stack_size; i++)
      list->menu_stack[i]      = (file_list_t*)
         calloc(1, sizeof(*list->menu_stack[i]));

   for (i = 0; i < list->selection_buf_size; i++)
      list->selection_buf[i]   = (file_list_t*)
         calloc(1, sizeof(*list->selection_buf[i]));

   return list;

error:
   menu_list_free(list);
   return NULL;
}

static file_list_t *menu_list_get(menu_list_t *list, unsigned idx)
{
   if (!list)
      return NULL;
   return list->menu_stack[idx];
}

static file_list_t *menu_list_get_selection(menu_list_t *list, unsigned idx)
{
   if (!list)
      return NULL;
   return list->selection_buf[idx];
}

static size_t menu_list_get_stack_size(menu_list_t *list, size_t idx)
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

static bool menu_list_pop_stack(menu_list_t *list,
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
      menu_driver_list_cache(&list_info);

   if (menu_list->size != 0)
   {
      menu_ctx_list_t list_info;

      list_info.list      = menu_list;
      list_info.idx       = menu_list->size - 1;
      list_info.list_size = menu_list->size - 1;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_FREE, &list_info);
   }

   file_list_pop(menu_list, directory_ptr);
   menu_driver_list_set_selection(menu_list);
   if (animate)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   return true;
}

static void menu_list_flush_stack(menu_list_t *list,
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
   file_list_get_last(menu_list,
         &path, &label, &type, &entry_idx);

   while (menu_list_flush_stack_type(
            needle, label, type, final_type) != 0)
   {
      size_t new_selection_ptr = menu_navigation_get_selection();

      if (!menu_list_pop_stack(list, idx, &new_selection_ptr, 1))
         break;

      menu_navigation_set_selection(new_selection_ptr);

      menu_list = menu_list_get(list, (unsigned)idx);

      file_list_get_last(menu_list,
            &path, &label, &type, &entry_idx);
   }
}

void menu_entries_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type,
      size_t *entry_idx, const char **alt)
{
   file_list_get_at_offset(list, idx, path, label, file_type, entry_idx);
   file_list_get_alt_at_offset(list, idx, alt);
}

static bool menu_entries_clear(file_list_t *list)
{
   unsigned i;
   if (!list)
      return false;

   menu_driver_list_clear(list);

   for (i = 0; i < list->size; i++)
      file_list_free_actiondata(list, i);

   if (list)
      file_list_clear(list);

   return true;
}

/**
 * menu_entries_elem_is_dir:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Is the current entry at offset @offset a directory?
 *
 * Returns: true (1) if entry is a directory, otherwise false (0).
 **/
static bool menu_entries_elem_is_dir(file_list_t *list,
      unsigned offset)
{
   unsigned type     = 0;

   menu_entries_get_at_offset(list, offset, NULL, NULL, &type, NULL, NULL);

   return type == FILE_TYPE_DIRECTORY;
}

/**
 * menu_entries_elem_get_first_char:
 * @list                     : File list handle.
 * @offset                   : Offset index of element.
 *
 * Gets the first character of an element in the
 * file list.
 *
 * Returns: first character of element in file list.
 **/
static int menu_entries_elem_get_first_char(
      file_list_t *list, unsigned offset)
{
   int ret          = 0;
   const char *path = NULL;

   menu_entries_get_at_offset(list, offset,
         NULL, NULL, NULL, NULL, &path);

   if (path != NULL)
      ret = tolower((int)*path);

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
   int current;
   bool current_is_dir;
   size_t i, scroll_value   = 0;

   if (!list || !list->size)
      return;

   menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR_SCROLL_INDICES, NULL);
   menu_driver_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &scroll_value);

   current        = menu_entries_elem_get_first_char(list, 0);
   current_is_dir = menu_entries_elem_is_dir(list, 0);

   for (i = 1; i < list->size; i++)
   {
      int first   = menu_entries_elem_get_first_char(list, (unsigned)i);
      bool is_dir = menu_entries_elem_is_dir(list, (unsigned)i);

      if ((current_is_dir && !is_dir) || (first > current))
         menu_driver_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &i);

      current        = first;
      current_is_dir = is_dir;
   }

   scroll_value = list->size - 1;
   menu_driver_ctl(MENU_NAVIGATION_CTL_ADD_SCROLL_INDEX, &scroll_value);
}

/**
 * Before a refresh, we could have deleted a
 * file on disk, causing selection_ptr to
 * suddendly be out of range.
 *
 * Ensure it doesn't overflow.
 **/
static bool menu_entries_refresh(void *data)
{
   size_t list_size;
   file_list_t *list = (file_list_t*)data;
   size_t selection  = menu_navigation_get_selection();

   menu_entries_build_scroll_indices(list);

   list_size = menu_entries_get_size();

   if ((selection >= list_size) && list_size)
   {
      size_t idx  = list_size - 1;
      menu_navigation_set_selection(idx);
      menu_driver_navigation_set(true);
   }
   else if (!list_size)
   {
      bool pending_push = true;
      menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }

   return true;
}

/* Sets title to what the name of the current menu should be. */
int menu_entries_get_title(char *s, size_t len)
{
   unsigned menu_type            = 0;
   const char *path              = NULL;
   const char *label             = NULL;
   enum msg_hash_enums enum_idx  = MSG_UNKNOWN;
   menu_file_list_cbs_t *cbs = menu_entries_get_last_stack_actiondata();

   if (!cbs)
      return -1;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   if (cbs && cbs->action_get_title)
      return cbs->action_get_title(path, label, menu_type, s, len);
   return 0;
}

int menu_entries_get_core_name(char *s, size_t len)
{
   struct retro_system_info    *system = runloop_get_libretro_system_info();
   const char *core_name               = NULL;

   if (system)
      core_name    = system->library_name;

   if (string_is_empty(core_name) && system)
      core_name = system->library_name;
   if (string_is_empty(core_name))
      core_name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);

   snprintf(s, len, "%s", core_name);

   return 0;
}

/* Sets 's' to the name of the current core
 * (shown at the top of the UI). */
int menu_entries_get_core_title(char *s, size_t len)
{
   struct retro_system_info    *system = runloop_get_libretro_system_info();
   const char *core_name               = system ? system->library_name    : NULL;
   const char *core_version            = system ? system->library_version : NULL;
#if _MSC_VER == 1200
   const char *extra_version = " msvc6";
#elif _MSC_VER == 1300
   const char *extra_version = " msvc2002";
#elif _MSC_VER == 1310
   const char *extra_version = " msvc2003";
#elif _MSC_VER == 1400
   const char *extra_version = " msvc2005";
#elif _MSC_VER == 1500
   const char *extra_version = " msvc2008";
#elif _MSC_VER == 1600
   const char *extra_version = " msvc2010";
#elif _MSC_VER == 1700
   const char *extra_version = " msvc2012";
#elif _MSC_VER == 1800
   const char *extra_version = " msvc2013";
#elif _MSC_VER == 1900
   const char *extra_version = " msvc2015";
#elif _MSC_VER >= 1910 && _MSC_VER < 2000
   const char *extra_version = " msvc2017";
#else
   const char *extra_version = "";
#endif

   if (string_is_empty(core_name))
      core_name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);
   if (!core_version)
      core_version = "";

   snprintf(s, len, "%s%s - %s %s", PACKAGE_VERSION, extra_version,
         core_name, core_version);

   return 0;
}

file_list_t *menu_entries_get_menu_stack_ptr(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return NULL;
   return menu_list_get(menu_list, (unsigned)idx);
}

file_list_t *menu_entries_get_selection_buf_ptr(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return NULL;
   return menu_list_get_selection(menu_list, (unsigned)idx);
}

static bool menu_entries_init(void)
{
   if (!menu_entries_ctl(MENU_ENTRIES_CTL_LIST_INIT, NULL))
      goto error;

   if (!menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_INIT, NULL))
      goto error;

   return true;

error:
   menu_entries_ctl(MENU_ENTRIES_CTL_LIST_DEINIT, NULL);
   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_DEINIT, NULL);

   return false;
}

void menu_entries_set_checked(file_list_t *list, size_t entry_idx,
      bool checked)
{
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(list, entry_idx);

   if (cbs)
      cbs->checked = checked;
}

void menu_entries_append(file_list_t *list, const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_append(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx                = list->size - 1;

   list_info.list     = list;
   list_info.path     = path;
   list_info.fullpath = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);

   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = MSG_UNKNOWN;
   cbs->setting  = menu_setting_find(label);

   menu_cbs_init(list, cbs, path, label, type, idx);
}

void menu_entries_append_enum(file_list_t *list, const char *path,
      const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_append(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx                   = list->size - 1;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = enum_idx;

   if (enum_idx != MENU_ENUM_LABEL_PLAYLIST_ENTRY
       && enum_idx != MENU_ENUM_LABEL_PLAYLIST_COLLECTION_ENTRY
       && enum_idx != MENU_ENUM_LABEL_RDB_ENTRY) {
      cbs->setting  = menu_setting_find_enum(enum_idx);
   }

   menu_cbs_init(list, cbs, path, label, type, idx);
}

void menu_entries_prepend(file_list_t *list, const char *path, const char *label,
      enum msg_hash_enums enum_idx,
      unsigned type, size_t directory_ptr, size_t entry_idx)
{
   menu_ctx_list_t list_info;
   size_t idx;
   const char *menu_path           = NULL;
   menu_file_list_cbs_t *cbs       = NULL;
   if (!list || !label)
      return;

   file_list_prepend(list, path, label, type, directory_ptr, entry_idx);

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

   idx              = 0;

   list_info.fullpath    = NULL;

   if (!string_is_empty(menu_path))
      list_info.fullpath = strdup(menu_path);
   list_info.list        = list;
   list_info.path        = path;
   list_info.label       = label;
   list_info.idx         = idx;
   list_info.entry_type  = type;

   menu_driver_list_insert(&list_info);

   if (list_info.fullpath)
      free(list_info.fullpath);

   file_list_free_actiondata(list, idx);
   cbs = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!cbs)
      return;

   file_list_set_actiondata(list, idx, cbs);

   cbs->enum_idx = enum_idx;
   cbs->setting  = menu_setting_find_enum(cbs->enum_idx);

   menu_cbs_init(list, cbs, path, label, type, idx);
}

menu_file_list_cbs_t *menu_entries_get_last_stack_actiondata(void)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return NULL;
   return (menu_file_list_cbs_t*)file_list_get_last_actiondata(
         menu_list_get(menu_list, 0));
}

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, enum msg_hash_enums *enum_idx, size_t *entry_idx)
{
   menu_file_list_cbs_t *cbs      = NULL;
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return;

   file_list_get_last(menu_list_get(menu_list, 0),
         path, label, file_type, entry_idx);
   cbs = menu_entries_get_last_stack_actiondata();
   if (cbs && enum_idx)
      *enum_idx = cbs->enum_idx;
}

void menu_entries_flush_stack(const char *needle, unsigned final_type)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (menu_list)
      menu_list_flush_stack(menu_list, 0, needle, final_type);
}

void menu_entries_pop_stack(size_t *ptr, size_t idx, bool animate)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (menu_list)
      menu_list_pop_stack(menu_list, idx, ptr, animate);
}

size_t menu_entries_get_stack_size(size_t idx)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return 0;
   return menu_list_get_stack_size(menu_list, idx);
}

size_t menu_entries_get_size(void)
{
   menu_list_t *menu_list         = menu_entries_list;
   if (!menu_list)
      return 0;
   return file_list_get_size(menu_list_get_selection(menu_list, 0));
}

rarch_setting_t *menu_entries_get_setting(uint32_t i)
{
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf, i) : NULL;

   if (!cbs)
      return NULL;
   return cbs->setting;
}

bool menu_entries_ctl(enum menu_entries_ctl_state state, void *data)
{
   switch (state)
   {
      case MENU_ENTRIES_CTL_DEINIT:
         menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_DEINIT, NULL);
         menu_entries_ctl(MENU_ENTRIES_CTL_LIST_DEINIT, NULL);

         menu_entries_need_refresh        = false;
         menu_entries_nonblocking_refresh = false;
         menu_entries_begin               = 0;
         break;
      case MENU_ENTRIES_CTL_NEEDS_REFRESH:
         if (menu_entries_nonblocking_refresh)
            return false;
         if (!menu_entries_need_refresh)
            return false;
         break;
      case MENU_ENTRIES_CTL_LIST_GET:
         {
            menu_list_t **list = (menu_list_t**)data;
            if (!list)
               return false;
            *list = menu_entries_list;
         }
         return true;
      case MENU_ENTRIES_CTL_LIST_DEINIT:
         if (menu_entries_list)
            menu_list_free(menu_entries_list);
         menu_entries_list     = NULL;
         break;
      case MENU_ENTRIES_CTL_LIST_INIT:
         if (!(menu_entries_list = (menu_list_t*)menu_list_new()))
            return false;
         break;
      case MENU_ENTRIES_CTL_SETTINGS_GET:
         {
            rarch_setting_t **settings = (rarch_setting_t**)data;
            if (!settings)
               return false;
            *settings = menu_entries_list_settings;
         }
         break;
      case MENU_ENTRIES_CTL_SETTINGS_DEINIT:
         menu_setting_free(menu_entries_list_settings);
         if (menu_entries_list_settings)
            free(menu_entries_list_settings);
         menu_entries_list_settings = NULL;
         break;
      case MENU_ENTRIES_CTL_SETTINGS_INIT:
         menu_setting_ctl(MENU_SETTING_CTL_NEW, &menu_entries_list_settings);

         if (!menu_entries_list_settings)
            return false;
         break;
      case MENU_ENTRIES_CTL_SET_REFRESH:
         {
            bool *nonblocking = (bool*)data;

            if (*nonblocking)
               menu_entries_nonblocking_refresh = true;
            else
               menu_entries_need_refresh        = true;
         }
         break;
      case MENU_ENTRIES_CTL_UNSET_REFRESH:
         {
            bool *nonblocking = (bool*)data;

            if (*nonblocking)
               menu_entries_nonblocking_refresh = false;
            else
               menu_entries_need_refresh        = false;
         }
         break;
      case MENU_ENTRIES_CTL_SET_START:
         {
            size_t *idx = (size_t*)data;
            if (idx)
               menu_entries_begin = *idx;
         }
      case MENU_ENTRIES_CTL_START_GET:
         {
            size_t *idx = (size_t*)data;
            if (!idx)
               return 0;

            *idx = menu_entries_begin;
         }
         break;
      case MENU_ENTRIES_CTL_REFRESH:
         if (!data)
            return false;
         return menu_entries_refresh(data);
      case MENU_ENTRIES_CTL_CLEAR:
         return menu_entries_clear((file_list_t*)data);
      case MENU_ENTRIES_CTL_INIT:
         return menu_entries_init();
      case MENU_ENTRIES_CTL_SHOW_BACK:
         /* Returns true if a Back button should be shown
          * (i.e. we are at least
          * one level deep in the menu hierarchy). */
         return (menu_entries_get_stack_size(0) > 1);
      case MENU_ENTRIES_CTL_NONE:
      default:
         break;
   }

   return true;
}
