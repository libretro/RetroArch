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

#include <file/file_path.h>
#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "menu_navigation.h"

#include "../file_extract.h"
#include "../file_ops.h"
#include "../config.def.h"
#include "../retroarch.h"
#include "../runloop.h"

void menu_entries_common_load_content(bool persist)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return;

   rarch_main_command(persist ? RARCH_CMD_LOAD_CONTENT_PERSIST : RARCH_CMD_LOAD_CONTENT);

   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
   menu->msg_force = true;
}

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);
   return menu_setting_generic(setting);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */

static int zlib_extract_core_callback(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   char path[PATH_MAX_LENGTH];

   /* Make directory */
   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));
   path_basedir(path);

   if (!path_mkdir(path))
   {
      RARCH_ERR("Failed to create directory: %s.\n", path);
      return 0;
   }

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return 1;

   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));

   RARCH_LOG("path is: %s, CRC32: 0x%x\n", path, crc32);

   switch (cmode)
   {
      case 0: /* Uncompressed */
         write_file(path, cdata, size);
         break;
      case 8: /* Deflate */
         zlib_inflate_data_to_file(path, valid_exts, cdata, csize, size, crc32);
         break;
   }

   return 1;
}

int cb_core_updater_download(void *data_, size_t len)
{
   FILE *f;
   const char* file_ext = NULL;
   char output_path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   char *data = (char*)data_;

   if (!data)
      return -1;

   fill_pathname_join(output_path, g_settings.libretro_directory,
         core_updater_path, sizeof(output_path));
   
   f = fopen(output_path, "wb");

   if (!f)
      return -1;

   fwrite(data, 1, len, f);
   fclose(f);

   snprintf(msg, sizeof(msg), "Download complete: %s.",
         core_updater_path);

   rarch_main_msg_queue_push(msg, 1, 90, true);

#ifdef HAVE_ZLIB
   file_ext = path_get_extension(output_path);

   if (!g_settings.network.buildbot_auto_extract_archive)
      return 0;

   if (!strcasecmp(file_ext,"zip"))
   {
      if (!zlib_parse_file(output_path, NULL, zlib_extract_core_callback,

               (void*)g_settings.libretro_directory))
         RARCH_LOG("Could not process ZIP file.\n");
   }
#endif

   return 0;
}
#endif

int menu_entries_common_is_settings_entry(const char *label)
{
   return (
    !strcmp(label, "Driver Settings") ||
    !strcmp(label, "General Settings") ||
    !strcmp(label, "Video Settings") ||
    !strcmp(label, "Shader Settings") ||
    !strcmp(label, "Font Settings") ||
    !strcmp(label, "Audio Settings") ||
    !strcmp(label, "Input Settings") ||
    !strcmp(label, "Overlay Settings") ||
    !strcmp(label, "Menu Settings") ||
    !strcmp(label, "UI Settings") ||
    !strcmp(label, "Patch Settings") ||
    !strcmp(label, "Playlist Settings") ||
    !strcmp(label, "Onscreen Keyboard Overlay Settings") ||
    !strcmp(label, "Core Updater Settings") ||
    !strcmp(label, "Network Settings") ||
    !strcmp(label, "Archive Settings") ||
    !strcmp(label, "User Settings") ||
    !strcmp(label, "Path Settings") ||
    !strcmp(label, "Privacy Settings"));
}

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   struct string_list *str_list = NULL;
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   const char *menu_label = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   if (!list)
      return;

   if (!(cbs = (menu_file_list_cbs_t*)list->list[idx].actiondata))
      return;

   menu_list_get_last_stack(menu->menu_list,
         NULL, &menu_label, NULL);

   if (label)
      str_list = string_split(label, "|");

   if (str_list && str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   else elem0[0]='\0';
   if (str_list && str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));
   else elem1[0]='\0';

   if (str_list)
   {
      string_list_free(str_list);
      str_list = NULL;
   }

   menu_entries_cbs_init_bind_ok(cbs, path, label, type, idx, elem0, elem1, menu_label);
   menu_entries_cbs_init_bind_cancel(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_start(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_select(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_content_list_switch(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_up_or_down(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_toggle(cbs, path, label, type, idx, elem0, elem1, menu_label);
   menu_entries_cbs_init_bind_deferred_push(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_refresh(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_iterate(cbs, path, label, type, idx, elem0, elem1);
   menu_entries_cbs_init_bind_get_string_representation(cbs, path, label, type, idx, elem0, elem1);
}
