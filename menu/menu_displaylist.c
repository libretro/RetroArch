/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stddef.h>

#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

#include "menu.h"
#include "menu_display.h"
#include "menu_entries.h"
#include "menu_displaylist.h"
#include "menu_navigation.h"
#include "../performance.h"

static void menu_displaylist_push_perfcounter(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(info->list,
               counters[i]->ident, "", id + i, 0);
}

static int menu_displaylist_push_perfcounter_generic(
      menu_displaylist_info_t *info,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned ident)
{
   menu_list_clear(info->list);
   menu_displaylist_push_perfcounter(info, counters, num, ident);

   menu_driver_populate_entries(
         info->path, info->label, info->type);

   return 0;
}

/**
 * menu_displaylist_parse_drive_list:
 * @list                     : File list handle.
 *
 * Generates default directory drive list.
 * Platform-specific.
 *
 **/
static void menu_displaylist_parse_drive_list(file_list_t *list)
{
   size_t i = 0;

   (void)i;

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
#elif defined(_3DS)
   menu_list_push(list,
         "sdmc:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(IOS)
   menu_list_push(list,
         "/var/mobile/Documents/", "", MENU_FILE_DIRECTORY, 0);
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
}

static int menu_displaylist_parse(
      file_list_t *list, file_list_t *menu_list,
      const char *dir, const char *label, unsigned type,
      unsigned default_type_plain, const char *exts,
      rarch_setting_t *setting)
{
   size_t i, list_size;
   bool path_is_compressed, push_dir;
   int                   device = 0;
   struct string_list *str_list = NULL;
   settings_t *settings         = config_get_ptr();
   menu_handle_t *menu          = menu_driver_get_ptr();
   global_t *global             = global_get_ptr();

   (void)device;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   if (!*dir)
   {
      menu_displaylist_parse_drive_list(list);
      menu_driver_populate_entries(dir, label, type);
      return 0;
   }

#if defined(GEKKO) && defined(HW_RVL)
   slock_lock(gx_device_mutex);
   device = gx_get_device_from_path(dir);

   if (device != -1 && !gx_devices[device].mounted &&
         gx_devices[device].interface->isInserted())
      fatMountSimple(gx_devices[device].name, gx_devices[device].interface);

   slock_unlock(gx_device_mutex);
#endif

   path_is_compressed = path_is_compressed_file(dir);
   push_dir           = (setting && setting->browser_selection_type == ST_DIR);

   if (path_is_compressed)
      str_list = compressed_file_list_new(dir,exts);
   else
      str_list = dir_list_new(dir,
            settings->menu.navigation.browser.filter.supported_extensions_enable 
            ? exts : NULL, true);

   if (push_dir)
      menu_list_push(list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   if (!str_list)
      return -1;

   dir_list_sort(str_list, true);


   list_size = str_list->size;
   for (i = 0; i < str_list->size; i++)
   {
      bool is_dir;
      const char *path = NULL;
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

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

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
      menu_list_get_last_stack(menu->menu_list, &dir, NULL, NULL);
      list_size = file_list_get_size(list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX_LENGTH], display_name[PATH_MAX_LENGTH];
         const char *path = NULL;

         menu_list_get_at_offset(list, i, &path, NULL, &type);

         if (type != MENU_FILE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (global->core_info &&
               core_info_list_get_display_name(global->core_info,
                  core_path, display_name, sizeof(display_name)))
            menu_list_set_alt_at_offset(list, i, display_name);
      }
      menu_list_sort_on_alt(list);
   }

   menu_list_populate_generic(list, dir, label, type);

   return 0;
}

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type)
{
   int ret = 0;
   menu_handle_t    *menu = menu_driver_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();
   global_t       *global = global_get_ptr();

   switch (type)
   {
      case DISPLAYLIST_NONE:
         break;
      case DISPLAYLIST_MAIN_MENU:
         menu->list_settings = setting_new(SL_FLAG_ALL);

         menu_list_push(menu_list->menu_stack,
               info->path, info->label, info->type, info->flags);
         menu_navigation_clear(nav, true);
      case DISPLAYLIST_SETTINGS:
         ret = menu_entries_push_list(menu, info->list,
               info->path, info->label, info->type, info->flags);
         break;
      case DISPLAYLIST_HORIZONTAL:
         break;
      case DISPLAYLIST_DEFAULT:
      case DISPLAYLIST_CORES:
      case DISPLAYLIST_CORES_DETECTED:
      case DISPLAYLIST_SHADER_PASS:
      case DISPLAYLIST_SHADER_PRESET:
      case DISPLAYLIST_DATABASES:
      case DISPLAYLIST_DATABASE_CURSORS:
      case DISPLAYLIST_VIDEO_FILTERS:
      case DISPLAYLIST_AUDIO_FILTERS:
      case DISPLAYLIST_IMAGES:
      case DISPLAYLIST_OVERLAYS:
      case DISPLAYLIST_FONTS:
      case DISPLAYLIST_CHEAT_FILES:
      case DISPLAYLIST_REMAP_FILES:
      case DISPLAYLIST_RECORD_CONFIG_FILES:
      case DISPLAYLIST_CONFIG_FILES:
      case DISPLAYLIST_CONTENT_HISTORY:
         ret = menu_displaylist_parse(info->list, info->menu_list,
               info->path, info->label, info->type,
               info->type_default, info->exts, info->setting);
         break;
      case DISPLAYLIST_PERFCOUNTER_SELECTION:
         menu_list_clear(info->list);
         menu_list_push(info->list, "Frontend Counters", "frontend_counters",
               MENU_SETTING_ACTION, 0);
         menu_list_push(info->list, "Core Counters", "core_counters",
               MENU_SETTING_ACTION, 0);

         menu_driver_populate_entries(info->path, info->label, info->type);
         break;
      case DISPLAYLIST_PERFCOUNTERS_CORE:
         ret = menu_displaylist_push_perfcounter_generic(info,
               perf_counters_libretro, perf_ptr_libretro, 
               MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
         break;
      case DISPLAYLIST_PERFCOUNTERS_FRONTEND:
         ret = menu_displaylist_push_perfcounter_generic(info,
               perf_counters_rarch, perf_ptr_rarch, 
               MENU_SETTINGS_PERF_COUNTERS_BEGIN);
         break;
   }

   return ret;
}

int menu_displaylist_deferred_push(menu_displaylist_info_t *info)
{
   unsigned type             = 0;
   const char *path          = NULL;
   const char *label         = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t       *menu = menu_driver_get_ptr();

   if (!info->list)
      return -1;

   menu_list_get_last_stack(menu->menu_list, &path, &label, &type);

   if (!strcmp(label, "Main Menu"))
      return menu_entries_push_list(menu, info->list, path, label, type,
            SL_FLAG_MAIN_MENU);
   else if (!strcmp(label, "Horizontal Menu"))
      return menu_entries_push_horizontal_menu_list(menu, info->list, path, label, type);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(menu->menu_list);

   if (!cbs->action_deferred_push)
      return 0;

   return cbs->action_deferred_push(info->list, info->menu_list, path, label, type);
}

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list)
{
   int ret;
   menu_handle_t *menu    = menu_driver_get_ptr();
   driver_t       *driver = driver_get_ptr();
   menu_displaylist_info_t info = {0};

   info.list      = list;
   info.menu_list = menu_list;
   
   ret = menu_displaylist_deferred_push(&info);

   menu->need_refresh = false;

   if (ret == 0)
   {
      const ui_companion_driver_t *ui = ui_companion_get_ptr();

      if (ui)
         ui->notify_list_loaded(driver->ui_companion_data, list, menu_list);
   }

   return ret;
}
