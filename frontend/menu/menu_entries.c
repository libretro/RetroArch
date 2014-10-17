/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "menu_entries.h"
#include "menu_action.h"
#include "menu_list.h"
#include "../../settings_data.h"
#include "../../performance.h"

void menu_entries_refresh(file_list_t *list)
{
   if (!list || !driver.menu)
      return;

   (void)list;

   /* Before a refresh, we could have deleted a file on disk, causing
    * selection_ptr to suddendly be out of range.
    * Ensure it doesn't overflow. */

   if (driver.menu->selection_ptr >= menu_list_get_size()
         && menu_list_get_size())
      menu_navigation_set(driver.menu, menu_list_get_size() - 1);
   else if (!menu_list_get_size())
      menu_navigation_clear(driver.menu, true);
}

static inline bool entries_list_elem_is_dir(file_list_t *buf,
      unsigned offset)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   menu_list_get_at_offset(buf, offset, &path, &label, &type);

   return type != MENU_FILE_PLAIN;
}

static inline int entries_list_get_first_char(file_list_t *buf,
      unsigned offset)
{
   int ret;
   const char *path = NULL;

   menu_list_get_alt_at_offset(buf, offset, &path);
   ret = tolower(*path);

   /* "Normalize" non-alphabetical entries so they 
    * are lumped together for purposes of jumping. */
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

void menu_entries_build_scroll_indices(file_list_t *list)
{
   size_t i;
   int current;
   bool current_is_dir;

   if (!driver.menu || !list)
      return;

   driver.menu->scroll_indices_size = 0;
   if (!list->size)
      return;

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 0;

   current        = entries_list_get_first_char(list, 0);
   current_is_dir = entries_list_elem_is_dir(list, 0);

   for (i = 1; i < list->size; i++)
   {
      int first   = entries_list_get_first_char(list, i);
      bool is_dir = entries_list_elem_is_dir(list, i);

      if ((current_is_dir && !is_dir) || (first > current))
         driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 
      list->size - 1;
}

int setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   if (setting->flags & SD_FLAG_ALLOW_INPUT)
      return MENU_FILE_LINEFEED;
   if (setting->flags & SD_FLAG_PUSH_ACTION)
      return MENU_FILE_PUSH;
   if (setting->flags & SD_FLAG_IS_DRIVER)
      return MENU_FILE_DRIVER;
   if (setting->type == ST_PATH)
      return MENU_FILE_PATH;
   if (setting->flags & SD_FLAG_IS_CATEGORY)
      return MENU_FILE_CATEGORY;
   return 0;
}

static int entries_push_main_menu_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned menu_type)
{
   settings_list_free(menu->list_mainmenu);
   menu->list_mainmenu = (rarch_setting_t *)setting_data_new(SL_FLAG_MAIN_MENU);
   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(menu->list_mainmenu,
         label);

   if (!setting)
      return -1;

   menu_list_clear(list);

   for (; setting->type != ST_END_GROUP; setting++)
   {
      if (
            setting->type == ST_GROUP ||
            setting->type == ST_SUB_GROUP ||
            setting->type == ST_END_SUB_GROUP
         )
         continue;

      menu_list_push(list, setting->short_description,
            setting->name, setting_set_flags(setting), 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, path, label, menu_type);

   return 0;
}

int menu_entries_parse_list(file_list_t *list, file_list_t *menu_list,
      const char *dir, const char *label, unsigned type,
      unsigned default_type_plain, const char *exts)
{
   size_t i, list_size;
   struct string_list *str_list = NULL;

   menu_list_clear(list);

   if (!*dir)
   {
#if defined(GEKKO)
#ifdef HW_RVL
      menu_list_push(list,
            "sd:/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "usb:/", "", MENU_FILE_DIRECTORY, 0);
#endif
      menu_list_push(list,
            "carda:/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "cardb:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX1)
      menu_list_push(list,
            "C:", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "D:", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "E:", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "F:", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "G:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX360)
      menu_list_push(list,
            "game:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_WIN32)
      unsigned drives = GetLogicalDrives();
      char drive[] = " :\\";
      for (i = 0; i < 32; i++)
      {
         drive[0] = 'A' + i;
         if (drives & (1 << i))
            menu_list_push(list,
                  drive, "", MENU_FILE_DIRECTORY, 0);
      }
#elif defined(__CELLOS_LV2__)
      menu_list_push(list,
            "/app_home/",   "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_hdd0/",   "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_hdd1/",   "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/host_root/",  "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb000/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb001/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb002/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb003/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb004/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb005/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "/dev_usb006/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(PSP)
      menu_list_push(list,
            "ms0:/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "ef0:/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            "host0:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(IOS)
      menu_list_push(list,
            "/var/mobile/", "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list,
            g_defaults.core_dir, "", MENU_FILE_DIRECTORY, 0);
      menu_list_push(list, "/", "",
            MENU_FILE_DIRECTORY, 0);
#else
      menu_list_push(list, "/", "",
            MENU_FILE_DIRECTORY, 0);
#endif
      return 0;
   }
#if defined(GEKKO) && defined(HW_RVL)
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(dir);

   if (dev != -1 && !gx_devices[dev].mounted &&
         gx_devices[dev].interface->isInserted())
      fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   bool path_is_compressed = path_is_compressed_file(dir);
   bool push_dir = menu_common_type_is(label, type) == MENU_FILE_DIRECTORY;

   if (path_is_compressed)
      str_list = compressed_file_list_new(dir,exts);
   else
      str_list = dir_list_new(dir, exts, true);

   if (!str_list)
      return -1;

   dir_list_sort(str_list, true);

   if (push_dir)
      menu_list_push(list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   list_size = str_list->size;
   for (i = 0; i < str_list->size; i++)
   {
      menu_file_type_t file_type = MENU_FILE_NONE;
      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = MENU_FILE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = MENU_FILE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = MENU_FILE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            if (!strcmp(label, "detect_core_list"))
            {
               if (path_is_compressed_file(str_list->elems[i].data))
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  file_type = MENU_FILE_CARCHIVE;
                  break;
               }
            }
            file_type = (menu_file_type_t)default_type_plain;
            break;
      }
      bool is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;


      /* Need to preserve slash first time. */
      const char *path = str_list->elems[i].data;

      if (*dir && !path_is_compressed)
         path = path_basename(path);


#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
      if (!strcmp(label, "core_list") && (is_dir ||
               strcasecmp(path, SALAMANDER_FILE) == 0))
         continue;
#endif
#endif

      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      if (!strcmp(label, "core_list"))
      {
         /* Compressed cores are unsupported */
         if (file_type == MENU_FILE_CARCHIVE)
            continue;

         menu_list_push(list, path, "",
               is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE, 0);
      }
      else
      menu_list_push(list, path, "",
            file_type, 0);
   }

   string_list_free(str_list);

   if (!strcmp(label, "core_list"))
   {
      menu_list_get_last_stack(menu_list, &dir, NULL, NULL);
      list_size = file_list_get_size(list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX], display_name[PATH_MAX];
         const char *path = NULL;

         menu_list_get_at_offset(list, i, &path, NULL, &type);
         if (type != MENU_FILE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (g_extern.core_info &&
               core_info_list_get_display_name(g_extern.core_info,
                  core_path, display_name, sizeof(display_name)))
            menu_list_set_alt_at_offset(list, i, display_name);
      }
      menu_list_sort_on_alt(list);
   }

   driver.menu->scroll_indices_size = 0;
   menu_entries_build_scroll_indices(list);
   menu_entries_refresh(list);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, dir, label, type);

   return 0;
}

int menu_entries_deferred_push(file_list_t *list, file_list_t *menu_list)
{
   unsigned type = 0;

   const char *path = NULL;
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = NULL;

   menu_list_get_last_stack(menu_list, &path, &label, &type);

   if (!strcmp(label, "Main Menu"))
      return entries_push_main_menu_list(driver.menu, list, path, label, type);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(menu_list);

   if (cbs->action_deferred_push)
      return cbs->action_deferred_push(list, menu_list, path, label, type);

   return 0;
}

bool menu_entries_init(menu_handle_t *menu)
{
   if (!menu)
      return false;

   menu->list_mainmenu = setting_data_new(SL_FLAG_MAIN_MENU);
   menu->list_settings = setting_data_new(SL_FLAG_ALL_SETTINGS);

   menu_list_push_stack(menu->menu_list, "", "Main Menu", MENU_SETTINGS, 0);
   menu_navigation_clear(menu, true);
   entries_push_main_menu_list(menu, menu->menu_list->selection_buf,
         "", "Main Menu", 0);

   return true;
}
