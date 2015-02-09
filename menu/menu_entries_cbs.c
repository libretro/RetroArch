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
#include "menu_action.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "menu_shader.h"

#include "../file_ext.h"
#include "../file_extract.h"
#include "../file_ops.h"
#include "../config.def.h"
#include "../cheats.h"
#include "../retroarch.h"
#include "../performance.h"

#ifdef HAVE_NETWORKING
#include "../net_http.h"
#endif

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "menu_database.h"

#include "../input/input_autodetect.h"
#include "../input/input_remapping.h"

#ifdef GEKKO
enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_608_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_598_400,
   GX_RESOLUTIONS_640_400,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_448_448,
   GX_RESOLUTIONS_480_448,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_576_448,
   GX_RESOLUTIONS_608_448,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_340_464,
   GX_RESOLUTIONS_512_464,
   GX_RESOLUTIONS_512_472,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_480,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_530_480,
   GX_RESOLUTIONS_608_480,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 608, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;
#endif

static unsigned rdb_entry_start_game_selection_ptr;

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH];
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;

   menu_list_pop_stack(driver.menu->menu_list);

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(driver.menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
         driver.menu->selection_ptr, &path, NULL, &type);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         driver.menu->menu_list,
         cat_path,
         menu_label,
         type,
         driver.menu->selection_ptr);

   return 0;
}

static void common_load_content(bool persist)
{
   rarch_main_command(persist ? RARCH_CMD_LOAD_CONTENT_PERSIST : RARCH_CMD_LOAD_CONTENT);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;
}

static int archive_load(void)
{
   int ret;
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;

   menu_list_pop_stack(driver.menu->menu_list);

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(driver.menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
         driver.menu->selection_ptr, &path, NULL, &type);

   ret = rarch_defer_core(g_extern.core_info, menu_path, path, menu_label,
         driver.menu->deferred_path, sizeof(driver.menu->deferred_path));

   switch (ret)
   {
      case -1:
         rarch_main_command(RARCH_CMD_LOAD_CORE);
         common_load_content(false);
         break;
      case 0:
         menu_list_push_stack_refresh(
               driver.menu->menu_list,
               g_settings.libretro_directory,
               "deferred_core_list",
               0,
               driver.menu->selection_ptr);
         break;
   }

   return 0;
}

static int load_or_open_zip_iterate(unsigned action)
{
   char msg[PATH_MAX_LENGTH];

   snprintf(msg, sizeof(msg), "Opening compressed file\n"
         " \n"

         " - OK to open as Folder\n"
         " - Cancel/Back to Load \n");

   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

   switch (action)
   {
      case MENU_ACTION_OK:
         archive_open();
         break;
      case MENU_ACTION_CANCEL:
         archive_load();
         break;
   }

   return 0;
}

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);
   return menu_action_generic_setting(setting);
}

static int action_ok_rdb_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   rarch_playlist_load_content(driver.menu->db_playlist,
         rdb_entry_start_game_selection_ptr);
   return -1;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   rarch_playlist_load_content(g_defaults.history,
         driver.menu->selection_ptr);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   return -1;
}



static int action_ok_cheat_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx)
{
   cheat_manager_t *cheat = g_extern.cheat;

   if (!cheat)
      return -1;

   cheat_manager_apply_cheats(cheat);

   return 0;
}

/* FIXME: Ugly hack, need to be refactored badly. */
size_t hack_shader_pass = 0;

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   if (!driver.menu)
      return -1;
   (void)menu_path;

#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(driver.menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(driver.menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   video_shader_resolve_parameters(NULL, driver.menu->shader);
   menu_list_flush_stack_by_needle(driver.menu->menu_list, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

#ifdef HAVE_SHADER_MANAGER
extern size_t hack_shader_pass;
#endif

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx)
{
   hack_shader_pass = type - MENU_SETTINGS_SHADER_PASS_0;

   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.video.shader_dir, 
         label,
         type,
         idx);
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         "", label, MENU_SETTING_ACTION,
         idx);
}

static int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (path)
      strlcpy(driver.menu->deferred_path, path,
            sizeof(driver.menu->deferred_path));
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         "", label, type, idx);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         label, label, type, idx);
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.video.shader_dir, 
         label, type, idx);
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.menu_content_directory,
         label, MENU_FILE_DIRECTORY, idx);
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.menu_content_directory, label, type,
         idx);
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *dir = g_settings.menu_config_directory;
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         dir ? dir : label, label, type,
         idx);
}

static int action_ok_cheat_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.cheat_database,
         label, type, idx);
}

static int action_ok_remap_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.input_remapping_directory,
         label, type, idx);
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         g_settings.libretro_directory,
         label, type, idx);
}

static int action_ok_remap_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char remap_path[PATH_MAX_LENGTH];
   if (!driver.menu)
      return -1;

   (void)remap_path;
   (void)menu_path;
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(remap_path, menu_path, path, sizeof(remap_path));
   input_remapping_load_file(remap_path);

   menu_list_flush_stack_by_needle(driver.menu->menu_list, "core_input_remapping_options");

   return 0;
}

static int action_ok_cheat_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char cheat_path[PATH_MAX_LENGTH];
   if (!driver.menu)
      return -1;

   (void)cheat_path;
   (void)menu_path;
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(cheat_path, menu_path, path, sizeof(cheat_path));

   if (g_extern.cheat)
      cheat_manager_free(g_extern.cheat);

   g_extern.cheat = cheat_manager_load(cheat_path);

   if (!g_extern.cheat)
      return -1;

   menu_list_flush_stack_by_needle(driver.menu->menu_list, "core_cheat_options");

   return 0;
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path = NULL;
   rarch_setting_t *setting = NULL;
   char wallpaper_path[PATH_MAX_LENGTH];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, &menu_label,
         NULL);

   setting = menu_action_find_setting(menu_label);

   if (!setting)
      return -1;

   fill_pathname_join(wallpaper_path, menu_path, path, sizeof(wallpaper_path));

   strlcpy(g_settings.menu.wallpaper, wallpaper_path, sizeof(g_settings.menu.wallpaper));
   menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);

   return 0;
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char shader_path[PATH_MAX_LENGTH];
   if (!driver.menu)
      return -1;

   (void)shader_path;
   (void)menu_path;
#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(driver.menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(driver.menu->shader,
         video_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_list_flush_stack_by_needle(driver.menu->menu_list, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_cheat(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line(driver.menu, "Input Cheat",
         label, type, idx, menu_input_st_cheat_callback);
   return 0;
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line(driver.menu, "Preset Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_cheat_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line(driver.menu, "Cheat Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_remap_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line(driver.menu, "Remapping Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = menu_action_find_setting(menu_label);

   if (!setting)
      return -1;

   if (setting->type != ST_DIR)
      return -1;

   menu_action_setting_set_current_string(setting, menu_path);
   menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);

   return 0;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   if (path)
      strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, driver.menu->deferred_path,
         sizeof(g_extern.fullpath));

   common_load_content(false);

   return -1;
}

static int action_ok_database_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int deferred_push_core_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   core_info_t *info      = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   info = (core_info_t*)g_extern.core_info_current;
   menu_list_clear(list);

   if (info->data)
   {
      char tmp[PATH_MAX_LENGTH];

      snprintf(tmp, sizeof(tmp), "Core name: %s",
            info->core_name ? info->core_name : "");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(tmp, sizeof(tmp), "Core label: %s",
            info->display_name ? info->display_name : "");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      if (info->systemname)
      {
         snprintf(tmp, sizeof(tmp), "System name: %s",
               info->systemname);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->system_manufacturer)
      {
         snprintf(tmp, sizeof(tmp), "System manufacturer: %s",
               info->system_manufacturer);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->categories_list)
      {
         strlcpy(tmp, "Categories: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->categories_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->authors_list)
      {
         strlcpy(tmp, "Authors: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->authors_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->permissions_list)
      {
         strlcpy(tmp, "Permissions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->permissions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->licenses_list)
      {
         strlcpy(tmp, "License(s): ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->licenses_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->supported_extensions_list)
      {
         strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->supported_extensions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->firmware_count > 0)
      {
         core_info_list_update_missing_firmware(
               g_extern.core_info, info->path,
               g_settings.system_directory);

         menu_list_push(list, "Firmware: ", "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
         for (i = 0; i < info->firmware_count; i++)
         {
            if (info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "	name: %s",
                     info->firmware[i].desc ? info->firmware[i].desc : "");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);

               snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                     info->firmware[i].missing ?
                     "missing" : "present",
                     info->firmware[i].optional ?
                     "optional" : "required");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);
            }
         }
      }

      if (info->notes)
      {
         snprintf(tmp, sizeof(tmp), "Core notes: ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         for (i = 0; i < info->note_list->size; i++)
         {
            snprintf(tmp, sizeof(tmp), " %s",
                  info->note_list->elems[i].data);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
   }
   else
      menu_list_push(list,
            "No information available.", "",
            MENU_SETTINGS_CORE_OPTION_NONE, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int create_string_list_rdb_entry_string(const char *desc, const char *label,
      const char *actual_string, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   str_len += strlen(actual_string) + 1;
   string_list_append(str_list, actual_string, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %s", desc, actual_string);
   menu_list_push(list, tmp, output_label, 0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}

static uint32_t create_string_list_rdb_entry_int(const char *desc, const char *label,
      int actual_int, const char *path, file_list_t *list)
{
   char tmp[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   char str[PATH_MAX_LENGTH];
   char *output_label = NULL;
   int str_len = 0;
   struct string_list *str_list = string_list_new();

   if (!str_list)
      return -1;

   str_len += strlen(label) + 1;
   string_list_append(str_list, label, attr);

   str_len += sizeof(actual_int);
   snprintf(str, sizeof(str), "%d", actual_int);
   string_list_append(str_list, str, attr);

   str_len += strlen(path) + 1;
   string_list_append(str_list, path, attr);

   output_label = (char*)calloc(str_len, sizeof(char));

   if (!output_label)
   {
      string_list_free(str_list);
      return -1;
   }

   string_list_join_concat(output_label, str_len, str_list, "|");

   snprintf(tmp, sizeof(tmp), "%s: %d", desc, actual_int);
   menu_list_push(list, tmp, output_label,
         0, 0);

   if (output_label)
      free(output_label);
   string_list_free(str_list);
   str_list = NULL;

   return 0;
}


static int deferred_push_rdb_entry_detail(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
#ifdef HAVE_LIBRETRODB
   content_playlist_t *playlist;
   char query[PATH_MAX_LENGTH];
   char path_rdl[PATH_MAX_LENGTH], path_base[PATH_MAX_LENGTH];
   unsigned i, j;
   int ret = 0;
   database_info_list_t *db_info = NULL;
   file_list_t *list             = (file_list_t*)data;
   file_list_t *menu_list        = (file_list_t*)userdata;
   struct string_list *str_list  = NULL;
   
   str_list = string_split(label, "|"); 

   if (!str_list)
      return -1;

   if (!list || !menu_list)
   {
      ret = -1;
      goto done;
   }

   strlcpy(query, "{'name':\"", sizeof(query));
   strlcat(query, str_list->elems[1].data, sizeof(query));
   strlcat(query, "\"}", sizeof(query));

   menu_list_clear(list);

   if (!(db_info = database_info_list_new(path, query)))
   {
      ret = -1;
      goto done;
   }

   strlcpy(path_base, path_basename(path), sizeof(path_base));
   path_remove_extension(path_base);
   strlcat(path_base, ".rdl", sizeof(path_base));

   fill_pathname_join(path_rdl, g_settings.content_database, path_base,
         sizeof(path_rdl));

   menu_database_realloc(driver.menu, path_rdl, false);

   playlist = driver.menu->db_playlist;

   for (i = 0; i < db_info->count; i++)
   {
      char tmp[PATH_MAX_LENGTH];
      database_info_t *db_info_entry = (database_info_t*)&db_info->list[i];

      if (!db_info_entry)
         continue;


      if (db_info_entry->name)
      {
         snprintf(tmp, sizeof(tmp), "Name: %s", db_info_entry->name);
         menu_list_push(list, tmp, "rdb_entry_name",
               0, 0);
      }
      if (db_info_entry->description)
      {
         snprintf(tmp, sizeof(tmp), "Description: %s", db_info_entry->description);
         menu_list_push(list, tmp, "rdb_entry_description",
               0, 0);
      }
      if (db_info_entry->publisher)
      {
         if (create_string_list_rdb_entry_string("Publisher", "rdb_entry_publisher",
               db_info_entry->publisher, path, list) == -1)
            return -1;
      }
      if (db_info_entry->developer)
      {
         if (create_string_list_rdb_entry_string("Developer", "rdb_entry_developer",
               db_info_entry->developer, path, list) == -1)
            return -1;
      }
      if (db_info_entry->origin)
      {
         if (create_string_list_rdb_entry_string("Origin", "rdb_entry_origin",
               db_info_entry->origin, path, list) == -1)
            return -1;
      }
      if (db_info_entry->franchise)
      {
         if (create_string_list_rdb_entry_string("Franchise", "rdb_entry_franchise",
               db_info_entry->franchise, path, list) == -1)
            return -1;
      }
      if (db_info_entry->max_users)
      {
         if (create_string_list_rdb_entry_int("Max Users",
               "rdb_entry_max_users", db_info_entry->max_users,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->famitsu_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Famitsu Magazine Rating",
               "rdb_entry_famitsu_magazine_rating", db_info_entry->famitsu_magazine_rating,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_rating)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Rating",
               "rdb_entry_edge_magazine_rating", db_info_entry->edge_magazine_rating,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->edge_magazine_issue)
      {
         if (create_string_list_rdb_entry_int("Edge Magazine Issue",
               "rdb_entry_edge_magazine_issue", db_info_entry->edge_magazine_issue,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->releasemonth)
      {
         if (create_string_list_rdb_entry_int("Releasedate Month",
               "rdb_entry_releasemonth", db_info_entry->releasemonth,
               path, list) == -1)
            return -1;
      }

      if (db_info_entry->releaseyear)
      {
         if (create_string_list_rdb_entry_int("Releasedate Year",
               "rdb_entry_releaseyear", db_info_entry->releaseyear,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->bbfc_rating)
      {
         if (create_string_list_rdb_entry_string("BBFC Rating", "rdb_entry_bbfc_rating",
               db_info_entry->bbfc_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->esrb_rating)
      {
         if (create_string_list_rdb_entry_string("ESRB Rating", "rdb_entry_esrb_rating",
               db_info_entry->esrb_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->elspa_rating)
      {
         if (create_string_list_rdb_entry_string("ELSPA Rating", "rdb_entry_elspa_rating",
               db_info_entry->elspa_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->pegi_rating)
      {
         if (create_string_list_rdb_entry_string("PEGI Rating", "rdb_entry_pegi_rating",
               db_info_entry->pegi_rating, path, list) == -1)
            return -1;
      }
      if (db_info_entry->enhancement_hw)
      {
         if (create_string_list_rdb_entry_string("Enhancement Hardware", "rdb_entry_enhancement_hw",
               db_info_entry->enhancement_hw, path, list) == -1)
            return -1;
      }
      if (db_info_entry->cero_rating)
      {
         if (create_string_list_rdb_entry_string("CERO Rating", "rdb_entry_cero_rating",
               db_info_entry->cero_rating, path, list) == -1)
            return -1;
      }
      snprintf(tmp, sizeof(tmp),
            "Analog supported: %s",
            (db_info_entry->analog_supported == 1)  ? "true" : 
            (db_info_entry->analog_supported == -1) ? "N/A"  : "false");
      menu_list_push(list, tmp, "rdb_entry_analog",
            0, 0);
      snprintf(tmp, sizeof(tmp),
            "Rumble supported: %s",
            (db_info_entry->rumble_supported == 1)  ? "true" : 
            (db_info_entry->rumble_supported == -1) ? "N/A"  :  "false");
      menu_list_push(list, tmp, "rdb_entry_rumble",
            0, 0);

      if (db_info_entry->crc32)
      {
         if (create_string_list_rdb_entry_string("CRC32 Checksum",
               "rdb_entry_crc32", db_info_entry->crc32,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->sha1)
      {
         if (create_string_list_rdb_entry_string("SHA1 Checksum",
               "rdb_entry_sha1", db_info_entry->sha1,
               path, list) == -1)
            return -1;
      }
      if (db_info_entry->md5)
      {
         if (create_string_list_rdb_entry_string("MD5 Checksum",
               "rdb_entry_md5", db_info_entry->md5,
               path, list) == -1)
            return -1;
      }

      if (playlist)
      {
         for (j = 0; j < playlist->size; j++)
         {
            char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
            bool match_found = false;
            str_list = string_split(
                  playlist->entries[j].core_name, "|"); 

            if (!str_list)
               continue;;

            if (str_list && str_list->size > 0)
               strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
            if (str_list && str_list->size > 1)
               strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

            if (!strcmp(elem1, "crc"))
            {
               if (!strcmp(db_info_entry->crc32, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "sha1"))
            {
               if (!strcmp(db_info_entry->sha1, elem0))
                  match_found = true;
            }
            else if (!strcmp(elem1, "md5"))
            {
               if (!strcmp(db_info_entry->md5, elem0))
                  match_found = true;
            }

            string_list_free(str_list);

            if (!match_found)
               continue;

            rdb_entry_start_game_selection_ptr = j;
            menu_list_push(list, "Start Content", "rdb_entry_start_game",
                  MENU_FILE_PLAYLIST_ENTRY, 0);
         }
      }
   }
   


   if (db_info->count < 1)
      menu_list_push(list,
            "No information available.", "",
            0, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path,
            str_list->elems[0].data, type);

   ret = 0;

#endif

done:
#ifdef HAVE_LIBRETRODB
   string_list_free(str_list);
#endif
   return ret;
}

static int action_ok_rdb_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char tmp[PATH_MAX_LENGTH];
   strlcpy(tmp, "deferred_rdb_entry_detail|", sizeof(tmp));
   strlcat(tmp, path, sizeof(tmp));

   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         label,
         tmp,
         0, idx);
}

static int action_ok_cursor_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(g_settings.libretro, menu_path, path,
         sizeof(g_settings.libretro));
   rarch_main_command(RARCH_CMD_LOAD_CORE);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */
   if (driver.menu->load_no_content)
   {
      *g_extern.fullpath = '\0';
      common_load_content(false);
      return -1;
   }

   return 0;
   /* Core selection on non-console just updates directory listing.
    * Will take effect on new content load. */
#elif defined(RARCH_CONSOLE)
   rarch_main_command(RARCH_CMD_RESTART_RETROARCH);
   return -1;
#endif
}

static int action_ok_core_download(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   return 0;
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack(
         driver.menu->menu_list,
         path,
         "load_open_zip",
         0,
         idx);

   return 0;
}

static int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX_LENGTH];

   if (!driver.menu)
      return -1;

   if (!path)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));

   return menu_list_push_stack_refresh(driver.menu->menu_list,
         cat_path, menu_label, type, idx);
}

static int action_ok_database_manager_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char rdb_path[PATH_MAX_LENGTH];
   if (!path)
      return -1;
   if (!label)
      return -1;

   fill_pathname_join(rdb_path, g_settings.content_database,
         path, sizeof(rdb_path));

   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         rdb_path,
         "deferred_database_manager_list",
         0, idx);
}

static int action_ok_cursor_manager_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char cursor_path[PATH_MAX_LENGTH];

   fill_pathname_join(cursor_path, g_settings.cursor_directory,
         path, sizeof(cursor_path));

   return menu_list_push_stack_refresh(
         driver.menu->menu_list,
         cursor_path,
         "deferred_cursor_manager_list",
         0, idx);
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   char config[PATH_MAX_LENGTH];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(config, menu_path, path, sizeof(config));
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;
   if (rarch_replace_config(config))
   {
      menu_navigation_clear(driver.menu, false);
      return -1;
   }

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   char image[PATH_MAX_LENGTH];

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(image, menu_path, path, sizeof(image));
   rarch_disk_control_append_image(image);

   rarch_main_command(RARCH_CMD_RESUME);

   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   return -1;
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   int ret;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, NULL, NULL);

   ret = rarch_defer_core(g_extern.core_info,
         menu_path, path, label, driver.menu->deferred_path,
         sizeof(driver.menu->deferred_path));

   if (ret == -1)
   {
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      common_load_content(false);
      return -1;
   }

   if (ret == 0)
      menu_list_push_stack_refresh(
            driver.menu->menu_list,
            g_settings.libretro_directory,
            "deferred_core_list",
            0, idx);

   return ret;
}


static int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);
   return menu_action_generic_setting(setting);
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last(driver.menu->menu_list->menu_stack,
         &menu_path, &menu_label, NULL);

   setting = menu_action_find_setting(menu_label);

   if (setting && setting->type == ST_PATH)
   {
      menu_action_setting_set_current_string_path(setting, menu_path, path);
      menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);
   }
   else
   {
      if (type == MENU_FILE_IN_CARCHIVE)
         fill_pathname_join_delim(g_extern.fullpath, menu_path, path,
               '#',sizeof(g_extern.fullpath));
      else
         fill_pathname_join(g_extern.fullpath, menu_path, path,
               sizeof(g_extern.fullpath));

      common_load_content(true);

      return -1;
   }

   return 0;
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   menu_list_get_last_stack(driver.menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = menu_action_find_setting(menu_label);

   if (!setting)
      return -1;

   menu_action_setting_set_current_string_path(setting, menu_path, path);
   menu_list_pop_stack_by_needle(driver.menu->menu_list, setting->name);

   return 0;
}

static int action_ok_custom_viewport(const char *path,
      const char *label, unsigned type, size_t idx)
{
   /* Start with something sane. */
   rarch_viewport_t *custom = (rarch_viewport_t*)
      &g_extern.console.screen.viewports.custom_vp;

   menu_list_push_stack(
         driver.menu->menu_list,
         "",
         "custom_viewport_1",
         MENU_SETTINGS_CUSTOM_VIEWPORT,
         idx);

   if (driver.video_data && driver.video &&
         driver.video->viewport_info)
      driver.video->viewport_info(driver.video_data, custom);

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   return 0;
}


static int generic_action_ok_command(unsigned cmd)
{
   if (!driver.menu)
      return -1;

   if (!rarch_main_command(cmd))
      return -1;
   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (generic_action_ok_command(RARCH_CMD_LOAD_STATE) == -1)
      return -1;
   return generic_action_ok_command(RARCH_CMD_RESUME);
}


static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (generic_action_ok_command(RARCH_CMD_SAVE_STATE) == -1)
      return -1;
   return generic_action_ok_command(RARCH_CMD_RESUME);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
static char core_updater_path[PATH_MAX_LENGTH];

static bool zlib_extract_core_callback(const char *name, const char *valid_exts,
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
      return false;
   }

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return true;

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

   return true;
}

static int cb_core_updater_download(void *data_, size_t len)
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

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 90);

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

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
#ifdef HAVE_NETWORKING
   char core_path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   fill_pathname_join(core_path, g_settings.network.buildbot_url,
         path, sizeof(core_path));

   strlcpy(core_updater_path, path, sizeof(core_updater_path));
   snprintf(msg, sizeof(msg), "Starting download: %s.", path);

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 90);

   msg_queue_clear(g_extern.http_msg_queue);
   msg_queue_push(g_extern.http_msg_queue, core_path, 0, 1);

   net_http_set_pending_cb(cb_core_updater_download);
#endif
   return 0;
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_DISK_EJECT_TOGGLE);
}

static int action_ok_quit(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_QUIT);
}

static int action_ok_save_new_config(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_MENU_SAVE_CONFIG);
}

static int action_ok_resume_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_RESUME);
}

static int action_ok_restart_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_RESET);
}

static int action_ok_screenshot(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_TAKE_SCREENSHOT);
}

static int action_ok_file_load_or_resume(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!strcmp(driver.menu->deferred_path, g_extern.fullpath))
      return generic_action_ok_command(RARCH_CMD_RESUME);
   else
   {
      strlcpy(g_extern.fullpath,
            driver.menu->deferred_path, sizeof(g_extern.fullpath));
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
      return -1;
   }
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_action_setting_set(type, label, MENU_ACTION_OK);
}

static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx)
{
   int ret;
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH];
   char *rdb = NULL;
   int len = 0;
   struct string_list *str_list  = NULL;
   struct string_list *str_list2 = NULL;

   if (!label)
      return -1;

   str_list = string_split(label, "|"); 

   if (!str_list)
      return -1;

   str_list2 = string_list_new();
   if (!str_list2)
   {
      string_list_free(str_list);
      return -1;
   }

   /* element 0 : label
    * element 1 : value
    * element 2 : database path
    */

   len += strlen(str_list->elems[1].data) + 1;
   string_list_append(str_list2, str_list->elems[1].data, attr);

   len += strlen(str_list->elems[2].data) + 1;
   string_list_append(str_list2, str_list->elems[2].data, attr);

   rdb = (char*)calloc(len, sizeof(char));

   if (!rdb)
   {
      string_list_free(str_list);
      string_list_free(str_list2);
      return -1;
   }

   string_list_join_concat(rdb, len, str_list2, "|");

   strlcpy(new_label, "deferred_cursor_manager_list_", sizeof(new_label));
   strlcat(new_label, str_list->elems[0].data, sizeof(new_label));

   ret = menu_list_push_stack_refresh(
         driver.menu->menu_list,
         rdb,
         new_label,
         0, idx);

   string_list_free(str_list);
   string_list_free(str_list2);

   return ret;
}


static int action_cancel_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_action_setting_set(type, label, MENU_ACTION_CANCEL);
}

static int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_apply_deferred_settings();
   menu_list_pop_stack(driver.menu->menu_list);
   return 0;
}


static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (!driver.menu)
      return -1;

   menu_list_push_stack(
         driver.menu->menu_list,
         "",
         "help",
         0,
         0);
   driver.menu->push_start_screen = false;

   return 0;
}

static int action_start_remap_file_load(unsigned type, const char *label,
      unsigned action)
{
   g_settings.input.remapping_path[0] = '\0';
   input_remapping_set_defaults();
   return 0;
}

static int action_start_performance_counters_core(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   (void)label;
   (void)action;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_input_desc(unsigned type, const char *label,
      unsigned action)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);

   (void)label;
   (void)action;

   g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset] = 
      g_settings.input.binds[inp_desc_user][inp_desc_button_index_offset].id;

   return 0;
}

static int action_start_shader_action_parameter(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;

   if (driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      shader = driver.video_poke->get_current_shader(driver.video_data);

   if (!shader)
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   param->current = param->initial;
   param->current = min(max(param->minimum, param->current), param->maximum);
#endif

   return 0;
}

static int action_start_shader_action_preset_parameter(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;

   if (!(shader = driver.menu->shader))
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];
   param->current = param->initial;
   param->current = min(max(param->minimum, param->current), param->maximum);
#endif

   return 0;
}

static int shader_action_parameter_toggle(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;

   if (driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      shader = driver.video_poke->get_current_shader(driver.video_data);

   if (!shader)
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   switch (action)
   {
      case MENU_ACTION_LEFT:
         param->current -= param->step;
         break;

      case MENU_ACTION_RIGHT:
         param->current += param->step;
         break;

      default:
         break;
   }

   param->current = min(max(param->minimum, param->current), param->maximum);

#endif
   return 0;
}

static int shader_action_parameter_preset_toggle(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;

   if (!(shader = driver.menu->shader))
      return 0;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];

   switch (action)
   {
      case MENU_ACTION_LEFT:
         param->current -= param->step;
         break;

      case MENU_ACTION_RIGHT:
         param->current += param->step;
         break;

      default:
         break;
   }

   param->current = min(max(param->minimum, param->current), param->maximum);

#endif
   return 0;
}

static int action_toggle_cheat(unsigned type, const char *label,
      unsigned action)
{
   cheat_manager_t *cheat = g_extern.cheat;
   size_t idx = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (!cheat)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         cheat->cheats[idx].state = !cheat->cheats[idx].state;
         cheat_manager_update(cheat, idx);
         break;
   }

   return 0;
}

static int action_toggle_input_desc(unsigned type, const char *label,
      unsigned action)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset] > 0)
            g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset]--;
         break;
      case MENU_ACTION_RIGHT:
         if (g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset] < RARCH_FIRST_CUSTOM_BIND)
            g_settings.input.remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
         break;
   }

   return 0;
}


static int action_start_shader_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   hack_shader_pass = type - MENU_SETTINGS_SHADER_PASS_0;
   struct video_shader *shader = driver.menu->shader;
   struct video_shader_pass *shader_pass = NULL;

   if (shader)
      shader_pass = &shader->pass[hack_shader_pass];

   if (shader_pass)
      *shader_pass->source.path = '\0';
#endif

   return 0;
}


static int action_start_shader_scale_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader = driver.menu->shader;
   struct video_shader_pass *shader_pass = &shader->pass[pass];

   if (shader)
   {
      shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = 0;
      shader_pass->fbo.valid = false;
   }
#endif

   return 0;
}

static int action_toggle_save_state(unsigned type, const char *label,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         /* Slot -1 is (auto) slot. */
         if (g_settings.state_slot >= 0)
            g_settings.state_slot--;
         break;
      case MENU_ACTION_RIGHT:
         g_settings.state_slot++;
         break;
   }

   return 0;
}

static int action_toggle_scroll(unsigned type, const char *label,
      unsigned action)
{
   unsigned scroll_speed = 0, fast_scroll_speed = 0;
   if (!driver.menu)
      return -1;

   scroll_speed      = (max(driver.menu->scroll_accel, 2) - 2) / 4 + 1;
   fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (driver.menu->selection_ptr > fast_scroll_speed)
            menu_navigation_set(driver.menu,
                  driver.menu->selection_ptr - fast_scroll_speed, true);
         else
            menu_navigation_clear(driver.menu, false);
         break;
      case MENU_ACTION_RIGHT:
         if (driver.menu->selection_ptr + fast_scroll_speed < (menu_list_get_size(driver.menu->menu_list)))
            menu_navigation_set(driver.menu,
                  driver.menu->selection_ptr + fast_scroll_speed, true);
         else
         {
            if ((menu_list_get_size(driver.menu->menu_list) > 0))
                  menu_navigation_set_last(driver.menu);
         }
         break;
   }

   return 0;
}

static int action_toggle_mainmenu(unsigned type, const char *label,
      unsigned action)
{
   menu_file_list_cbs_t *cbs = NULL;
   unsigned push_list = 0;
   if (!driver.menu)
      return -1;

   if (file_list_get_size(driver.menu->menu_list->menu_stack) == 1)
   {
      if (!strcmp(driver.menu_ctx->ident, "xmb"))
      {
         driver.menu->selection_ptr = 0;
         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (driver.menu->cat_selection_ptr == 0)
                  break;
               push_list = 1;
               break;
            case MENU_ACTION_RIGHT:
               if (driver.menu->cat_selection_ptr == g_extern.core_info->count)
                  break;
               push_list = 1;
               break;
         }
      }
   }
   else 
      push_list = 2;

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   switch (push_list)
   {
      case 1:
         if (driver.menu_ctx->list_cache)
            driver.menu_ctx->list_cache(true, action);

         if (cbs && cbs->action_content_list_switch)
            return cbs->action_content_list_switch(
                  driver.menu->menu_list->selection_buf,
                  driver.menu->menu_list->menu_stack,
                  "",
                  "",
                  0);
         break;
      case 2:
         action_toggle_scroll(0, "", action);
         break;
      case 0:
      default:
         break;
   }

   return 0;
}

static int action_toggle_shader_scale_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader = driver.menu->shader;
   struct video_shader_pass *shader_pass = &shader->pass[pass];

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned current_scale = shader_pass->fbo.scale_x;
            unsigned delta = action == MENU_ACTION_LEFT ? 5 : 1;
            current_scale = (current_scale + delta) % 6;

            if (shader_pass)
            {
               shader_pass->fbo.valid = current_scale;
               shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;
            }
         }
         break;
   }
#endif
   return 0;
}

static int action_start_shader_filter_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader = driver.menu->shader;
   struct video_shader_pass *shader_pass = &shader->pass[pass];

   if (shader && shader_pass)
      shader_pass->filter = RARCH_FILTER_UNSPEC;
#endif

   return 0;
}

static int action_toggle_shader_filter_pass(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader = driver.menu->shader;
   struct video_shader_pass *shader_pass = &shader->pass[pass];

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned delta = (action == MENU_ACTION_LEFT) ? 2 : 1;
            if (shader_pass)
               shader_pass->filter = ((shader_pass->filter + delta) % 3);
         }
         break;
   }
#endif
   return 0;
}

static int action_toggle_shader_filter_default(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_action_find_setting("video_smooth");
   if (setting)
      menu_action_setting_handler(setting, action);
#endif
   return 0;
}

static int action_start_shader_num_passes(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = driver.menu->shader;

   if (!shader)
      return -1;

   if (shader->passes)
      shader->passes = 0;
   driver.menu->need_refresh = true;

   video_shader_resolve_parameters(NULL, driver.menu->shader);
#endif
   return 0;
}

static int action_start_cheat_num_passes(unsigned type, const char *label,
      unsigned action)
{
   cheat_manager_t *cheat = g_extern.cheat;

   if (!cheat)
      return -1;

   if (cheat->size)
   {
      cheat_manager_realloc(cheat, 0);
      driver.menu->need_refresh = true;
   }

   return 0;
}

static int action_toggle_cheat_num_passes(unsigned type, const char *label,
      unsigned action)
{
   unsigned new_size = 0;
   cheat_manager_t *cheat = g_extern.cheat;

   if (!cheat)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (cheat->size)
            new_size = cheat->size - 1;
         driver.menu->need_refresh = true;
         break;

      case MENU_ACTION_RIGHT:
         new_size = cheat->size + 1;
         driver.menu->need_refresh = true;
         break;
   }

   if (driver.menu->need_refresh)
      cheat_manager_realloc(cheat, new_size);

   return 0;
}

static int action_toggle_shader_num_passes(unsigned type, const char *label,
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = driver.menu->shader;

   if (!shader)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (shader->passes)
            shader->passes--;
         driver.menu->need_refresh = true;
         break;

      case MENU_ACTION_RIGHT:
         if ((shader->passes < GFX_MAX_SHADERS))
            shader->passes++;
         driver.menu->need_refresh = true;
         break;
   }

   if (driver.menu->need_refresh)
      video_shader_resolve_parameters(NULL, driver.menu->shader);

#endif
   return 0;
}


static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx)
{
#ifdef GEKKO
   if (driver.video_data)
      gx_set_video_mode(driver.video_data, menu_gx_resolutions
            [menu_current_gx_resolution][0],
            menu_gx_resolutions[menu_current_gx_resolution][1]);
#elif defined(__CELLOS_LV2__)
   if (g_extern.console.screen.resolutions.list[
         g_extern.console.screen.resolutions.current.idx] == 
         CELL_VIDEO_OUT_RESOLUTION_576)
   {
      if (g_extern.console.screen.pal_enable)
         g_extern.console.screen.pal60_enable = true;
   }
   else
   {
      g_extern.console.screen.pal_enable = false;
      g_extern.console.screen.pal60_enable = false;
   }

   rarch_main_command(RARCH_CMD_REINIT);
#endif
   return 0;
}

static int action_toggle_video_resolution(unsigned type, const char *label,
      unsigned action)
{
#ifdef GEKKO
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (menu_current_gx_resolution > 0)
            menu_current_gx_resolution--;
         break;
      case MENU_ACTION_RIGHT:
         if (menu_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
         {
#ifdef HW_RVL
            if ((menu_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
               if (CONF_GetVideo() != CONF_VIDEO_PAL)
                  return 0;
#endif

            menu_current_gx_resolution++;
         }
         break;
   }
#elif defined(__CELLOS_LV2__)
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (g_extern.console.screen.resolutions.current.idx)
         {
            g_extern.console.screen.resolutions.current.idx--;
            g_extern.console.screen.resolutions.current.id =
               g_extern.console.screen.resolutions.list
               [g_extern.console.screen.resolutions.current.idx];
         }
         break;
      case MENU_ACTION_RIGHT:
         if (g_extern.console.screen.resolutions.current.idx + 1 <
               g_extern.console.screen.resolutions.count)
         {
            g_extern.console.screen.resolutions.current.idx++;
            g_extern.console.screen.resolutions.current.id =
               g_extern.console.screen.resolutions.list
               [g_extern.console.screen.resolutions.current.idx];
         }
         break;
   }
#endif

   return 0;
}

static int action_start_performance_counters_frontend(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset])
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int action_start_core_setting(unsigned type,
      const char *label, unsigned action)
{
   unsigned idx = type - MENU_SETTINGS_CORE_OPTION_START;

   (void)label;

   core_option_set_default(g_extern.system.core_options, idx);

   return 0;
}

static int core_setting_toggle(unsigned type, const char *label,
      unsigned action)
{
   unsigned idx = type - MENU_SETTINGS_CORE_OPTION_START;

   (void)label;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, idx);
         break;

      case MENU_ACTION_RIGHT:
         core_option_next(g_extern.system.core_options, idx);
         break;
   }

   return 0;
}

static int disk_options_disk_idx_toggle(unsigned type, const char *label,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         rarch_main_command(RARCH_CMD_DISK_PREV);
         break;
      case MENU_ACTION_RIGHT:
         rarch_main_command(RARCH_CMD_DISK_NEXT);
         break;
   }

   return 0;
}

static int deferred_push_core_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   size_t list_size = 0;
   const core_info_t *info = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   core_info_list_get_supported_cores(g_extern.core_info,
         driver.menu->deferred_path, &info, &list_size);

   for (i = 0; i < list_size; i++)
   {
      menu_list_push(list, info[i].path, "",
            MENU_FILE_CORE, 0);
      menu_list_set_alt_at_offset(list, i,
            info[i].display_name);
   }

   menu_list_sort_on_alt(list);

   menu_list_populate_generic(driver.menu, list, path, label, type);

   return 0;
}

static int deferred_push_database_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   menu_database_populate_query(list, path, NULL);

   menu_list_sort_on_alt(list);

   menu_list_populate_generic(driver.menu, list, path, label, type);

   return 0;
}

static int deferred_push_cursor_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   char *query = NULL, *rdb = NULL;
   char rdb_path[PATH_MAX_LENGTH];
   config_file_t *conf    = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   conf = config_file_new(path);

   if (!conf)
      return -1;

   if (!config_get_string(conf, "query", &query))
      return -1;

   if (!config_get_string(conf, "rdb", &rdb))
      return -1;

   fill_pathname_join(rdb_path, g_settings.content_database,
         rdb, sizeof(rdb_path));

   menu_database_populate_query(list, rdb_path, query);

   menu_list_sort_on_alt(list);

   menu_list_populate_generic(driver.menu, list, path, label, type);

   return 0;
}

static int deferred_push_cursor_manager_list_deferred_query_subsearch(
      void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   char query[PATH_MAX_LENGTH];
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;
   struct string_list *str_list  = string_split(path, "|"); 
   bool add_quotes = true;

   if (!list || !menu_list)
   {
      string_list_free(str_list);
      return -1;
   }

   strlcpy(query, "{'", sizeof(query));

   if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
      strlcat(query, "publisher", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
      strlcat(query, "developer", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
      strlcat(query, "origin", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
      strlcat(query, "franchise", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
      strlcat(query, "esrb_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
      strlcat(query, "bbfc_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
      strlcat(query, "elspa_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
      strlcat(query, "pegi_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw"))
      strlcat(query, "enhancement_hw", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
      strlcat(query, "cero_rating", sizeof(query));
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
   {
      strlcat(query, "edge_rating", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
   {
      strlcat(query, "edge_issue", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
   {
      strlcat(query, "releasemonth", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
   {
      strlcat(query, "releaseyear", sizeof(query));
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
   {
      strlcat(query, "users", sizeof(query));
      add_quotes = false;
   }

   strlcat(query, "':", sizeof(query));
   if (add_quotes)
      strlcat(query, "\"", sizeof(query));
   strlcat(query, str_list->elems[0].data, sizeof(query));
   if (add_quotes)
      strlcat(query, "\"", sizeof(query));
   strlcat(query, "}", sizeof(query));

#if 0
   RARCH_LOG("query: %s\n", query);
#endif

   if (query[0] == '\0')
   {
      string_list_free(str_list);
      return -1;
   }

   menu_list_clear(list);

   menu_database_populate_query(list, str_list->elems[1].data, query);

   menu_list_sort_on_alt(list);

   menu_list_populate_generic(driver.menu, list, str_list->elems[0].data, label, type);

   string_list_free(str_list);

   return 0;
}

#if 0
static int deferred_push_core_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   core_info_t *info      = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   info = (core_info_t*)g_extern.core_info_current;
   menu_list_clear(list);

   if (info->data)
   {
      char tmp[PATH_MAX_LENGTH];

      snprintf(tmp, sizeof(tmp), "Core name: %s",
            info->core_name ? info->core_name : "");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      snprintf(tmp, sizeof(tmp), "Core label: %s",
            info->display_name ? info->display_name : "");
      menu_list_push(list, tmp, "",
            MENU_SETTINGS_CORE_INFO_NONE, 0);

      if (info->systemname)
      {
         snprintf(tmp, sizeof(tmp), "System name: %s",
               info->systemname);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->system_manufacturer)
      {
         snprintf(tmp, sizeof(tmp), "System manufacturer: %s",
               info->system_manufacturer);
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->categories_list)
      {
         strlcpy(tmp, "Categories: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->categories_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->authors_list)
      {
         strlcpy(tmp, "Authors: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->authors_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->permissions_list)
      {
         strlcpy(tmp, "Permissions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->permissions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->licenses_list)
      {
         strlcpy(tmp, "License(s): ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->licenses_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->supported_extensions_list)
      {
         strlcpy(tmp, "Supported extensions: ", sizeof(tmp));
         string_list_join_concat(tmp, sizeof(tmp),
               info->supported_extensions_list, ", ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
      }

      if (info->firmware_count > 0)
      {
         core_info_list_update_missing_firmware(
               g_extern.core_info, info->path,
               g_settings.system_directory);

         menu_list_push(list, "Firmware: ", "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);
         for (i = 0; i < info->firmware_count; i++)
         {
            if (info->firmware[i].desc)
            {
               snprintf(tmp, sizeof(tmp), "	name: %s",
                     info->firmware[i].desc ? info->firmware[i].desc : "");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);

               snprintf(tmp, sizeof(tmp), "	status: %s, %s",
                     info->firmware[i].missing ?
                     "missing" : "present",
                     info->firmware[i].optional ?
                     "optional" : "required");
               menu_list_push(list, tmp, "",
                     MENU_SETTINGS_CORE_INFO_NONE, 0);
            }
         }
      }

      if (info->notes)
      {
         snprintf(tmp, sizeof(tmp), "Core notes: ");
         menu_list_push(list, tmp, "",
               MENU_SETTINGS_CORE_INFO_NONE, 0);

         for (i = 0; i < info->note_list->size; i++)
         {
            snprintf(tmp, sizeof(tmp), " %s",
                  info->note_list->elems[i].data);
            menu_list_push(list, tmp, "",
                  MENU_SETTINGS_CORE_INFO_NONE, 0);
         }
      }
   }
   else
      menu_list_push(list,
            "No information available.", "",
            MENU_SETTINGS_CORE_OPTION_NONE, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}
#endif

static int deferred_push_performance_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Frontend Counters", "frontend_counters",
         MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Core Counters", "core_counters",
         MENU_SETTING_ACTION, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static inline struct video_shader *shader_manager_get_current_shader(
      menu_handle_t *menu, const char *label, unsigned type)
{
   if (!strcmp(label, "video_shader_preset_parameters"))
      return menu->shader;
   else if (!strcmp(label, "video_shader_parameters") &&
         driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      return driver.video_poke->get_current_shader(driver.video_data);
   return NULL;
}

static int deferred_push_video_shader_parameters_common(void *data, void *userdata,
      const char *path, const char *label, unsigned type,
      struct video_shader *shader, unsigned base_parameter)
{
   unsigned i;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   for (i = 0; i < shader->num_parameters; i++)
   {
      menu_list_push(list,
            shader->parameters[i].desc, label,
            base_parameter + i, 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_video_shader_preset_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   if (!driver.menu->shader)
      return 0;

   return deferred_push_video_shader_parameters_common(data, userdata,
         path, label, type,
         driver.menu->shader, MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}

static int deferred_push_video_shader_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   struct video_shader *shader = NULL;
   if (driver.video_poke && driver.video_data && driver.video_poke->get_current_shader)
      shader = driver.video_poke->get_current_shader(driver.video_data);

   if (!shader)
      return 0;

   return deferred_push_video_shader_parameters_common(data, userdata,
         path, label, type,
         shader, MENU_SETTINGS_SHADER_PARAMETER_0);
}

static int deferred_push_settings(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   rarch_setting_t *setting = NULL;
   file_list_t *list        = (file_list_t*)data;
   file_list_t *menu_list   = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   settings_list_free(driver.menu->list_settings);
   driver.menu->list_settings = (rarch_setting_t *)setting_data_new(SL_FLAG_ALL_SETTINGS);

   setting = menu_action_find_setting("Driver Options");

   menu_list_clear(list);

   if (g_settings.menu.collapse_subgroups_enable)
   {
      for (; setting->type != ST_NONE; setting++)
      {
         if (setting->type == ST_GROUP)
            menu_list_push(list, setting->short_description,
                  setting->name, menu_entries_setting_set_flags(setting), 0);
      }
   }
   else
   {
      for (; setting->type != ST_NONE; setting++)
      {
         char group_label[PATH_MAX_LENGTH];
         char subgroup_label[PATH_MAX_LENGTH];

         if (setting->type == ST_GROUP)
            strlcpy(group_label, setting->name, sizeof(group_label));
         else if (setting->type == ST_SUB_GROUP)
         {
            char new_label[PATH_MAX_LENGTH], new_path[PATH_MAX_LENGTH];
            strlcpy(subgroup_label, setting->name, sizeof(group_label));
            strlcpy(new_label, group_label, sizeof(new_label));
            strlcat(new_label, "|", sizeof(new_label));
            strlcat(new_label, subgroup_label, sizeof(new_label));

            strlcpy(new_path, group_label, sizeof(new_path));
            strlcat(new_path, " - ", sizeof(new_path));
            strlcat(new_path, setting->short_description, sizeof(new_path));

            menu_list_push(list, new_path,
                  new_label, MENU_SETTING_SUBGROUP, 0);
         }
      }
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_settings_subgroup(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   rarch_setting_t *setting = NULL;
   struct string_list *str_list = NULL;
   file_list_t *list        = (file_list_t*)data;
   file_list_t *menu_list   = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   if (label)
   {
      str_list = string_split(label, "|");

      if (str_list && str_list->size > 0)
         strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
      if (str_list && str_list->size > 1)
         strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

      if (str_list)
      {
         string_list_free(str_list);
         str_list = NULL;
      }
   }

   settings_list_free(driver.menu->list_settings);
   driver.menu->list_settings = (rarch_setting_t *)setting_data_new(SL_FLAG_ALL_SETTINGS);

   setting = menu_action_find_setting(elem0);

   menu_list_clear(list);

   if (!setting)
      return -1;

   while (1)
   {
      if (!setting)
         return -1;
      if (setting->type == ST_SUB_GROUP)
      {
         if ((strlen(setting->name) != 0) && !strcmp(setting->name, elem1))
            break;
      }
      setting++;
   }

   setting++;

   for (; setting->type != ST_END_SUB_GROUP; setting++)
   {
      char group_label[PATH_MAX_LENGTH];

      strlcpy(group_label, setting->name, sizeof(group_label));
      menu_list_push(list, setting->short_description,
            group_label, menu_entries_setting_set_flags(setting), 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_category(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_push_list(driver.menu, (file_list_t*)data,
         path, label, type, SL_FLAG_ALL_SETTINGS);
}

static int deferred_push_shader_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   struct video_shader *shader = NULL;
   file_list_t *list           = (file_list_t*)data;
   file_list_t *menu_list      = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   shader = driver.menu->shader;

   if (!shader)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Apply Shader Changes", "shader_apply_changes",
         MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Default Filter", "video_shader_default_filter",
         0, 0);
   menu_list_push(list, "Load Shader Preset", "video_shader_preset",
         MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Shader Preset Save As",
         "video_shader_preset_save_as", MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Parameters (Current)",
         "video_shader_parameters", MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Parameters (Menu)",
         "video_shader_preset_parameters", MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Shader Passes", "video_shader_num_passes",
         0, 0);

   for (i = 0; i < shader->passes; i++)
   {
      char buf[64];

      snprintf(buf, sizeof(buf), "Shader #%u", i);
      menu_list_push(list, buf, "video_shader_pass",
            MENU_SETTINGS_SHADER_PASS_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
      menu_list_push(list, buf, "video_shader_filter_pass",
            MENU_SETTINGS_SHADER_PASS_FILTER_0 + i, 0);

      snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
      menu_list_push(list, buf, "video_shader_scale_pass",
            MENU_SETTINGS_SHADER_PASS_SCALE_0 + i, 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}


static void push_perfcounter(menu_handle_t *menu,
      file_list_t *list,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned id)
{
   unsigned i;
   if (!counters || num == 0)
      return;

   for (i = 0; i < num; i++)
      if (counters[i] && counters[i]->ident)
         menu_list_push(list, counters[i]->ident, "",
               id + i, 0);
}

static int push_perfcounter_generic(
      void *data,
      void *userdata,
      const char *path, const char *label,
      const struct retro_perf_counter **counters,
      unsigned num, unsigned ident,
      unsigned type)
{
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);
   push_perfcounter(driver.menu, list, counters, num, ident);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return push_perfcounter_generic(data, userdata, path, label,
         perf_counters_libretro, perf_ptr_libretro, 
         MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN, type);
}

static int deferred_push_frontend_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return push_perfcounter_generic(data, userdata, path, label,
         perf_counters_rarch, perf_ptr_rarch, 
         MENU_SETTINGS_PERF_COUNTERS_BEGIN, type);
}

static int deferred_push_core_cheat_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   file_list_t *list      = (file_list_t*)data;
   cheat_manager_t *cheat = g_extern.cheat;

   (void)userdata;
   (void)type;

   if (!list)
      return -1;

   if (!cheat)
   {
      g_extern.cheat = cheat_manager_new(0);

      if (!g_extern.cheat)
         return -1;
      cheat = g_extern.cheat;
   }

   menu_list_clear(list);
   menu_list_push(list, "Cheat File Load", "cheat_file_load",
         MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Cheat File Save As",
         "cheat_file_save_as", MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Cheat Passes", "cheat_num_passes",
         0, 0);
   menu_list_push(list, "Apply Cheat Changes", "cheat_apply_changes",
         MENU_SETTING_ACTION, 0);

   for (i = 0; i < cheat->size; i++)
   {
      char cheat_label[64];
      snprintf(cheat_label, sizeof(cheat_label), "Cheat #%d: ", i);
      if (cheat->cheats[i].desc)
         strlcat(cheat_label, cheat->cheats[i].desc, sizeof(cheat_label));
      menu_list_push(list, cheat_label, "", MENU_SETTINGS_CHEAT_BEGIN + i, 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_input_remapping_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned p, retro_id;
   file_list_t *list      = (file_list_t*)data;

   (void)userdata;
   (void)type;

   if (!list)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Remap File Load", "remap_file_load",
         MENU_SETTING_ACTION, 0);
   menu_list_push(list, "Remap File Save As",
         "remap_file_save_as", MENU_SETTING_ACTION, 0);

   for (p = 0; p < g_settings.input.max_users; p++)
   {
      for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
      {
         char desc_label[64];
         unsigned user = p + 1;
         const char *description = g_extern.system.input_desc_btn[p][retro_id];

         if (!description)
            continue;

         snprintf(desc_label, sizeof(desc_label),
               "User %u %s : ", user, description);
         menu_list_push(list, desc_label, "",
               MENU_SETTINGS_INPUT_DESC_BEGIN + 
               (p * RARCH_FIRST_CUSTOM_BIND) +  retro_id, 0);
      }
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_core_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   file_list_t *list      = (file_list_t*)data;

   (void)userdata;

   if (!list)
      return -1;

   menu_list_clear(list);

   if (g_extern.system.core_options)
   {
      size_t opts = core_option_size(g_extern.system.core_options);

      for (i = 0; i < opts; i++)
         menu_list_push(list,
               core_option_get_desc(g_extern.system.core_options, i), "",
               MENU_SETTINGS_CORE_OPTION_START + i, 0);
   }
   else
      menu_list_push(list, "No options available.", "",
               MENU_SETTINGS_CORE_OPTION_NONE, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

static int deferred_push_disk_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list      = (file_list_t*)data;

   (void)userdata;

   if (!list)
      return -1;

   menu_list_clear(list);
   menu_list_push(list, "Disk Index", "disk_idx",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX, 0);
   menu_list_push(list, "Disk Cycle Tray Status", "disk_cycle_tray_status",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS, 0);
   menu_list_push(list, "Disk Image Append", "disk_image_append",
         MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_IMAGE_APPEND, 0);

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(driver.menu, path, label, type);

   return 0;
}

#ifdef HAVE_NETWORKING
static void print_buf_lines(file_list_t *list, char *buf, int buf_size,
      unsigned type)
{
   int i;
   char c, *line_start = buf;

   for (i = 0; i < buf_size; i++)
   {
      size_t ln;

      /* The end of the buffer, print the last bit */
      if (*(buf + i) == '\0')
         break;

      if (*(buf + i) != '\n')
         continue;

      /* Found a line ending, print the line and compute new line_start */

      /* Save the next char  */
      c = *(buf + i + 1);
      /* replace with \0 */
      *(buf + i + 1) = '\0';

      /* We need to strip the newline. */
      ln = strlen(line_start) - 1;
      if (line_start[ln] == '\n')
         line_start[ln] = '\0';

      menu_list_push(list, line_start, "",
            type, 0);

      /* Restore the saved char */
      *(buf + i + 1) = c;
      line_start = buf + i + 1;
   }
   /* If the buffer was completely full, and didn't end with a newline, just
    * ignore the partial last line.
    */
}

/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
static char core_updater_list_path[PATH_MAX_LENGTH];
static char core_updater_list_label[PATH_MAX_LENGTH];
static unsigned core_updater_list_type;

static int cb_core_updater_list(void *data_, size_t len)
{
   char *data = (char*)data_;
   file_list_t *list = NULL;

   if (!data)
      return -1;

   list      = (file_list_t*)driver.menu->menu_list->selection_buf;

   if (!list)
      return -1;

   menu_list_clear(list);


   print_buf_lines(list, data, len, MENU_FILE_DOWNLOAD_CORE);

   menu_list_populate_generic(driver.menu,
         list, core_updater_list_path,
         core_updater_list_label, core_updater_list_type);

   return 0;
}
#endif


static int deferred_push_core_updater_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
#ifdef HAVE_NETWORKING
   char url_path[PATH_MAX_LENGTH];

   strlcpy(core_updater_list_path, path, sizeof(core_updater_list_path));
   strlcpy(core_updater_list_label, label, sizeof(core_updater_list_label));
   core_updater_list_type = type;
#endif

   if (g_settings.network.buildbot_url[0] == '\0')
   {
      file_list_t *list      = (file_list_t*)data;

      menu_list_clear(list);
#ifdef HAVE_NETWORKING
      menu_list_push(list,
            "Buildbot URL not configured.", "",
            0, 0);
#else
      menu_list_push(list,
            "Network not available.", "",
            0, 0);
#endif

      menu_list_populate_generic(driver.menu, list, path, label, type);

      return 0;
   }

#ifdef HAVE_NETWORKING
   rarch_main_command(RARCH_CMD_NETWORK_INIT);

   fill_pathname_join(url_path, g_settings.network.buildbot_url,
         ".index", sizeof(url_path));

   msg_queue_clear(g_extern.http_msg_queue);
   msg_queue_push(g_extern.http_msg_queue, url_path, 0, 1);

   net_http_set_pending_cb(cb_core_updater_list);
#endif

   return 0;
}

static int deferred_push_history_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   unsigned i;
   size_t list_size = 0;
   file_list_t *list      = (file_list_t*)data;

   (void)userdata;

   if (!list || !driver.menu)
      return -1;

   menu_list_clear(list);
   list_size = content_playlist_size(g_defaults.history);

   for (i = 0; i < list_size; i++)
   {
      char fill_buf[PATH_MAX_LENGTH];
      const char *core_name = NULL;

      content_playlist_get_index(g_defaults.history, i,
            &path, NULL, &core_name);
      strlcpy(fill_buf, core_name, sizeof(fill_buf));

      if (path)
      {
         char path_short[PATH_MAX_LENGTH];
         fill_short_pathname_representation(path_short,path,sizeof(path_short));
         snprintf(fill_buf,sizeof(fill_buf),"%s (%s)",
               path_short,core_name);
      }

      menu_list_push(list, fill_buf, "",
            MENU_FILE_PLAYLIST_ENTRY, 0);
   }

   menu_list_populate_generic(driver.menu, list, path, label, type);

   return 0;
}

static int deferred_push_content_actions(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   file_list_t *list = (file_list_t*)data;

   (void)userdata;

   if (!list || !driver.menu)
      return -1;

   menu_list_clear(list);


   if (g_extern.main_is_init && !g_extern.libretro_dummy &&
         !strcmp(driver.menu->deferred_path, g_extern.fullpath))
   {
      menu_list_push(list, "Resume", "file_load_or_resume", MENU_SETTING_ACTION_RUN, 0);
      menu_list_push(list, "Core Informations", "core_information", MENU_SETTING_ACTION_CORE_INFORMATION, 0);
      menu_list_push(list, "Core Options", "core_options", MENU_SETTING_ACTION_CORE_OPTIONS, 0);
      if (g_extern.has_set_input_descriptors)
         menu_list_push(list, "Core Input Remapping Options", "core_input_remapping_options", MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS, 0);
      menu_list_push(list, "Core Cheat Options", "core_cheat_options", MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS, 0);
      if ( !g_extern.libretro_dummy && g_extern.system.disk_control.get_num_images)
         menu_list_push(list, "Core Disk Options", "disk_options", MENU_SETTING_ACTION_CORE_DISK_OPTIONS, 0);
      menu_list_push(list, "Save State", "savestate", MENU_SETTING_ACTION_SAVESTATE, 0);
      menu_list_push(list, "Load State", "loadstate", MENU_SETTING_ACTION_LOADSTATE, 0);
      menu_list_push(list, "Take Screenshot", "take_screenshot", MENU_SETTING_ACTION_SCREENSHOT, 0);
      menu_list_push(list, "Reset", "restart_content", MENU_SETTING_ACTION_RESET, 0);
   }
   else
      menu_list_push(list, "Run", "file_load_or_resume", MENU_SETTING_ACTION_RUN, 0);

   menu_list_populate_generic(driver.menu, list, path, label, type);

   return 0;
}

static int deferred_push_content_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_deferred_push((file_list_t*)data, driver.menu->menu_list->selection_buf);
}

static int deferred_push_database_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, g_settings.content_database, label, type,
         MENU_FILE_RDB, "rdb", NULL);
}

static int deferred_push_cursor_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, g_settings.cursor_directory, label, type,
         MENU_FILE_CURSOR, "dbc", NULL);
}

static int deferred_push_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_PLAIN, EXT_EXECUTABLES, NULL);
}

static int deferred_push_configurations(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_CONFIG, "cfg", NULL);
}

static int deferred_push_video_shader_preset(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_SHADER_PRESET, "cgp|glslp", NULL);
}

static int deferred_push_video_shader_pass(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_SHADER, "cg|glsl", NULL);
}

static int deferred_push_video_filter(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_VIDEOFILTER, "filt", NULL);
}

static int deferred_push_images(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_IMAGE, "png", NULL);
}

static int deferred_push_audio_dsp_plugin(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_AUDIOFILTER, "dsp", NULL);
}

static int deferred_push_cheat_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_CHEAT, "cht", NULL);
}

static int deferred_push_remap_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_REMAP, "rmp", NULL);
}

static int deferred_push_input_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_OVERLAY, "cfg", NULL);
}

static int deferred_push_input_osk_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_OVERLAY, "cfg", NULL);
}

static int deferred_push_video_font_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_FONT, "ttf", NULL);
}

static int deferred_push_content_history_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_PLAIN, "cfg", NULL);
}

static int deferred_push_detect_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   return menu_entries_parse_list((file_list_t*)data, (file_list_t*)userdata, path, label, type,
         MENU_FILE_PLAIN, 
         g_extern.core_info ? core_info_list_get_all_extensions(
         g_extern.core_info) : "", NULL);
}

static int deferred_push_default(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   char ext_buf[PATH_MAX_LENGTH];
   const char *exts = NULL;
   file_list_t *list      = (file_list_t*)data;
   file_list_t *menu_list = (file_list_t*)userdata;
   rarch_setting_t *setting = (rarch_setting_t*)
      menu_action_find_setting(label);

   if (!list || !menu_list)
      return -1;

   if (setting && setting->browser_selection_type == ST_DIR)
      exts = ""; /* we ignore files anyway */
   else if (g_extern.menu.info.valid_extensions)
   {
      exts = ext_buf;
      if (*g_extern.menu.info.valid_extensions)
         snprintf(ext_buf, sizeof(ext_buf), "%s",
               g_extern.menu.info.valid_extensions);
      else
         *ext_buf = '\0';
   }
   else
      exts = g_extern.system.valid_extensions;

   menu_entries_parse_list(list, menu_list, path, label,
         type, MENU_FILE_PLAIN, exts, setting);

   return 0;
}

static int action_bind_up_or_down_generic(unsigned type, const char *label,
      unsigned action)
{
   unsigned scroll_speed = (max(driver.menu->scroll_accel, 2) - 2) / 4 + 1;

   if (menu_list_get_size(driver.menu->menu_list) <= 0)
      return 0;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (driver.menu->selection_ptr >= scroll_speed)
               menu_navigation_set(driver.menu,
                     driver.menu->selection_ptr - scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_set(driver.menu,
                     menu_list_get_size(driver.menu->menu_list) - 1, true);
            else
               menu_navigation_set(driver.menu,
                     0, true);
         }
         break;
      case MENU_ACTION_DOWN:
         if (driver.menu->selection_ptr + scroll_speed < (menu_list_get_size(driver.menu->menu_list)))
            menu_navigation_set(driver.menu,
                  driver.menu->selection_ptr + scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_clear(driver.menu, false);
            else
               menu_navigation_set(driver.menu,
                     menu_list_get_size(driver.menu->menu_list) - 1, true);
         }
         break;
   }

   return 0;
}

static int action_refresh_default(file_list_t *list, file_list_t *menu_list)
{
   int ret = menu_entries_deferred_push(list, menu_list);
   driver.menu->need_refresh = false;
   return ret;
}

static int mouse_post_iterate(menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, unsigned action)
{
   if (!driver.menu->mouse.enable)
      return 0;

   if (driver.menu->mouse.ptr <= menu_list_get_size(driver.menu->menu_list)-1)
      menu_navigation_set(driver.menu, driver.menu->mouse.ptr, false);

   if (driver.menu->mouse.left)
   {
      if (!driver.menu->mouse.oldleft)
      {
         driver.menu->mouse.oldleft = true;

         if (cbs && cbs->action_ok)
            return cbs->action_ok(path, label, type, driver.menu->selection_ptr);
      }
   }
   else
      driver.menu->mouse.oldleft = false;

   if (driver.menu->mouse.right)
   {
      if (!driver.menu->mouse.oldright)
      {
         driver.menu->mouse.oldright = true;
         menu_list_pop_stack(driver.menu->menu_list);
      }
   }
   else
      driver.menu->mouse.oldright = false;

   return 0;
}

static int action_iterate_help(const char *label, unsigned action)
{
   unsigned i;
   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RARCH_MENU_TOGGLE,
      RARCH_QUIT_KEY,
   };
   char desc[ARRAY_SIZE(binds)][64];
   char msg[PATH_MAX_LENGTH];

   if (!driver.menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      const struct retro_keybind *keybind = (const struct retro_keybind*)
         &g_settings.input.binds[0][binds[i]];
      const struct retro_keybind *auto_bind = (const struct retro_keybind*)
         input_get_auto_bind(0, binds[i]);

      input_get_bind_string(desc[i], keybind, auto_bind, sizeof(desc[i]));
   }

   snprintf(msg, sizeof(msg),
         "-- Welcome to RetroArch --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic Menu controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "           Info: %-20s\n"
         "Enter/Exit Menu: %-20s\n"
         " Exit RetroArch: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Core).\n"
         "Load a content file (Load Content).     \n"
         " \n"
         "See Path Options to set directories for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
      desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6]);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      menu_list_pop(driver.menu->menu_list->menu_stack, NULL);

   return 0;
}

static int action_iterate_info(const char *label, unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   char needle[PATH_MAX_LENGTH];
   unsigned info_type = 0;
   rarch_setting_t *current_setting = NULL;
   file_list_t *list = (file_list_t*)driver.menu->menu_list->selection_buf;

   if (!driver.menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   current_setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_settings,
         list->list[driver.menu->selection_ptr].label);

   if (current_setting)
      strlcpy(needle, current_setting->name, sizeof(needle));
   else if ((current_setting = (rarch_setting_t*)setting_data_find_setting(
               driver.menu->list_settings,
               list->list[driver.menu->selection_ptr].label)))
   {
      if (current_setting)
         strlcpy(needle, current_setting->name, sizeof(needle));
   }
   else
   {
      const char *lbl = NULL;
      menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr, NULL, &lbl,
            &info_type);

      if (lbl)
         strlcpy(needle, lbl, sizeof(needle));
   }

   setting_data_get_description(needle, msg, sizeof(msg));

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

   if (action == MENU_ACTION_OK)
      menu_list_pop(driver.menu->menu_list->menu_stack, &driver.menu->selection_ptr);

   return 0;
}

static int action_iterate_load_open_zip(const char *label, unsigned action)
{
   switch (g_settings.archive.mode)
   {
      case 0:
         return load_or_open_zip_iterate(action);
      case 1:
         return archive_load();
      case 2:
         return archive_open();
      default:
         break;
   }

   return 0;
}

static int action_iterate_menu_viewport(const char *label, unsigned action)
{
   int stride_x = 1, stride_y = 1;
   char msg[PATH_MAX_LENGTH];
   struct retro_game_geometry *geom = NULL;
   const char *base_msg = NULL;
   unsigned type = 0;
   rarch_viewport_t *custom = (rarch_viewport_t*)
      &g_extern.console.screen.viewports.custom_vp;

   menu_list_get_last_stack(driver.menu->menu_list, NULL, NULL, &type);

   geom = (struct retro_game_geometry*)&g_extern.system.av_info.geometry;

   if (g_settings.video.scale_integer)
   {
      stride_x = geom->base_width;
      stride_y = geom->base_height;
   }

   switch (action)
   {
      case MENU_ACTION_UP:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_DOWN:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y += stride_y;
            if (custom->height >= (unsigned)stride_y)
               custom->height -= stride_y;
         }
         else
            custom->height += stride_y;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_LEFT:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_RIGHT:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x += stride_x;
            if (custom->width >= (unsigned)stride_x)
               custom->width -= stride_x;
         }
         else
            custom->width += stride_x;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_CANCEL:
         menu_list_pop_stack(driver.menu->menu_list);

         if (!strcmp(label, "custom_viewport_2"))
         {
            menu_list_push_stack(driver.menu->menu_list, "", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  driver.menu->selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(driver.menu->menu_list);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            menu_list_push_stack(driver.menu->menu_list, "",
                  "custom_viewport_2", 0, driver.menu->selection_ptr);
         }
         break;

      case MENU_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            rarch_viewport_t vp;

            if (driver.video_data && driver.video &&
                  driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, &vp);

            if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
            {
               custom->width += custom->x;
               custom->height += custom->y;
               custom->x = 0;
               custom->y = 0;
            }
            else
            {
               custom->width = vp.full_width - custom->x;
               custom->height = vp.full_height - custom->y;
            }

            rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         }
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, &type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   if (g_settings.video.scale_integer)
   {
      custom->x = 0;
      custom->y = 0;
      custom->width = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;

      base_msg = "Set scale";
      snprintf(msg, sizeof(msg), "%s (%4ux%4u, %u x %u scale)",
            base_msg,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height);
   }
   else
   {
      if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (!strcmp(label, "custom_viewport_2"))
         base_msg = "Set Bottom-Right Corner";

      snprintf(msg, sizeof(msg), "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height);
   }

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

static int action_iterate_custom_bind(const char *label, unsigned action)
{
   if (menu_input_bind_iterate(driver.menu))
      menu_list_pop_stack(driver.menu->menu_list);
   return 0;
}

static int action_iterate_custom_bind_keyboard(const char *label, unsigned action)
{
   if (menu_input_bind_iterate_keyboard(driver.menu))
      menu_list_pop_stack(driver.menu->menu_list);
   return 0;
}

static int action_iterate_message(const char *label, unsigned action)
{
   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(driver.menu->message_contents);

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(driver.menu->menu_list);

   return 0;
}

static int mouse_iterate(unsigned action)
{
   const struct retro_keybind *binds[MAX_USERS];

   if (!driver.menu->mouse.enable)
      return 0;

   driver.menu->mouse.dx = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   driver.menu->mouse.dy = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

   driver.menu->mouse.x += driver.menu->mouse.dx;
   driver.menu->mouse.y += driver.menu->mouse.dy;

   if (driver.menu->mouse.x < 5)
      driver.menu->mouse.x = 5;
   if (driver.menu->mouse.y < 5)
      driver.menu->mouse.y = 5;
   if (driver.menu->mouse.x > driver.menu->width - 5)
      driver.menu->mouse.x = driver.menu->width - 5;
   if (driver.menu->mouse.y > driver.menu->height - 5)
      driver.menu->mouse.y = driver.menu->height - 5;

   driver.menu->mouse.left = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

   driver.menu->mouse.right = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

   driver.menu->mouse.wheelup = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP)
         || driver.menu->mouse.y == 5;

   driver.menu->mouse.wheeldown = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
         || driver.menu->mouse.y == driver.menu->height - 5;

   return 0;
}

static int action_iterate_main(const char *label, unsigned action)
{
   int ret = 0;
   unsigned type_offset = 0;
   const char *label_offset = NULL;
   const char *path_offset = NULL;

   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
         driver.menu->selection_ptr, &path_offset, &label_offset, &type_offset);

   mouse_iterate(action);

   if (!strcmp(label, "help"))
      return action_iterate_help(label, action);
   else if (!strcmp(label, "info_screen"))
      return action_iterate_info(label, action);
   else if (!strcmp(label, "load_open_zip"))
      return action_iterate_load_open_zip(label, action);
   else if (!strcmp(label, "message"))
      return action_iterate_message(label, action);
   else if (
         !strcmp(label, "custom_viewport_1") ||
         !strcmp(label, "custom_viewport_2")
         )
      return action_iterate_menu_viewport(label, action);
   else if (
         !strcmp(label, "custom_bind") ||
         !strcmp(label, "custom_bind_all") ||
         !strcmp(label, "custom_bind_defaults")
         )
   {
      if (g_extern.menu.bind_mode_keyboard)
         return action_iterate_custom_bind_keyboard(label, action);
      else
         return action_iterate_custom_bind(label, action);
   }

   if (driver.menu->need_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_REFRESH;

   switch (action)
   {
      case MENU_ACTION_UP:
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_up_or_down)
            ret = cbs->action_up_or_down(type_offset, label_offset, action);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_navigation_descend_alphabet(driver.menu, &driver.menu->selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ascend_alphabet(driver.menu, &driver.menu->selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            return cbs->action_cancel(path_offset, label_offset, type_offset, driver.menu->selection_ptr);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            return cbs->action_ok(path_offset, label_offset, type_offset, driver.menu->selection_ptr);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            return cbs->action_start(type_offset, label_offset, action);
         break;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_toggle)
            ret = cbs->action_toggle(type_offset, label_offset, action);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(type_offset, label_offset, action);
         break;

      case MENU_ACTION_REFRESH:
         if (cbs && cbs->action_refresh)
            ret = cbs->action_refresh(driver.menu->menu_list->selection_buf,
                  driver.menu->menu_list->menu_stack);
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
         break;

      case MENU_ACTION_SEARCH:
         menu_input_search_start();
         break;

      case MENU_ACTION_TEST:
         break;

      default:
         break;
   }

   if (ret)
      return ret;

   ret = mouse_post_iterate(cbs, path_offset, label_offset, type_offset, action);

   if (driver.menu_ctx && driver.menu_ctx->iterate)
      driver.menu_ctx->iterate(driver.menu, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   /* Have to defer it so we let settings refresh. */
   if (driver.menu->push_start_screen)
   {
      menu_list_push_stack(driver.menu->menu_list, "", "help", 0, 0);
      driver.menu->push_start_screen = false;
   }

   return ret;
}

static int action_select_default(unsigned type, const char *label,
      unsigned action)
{
   menu_list_push_stack(driver.menu->menu_list, "", "info_screen",
         0, driver.menu->selection_ptr);
   return 0;
}

static int action_start_lookup_setting(unsigned type, const char *label,
      unsigned action)
{
   return menu_action_setting_set(type, label, MENU_ACTION_START);
}

static void menu_action_setting_disp_set_label_cheat_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   snprintf(type_str, type_str_size, "%u", g_extern.cheat->buf_size);
}

static void menu_action_setting_disp_set_label_remap_file_load(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   fill_pathname_base(type_str, g_settings.input.remapping_path,
         type_str_size);
}

static void menu_action_setting_disp_set_label_configurations(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   if (*g_extern.config_path)
      fill_pathname_base(type_str, g_extern.config_path,
            type_str_size);
   else
      strlcpy(type_str, "<default>", type_str_size);
}

static void menu_action_setting_disp_set_label_shader_filter_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass;
   static const char *modes[] = {
      "Don't care",
      "Linear",
      "Nearest"
   };

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!driver.menu->shader)
      return;

  pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);

  strlcpy(type_str, modes[driver.menu->shader->pass[pass].filter],
        type_str_size);
#endif
}

static void menu_action_setting_disp_set_label_shader_num_passes(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   snprintf(type_str, type_str_size, "%u", driver.menu->shader->passes);
#endif
}

static void menu_action_setting_disp_set_label_shader_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_0);

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   strlcpy(type_str, "N/A", type_str_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (*driver.menu->shader->pass[pass].source.path)
      fill_pathname_base(type_str,
            driver.menu->shader->pass[pass].source.path, type_str_size);
#endif
}

static void menu_action_setting_disp_set_label_shader_default_filter(

      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 19;
   snprintf(type_str, type_str_size, "%s",
         g_settings.video.smooth ? "Linear" : "Nearest");
}

static void menu_action_setting_disp_set_label_shader_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
   struct video_shader *shader = NULL;
#endif

   if (!driver.video_poke)
      return;
   if (!driver.video_data)
      return;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!driver.video_poke->get_current_shader)
      return;

   shader = driver.video_poke->get_current_shader(driver.video_data);

   if (!shader)
      return;

   param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];

   if (!param)
      return;

   snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_preset_parameter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   const struct video_shader_parameter *param = NULL;
#endif

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!driver.menu->shader)
      return;

   param = &driver.menu->shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];

   if (!param)
      return;

   snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]",
         param->current, param->minimum, param->maximum);
#endif
}

static void menu_action_setting_disp_set_label_shader_scale_pass(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned pass, scale_value;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!driver.menu->shader)
      return;

   pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
   scale_value = driver.menu->shader->pass[pass].fbo.scale_x;

   if (!scale_value)
      strlcpy(type_str, "Don't care", type_str_size);
   else
      snprintf(type_str, type_str_size, "%ux", scale_value);
#endif
}

static void menu_action_setting_disp_set_label_menu_file_core(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const char *alt = NULL;
   strlcpy(type_str, "(CORE)", type_str_size);
   menu_list_get_alt_at_offset(list, i, &alt);
   *w = strlen(type_str);
   if (alt)
      strlcpy(path_buf, alt, path_buf_size);
}

static void menu_action_setting_disp_set_label_input_desc(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / 
      RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - 
      (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);
   unsigned remap_id = g_settings.input.remap_ids
      [inp_desc_user][inp_desc_button_index_offset];

   snprintf(type_str, type_str_size, "%s",
         g_settings.input.binds[inp_desc_user][remap_id].desc);
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned cheat_index = type - MENU_SETTINGS_CHEAT_BEGIN;

   if (cheat_index < g_extern.cheat->buf_size)
      snprintf(type_str, type_str_size, "%s : (%s)",
            (g_extern.cheat->cheats[cheat_index].code != NULL)
            ? g_extern.cheat->cheats[cheat_index].code : "N/A",
            g_extern.cheat->cheats[cheat_index].state ? "ON" : "OFF"
            );
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const struct retro_perf_counter **counters = 
      (const struct retro_perf_counter **)perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(type_str, type_str_size,
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);
}

static void menu_action_setting_disp_set_label_libretro_perf_counters(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const struct retro_perf_counter **counters = 
      (const struct retro_perf_counter **)perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   if (!counters[offset])
      return;
   if (!counters[offset]->call_cnt)
      return;

   snprintf(type_str, type_str_size,
#ifdef _WIN32
         "%I64u ticks, %I64u runs.",
#else
         "%llu ticks, %llu runs.",
#endif
         ((unsigned long long)counters[offset]->total /
          (unsigned long long)counters[offset]->call_cnt),
         (unsigned long long)counters[offset]->call_cnt);
}

static void menu_action_setting_disp_set_label_menu_more(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "...", type_str_size);
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_plain(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(FILE)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_disk_index(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   const struct retro_disk_control_callback *control =
      (const struct retro_disk_control_callback*)
      &g_extern.system.disk_control;
   unsigned images = 0, current = 0;

   *w = 19;
   *type_str = '\0';
   strlcpy(path_buf, path, path_buf_size);
   if (!control)
      return;

   images = control->get_num_images();
   current = control->get_image_index();

   if (current >= images)
      strlcpy(type_str, "No Disk", type_str_size);
   else
      snprintf(type_str, type_str_size, "%u", current + 1);
}

static void menu_action_setting_disp_set_label_menu_video_resolution(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   unsigned width = 0, height = 0;
   *w = 19;
   *type_str = '\0';

   (void)width;
   (void)height;

#if defined(GEKKO)
   snprintf(type_str, type_str_size, "%.3ux%.3u%c",
         menu_gx_resolutions[menu_current_gx_resolution][0],
         menu_gx_resolutions[menu_current_gx_resolution][1],
         menu_gx_resolutions[menu_current_gx_resolution][1] > 300 ? 'i' : 'p');
#elif defined(__CELLOS_LV2__)
   width = gfx_ctx_get_resolution_width(
         g_extern.console.screen.resolutions.list
         [g_extern.console.screen.resolutions.current.idx]);
   height = gfx_ctx_get_resolution_height(
         g_extern.console.screen.resolutions.list
         [g_extern.console.screen.resolutions.current.idx]);
   snprintf(type_str, type_str_size, "%ux%u", width, height);
#endif
}

static void menu_action_setting_disp_set_label_menu_file_use_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 0;
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_directory(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(DIR)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(COMP)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_shader(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(SHADER)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_subgroup(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_shader_preset(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(PRESET)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_in_carchive(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(CFILE)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_overlay(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(OVERLAY)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_config(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(CONFIG)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_font(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(FONT)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_filter(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(FILTER)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_url(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(URL)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_rdb(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(RDB)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_cursor(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(CURSOR)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label_menu_file_cheat(
      file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   strlcpy(type_str, "(CHEAT)", type_str_size);
   *w = strlen(type_str);
   strlcpy(path_buf, path, path_buf_size);
}

static void menu_action_setting_disp_set_label(file_list_t* list,
      unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 19;

   if (!strcmp(label, "performance_counters"))
      *w = strlen(label);

   if (!strcmp(label, "history_list"))
      *w = strlen(label);

   if (type >= MENU_SETTINGS_CORE_OPTION_START)
      strlcpy(
            type_str,
            core_option_get_val(g_extern.system.core_options,
               type - MENU_SETTINGS_CORE_OPTION_START),
            type_str_size);
   else
      setting_data_get_label(list, type_str,
            type_str_size, w, type, label, entry_label, i);

   strlcpy(path_buf, path, path_buf_size);
}

static void menu_entries_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_select = action_select_default;
}

static void menu_entries_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_start = action_start_lookup_setting;

   if (!strcmp(label, "remap_file_load"))
      cbs->action_start = action_start_remap_file_load;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_start = action_start_shader_pass;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_start = action_start_shader_scale_pass;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_start = action_start_shader_filter_pass;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_start = action_start_shader_num_passes;
   else if (!strcmp(label, "cheat_num_passes"))
      cbs->action_start = action_start_cheat_num_passes;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_parameter;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_start = action_start_shader_action_preset_parameter;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_core;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_start = action_start_input_desc;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_start = action_start_performance_counters_frontend;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_start = action_start_core_setting;
}

static void menu_entries_cbs_init_bind_content_list_switch(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_content_list_switch = deferred_push_content_list;
}

static void menu_entries_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_cancel = action_cancel_lookup_setting;

   /* TODO - add some stuff here. */
   cbs->action_cancel = action_cancel_pop_default;
}

static int is_rdb_entry(const char *label)
{
   return (
         !(strcmp(label, "rdb_entry_publisher")) ||
         !(strcmp(label, "rdb_entry_developer")) ||
         !(strcmp(label, "rdb_entry_origin")) ||
         !(strcmp(label, "rdb_entry_franchise")) ||
         !(strcmp(label, "rdb_entry_enhancement_hw")) ||
         !(strcmp(label, "rdb_entry_esrb_rating")) ||
         !(strcmp(label, "rdb_entry_bbfc_rating")) ||
         !(strcmp(label, "rdb_entry_elspa_rating")) ||
         !(strcmp(label, "rdb_entry_pegi_rating")) ||
         !(strcmp(label, "rdb_entry_cero_rating")) ||
         !(strcmp(label, "rdb_entry_edge_magazine_rating")) ||
         !(strcmp(label, "rdb_entry_edge_magazine_issue")) ||
         !(strcmp(label, "rdb_entry_releasemonth")) ||
         !(strcmp(label, "rdb_entry_releaseyear")) ||
         !(strcmp(label, "rdb_entry_max_users"))
         );
}

static int is_settings_entry(const char *label)
{
   return (
    !strcmp(label, "Driver Options") ||
    !strcmp(label, "General Options") ||
    !strcmp(label, "Video Options") ||
    !strcmp(label, "Shader Options") ||
    !strcmp(label, "Font Options") ||
    !strcmp(label, "Audio Options") ||
    !strcmp(label, "Input Options") ||
    !strcmp(label, "Overlay Options") ||
    !strcmp(label, "Menu Options") ||
    !strcmp(label, "UI Options") ||
    !strcmp(label, "Patch Options") ||
    !strcmp(label, "Playlist Options") ||
    !strcmp(label, "Onscreen Keyboard Overlay Options") ||
    !strcmp(label, "Core Updater Options") ||
    !strcmp(label, "Network Options") ||
    !strcmp(label, "Archive Options") ||
    !strcmp(label, "User Options") ||
    !strcmp(label, "Path Options") ||
    !strcmp(label, "Privacy Options"));
}

static void menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label)
{
   rarch_setting_t *setting = menu_action_find_setting(label);

   if (!cbs)
      return;
   if (!driver.menu)
      return;

   cbs->action_ok = action_ok_lookup_setting;

   if (elem0[0] != '\0' && is_rdb_entry(elem0))
   {
      cbs->action_ok = action_ok_rdb_entry_submenu;
      return;
   }

   if (!strcmp(label, "custom_bind_all"))
      cbs->action_ok = action_ok_lookup_setting;
   else if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
      cbs->action_ok = action_ok_lookup_setting;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_ok = action_ok_cheat;
   else if (!strcmp(label, "savestate"))
      cbs->action_ok = action_ok_save_state;
   else if (!strcmp(label, "loadstate"))
      cbs->action_ok = action_ok_load_state;
   else if (!strcmp(label, "resume_content"))
      cbs->action_ok = action_ok_resume_content;
   else if (!strcmp(label, "restart_content"))
      cbs->action_ok = action_ok_restart_content;
   else if (!strcmp(label, "take_screenshot"))
      cbs->action_ok = action_ok_screenshot;
   else if (!strcmp(label, "file_load_or_resume"))
      cbs->action_ok = action_ok_file_load_or_resume;
   else if (!strcmp(label, "quit_retroarch"))
      cbs->action_ok = action_ok_quit;
   else if (!strcmp(label, "save_new_config"))
      cbs->action_ok = action_ok_save_new_config;
   else if (!strcmp(label, "help"))
      cbs->action_ok = action_ok_help;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_ok = action_ok_shader_pass;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_ok = action_ok_shader_preset;
   else if (!strcmp(label, "cheat_file_load"))
      cbs->action_ok = action_ok_cheat_file;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_ok = action_ok_remap_file;
   else if (!strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters")
         )
      cbs->action_ok = action_ok_shader_parameters;
   else if (
         !strcmp(label, "Shader Options") ||
         !strcmp(label, "Input Options") ||
         !strcmp(label, "core_options") ||
         !strcmp(label, "core_cheat_options") ||
         !strcmp(label, "core_input_remapping_options") ||
         !strcmp(label, "core_information") ||
         !strcmp(label, "disk_options") ||
         !strcmp(label, "settings") ||
         !strcmp(label, "performance_counters") ||
         !strcmp(label, "frontend_counters") ||
         !strcmp(label, "core_counters")
         )
      cbs->action_ok = action_ok_push_default;
   else if (
         !strcmp(label, "load_content") ||
         !strcmp(label, "detect_core_list")
         )
      cbs->action_ok = action_ok_push_content_list;
   else if (!strcmp(label, "history_list") ||
         !strcmp(label, "core_updater_list") ||
         !strcmp(label, "cursor_manager_list") ||
         !strcmp(label, "database_manager_list") ||
         (setting && setting->browser_selection_type == ST_DIR)
         )
      cbs->action_ok = action_ok_push_generic_list;
   else if (!strcmp(label, "shader_apply_changes"))
      cbs->action_ok = action_ok_shader_apply_changes;
   else if (!strcmp(label, "cheat_apply_changes"))
      cbs->action_ok = action_ok_cheat_apply_changes;
   else if (!strcmp(label, "video_shader_preset_save_as"))
      cbs->action_ok = action_ok_shader_preset_save_as;
   else if (!strcmp(label, "cheat_file_save_as"))
      cbs->action_ok = action_ok_cheat_file_save_as;
   else if (!strcmp(label, "remap_file_save_as"))
      cbs->action_ok = action_ok_remap_file_save_as;
   else if (!strcmp(label, "core_list"))
      cbs->action_ok = action_ok_core_list;
   else if (!strcmp(label, "disk_image_append"))
      cbs->action_ok = action_ok_disk_image_append_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_ok = action_ok_configurations_list;
   else
   switch (type)
   {
      case MENU_SETTINGS_VIDEO_RESOLUTION:
         cbs->action_ok = action_ok_video_resolution;
         break;
      case MENU_FILE_PLAYLIST_ENTRY:
         if (!strcmp(label, "rdb_entry_start_game"))
            cbs->action_ok = action_ok_rdb_playlist_entry;
         else
            cbs->action_ok = action_ok_playlist_entry;
         break;
      case MENU_FILE_CONTENTLIST_ENTRY:
         cbs->action_ok = action_ok_push_generic_list;
         break;
      case MENU_FILE_CHEAT:
         cbs->action_ok = action_ok_cheat_file_load;
         break;
      case MENU_FILE_REMAP:
         cbs->action_ok = action_ok_remap_file_load;
         break;
      case MENU_FILE_SHADER_PRESET:
         cbs->action_ok = action_ok_shader_preset_load;
         break;
      case MENU_FILE_SHADER:
         cbs->action_ok = action_ok_shader_pass_load;
         break;
      case MENU_FILE_IMAGE:
         cbs->action_ok = action_ok_menu_wallpaper_load;
         break;
      case MENU_FILE_USE_DIRECTORY:
         cbs->action_ok = action_ok_path_use_directory;
         break;
      case MENU_FILE_CONFIG:
         cbs->action_ok = action_ok_config_load;
         break;
      case MENU_FILE_DIRECTORY:
         cbs->action_ok = action_ok_directory_push;
         break;
      case MENU_FILE_CARCHIVE:
         cbs->action_ok = action_ok_compressed_archive_push;
         break;
      case MENU_FILE_CORE:
         if (!strcmp(menu_label, "deferred_core_list"))
            cbs->action_ok = action_ok_core_load_deferred;
         else if (!strcmp(menu_label, "core_list"))
            cbs->action_ok = action_ok_core_load;
         else if (!strcmp(menu_label, "core_updater_list"))
            cbs->action_ok = action_ok_core_download;
         else
            return;
         break;
      case MENU_FILE_DOWNLOAD_CORE:
         cbs->action_ok = action_ok_core_updater_list;
         break;
      case MENU_FILE_DOWNLOAD_CORE_INFO:
         break;
      case MENU_FILE_RDB:
         if (!strcmp(menu_label, "deferred_database_manager_list"))
            cbs->action_ok = action_ok_database_manager_list_deferred;
         else if (!strcmp(menu_label, "database_manager_list") 
               || !strcmp(menu_label, "Horizontal Menu"))
            cbs->action_ok = action_ok_database_manager_list;
         else
            return;
         break;
      case MENU_FILE_RDB_ENTRY:
         cbs->action_ok = action_ok_rdb_entry;
         break;
      case MENU_FILE_CURSOR:
         if (!strcmp(menu_label, "deferred_database_manager_list"))
            cbs->action_ok = action_ok_cursor_manager_list_deferred;
         else if (!strcmp(menu_label, "cursor_manager_list"))
            cbs->action_ok = action_ok_cursor_manager_list;
         break;
      case MENU_FILE_FONT:
      case MENU_FILE_OVERLAY:
      case MENU_FILE_AUDIOFILTER:
      case MENU_FILE_VIDEOFILTER:
         cbs->action_ok = action_ok_set_path;
         break;
#ifdef HAVE_COMPRESSION
      case MENU_FILE_IN_CARCHIVE:
#endif
      case MENU_FILE_PLAIN:
         if (!strcmp(menu_label, "detect_core_list"))
            cbs->action_ok = action_ok_file_load_with_detect_core;
         else if (!strcmp(menu_label, "disk_image_append"))
            cbs->action_ok = action_ok_disk_image_append;
         else
            cbs->action_ok = action_ok_file_load;
         break;
      case MENU_SETTINGS_CUSTOM_VIEWPORT:
         cbs->action_ok = action_ok_custom_viewport;
         break;
      case MENU_SETTINGS:
      case MENU_SETTING_GROUP:
      case MENU_SETTING_SUBGROUP:
         cbs->action_ok = action_ok_push_default;
         break;
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
         cbs->action_ok = action_ok_disk_cycle_tray_status;
         break;
      default:
         return;
   }
}


static void menu_entries_cbs_init_bind_up_or_down(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_up_or_down = action_bind_up_or_down_generic;
}


static void menu_entries_cbs_init_bind_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label)
{
   int i;

   if (!cbs)
      return;

   if (label)
   {
      if (is_settings_entry(elem0))
      {
         cbs->action_toggle = action_toggle_scroll;
         return;
      }
   }

   cbs->action_toggle = menu_action_setting_set;

   switch (type)
   {
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
         cbs->action_toggle = disk_options_disk_idx_toggle;
         break;
      case MENU_FILE_PLAIN:
      case MENU_FILE_DIRECTORY:
      case MENU_FILE_CARCHIVE:
      case MENU_FILE_CORE:
      case MENU_FILE_RDB:
      case MENU_FILE_RDB_ENTRY:
      case MENU_FILE_CURSOR:
      case MENU_FILE_SHADER:
      case MENU_FILE_IMAGE:
      case MENU_FILE_OVERLAY:
      case MENU_FILE_VIDEOFILTER:
      case MENU_FILE_AUDIOFILTER:
      case MENU_FILE_CONFIG:
      case MENU_FILE_USE_DIRECTORY:
      case MENU_FILE_PLAYLIST_ENTRY:
      case MENU_FILE_DOWNLOAD_CORE:
      case MENU_FILE_CHEAT:
      case MENU_FILE_REMAP:
      case MENU_SETTING_GROUP:
         if (!strcmp(menu_label, "Horizontal Menu")
               || !strcmp(menu_label, "Main Menu"))
            cbs->action_toggle = action_toggle_mainmenu;
         else
            cbs->action_toggle = action_toggle_scroll;
         break;
      case MENU_SETTING_ACTION:
      case MENU_FILE_CONTENTLIST_ENTRY:
         cbs->action_toggle = action_toggle_mainmenu;
         break;
   }

   if (strstr(label, "rdb_entry"))
      cbs->action_toggle = action_toggle_scroll;

   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_toggle = shader_action_parameter_toggle;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_toggle = shader_action_parameter_preset_toggle;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_toggle = action_toggle_cheat;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_toggle = action_toggle_input_desc;
   else if (!strcmp(label, "savestate") ||
         !strcmp(label, "loadstate"))
      cbs->action_toggle = action_toggle_save_state;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_toggle = action_toggle_shader_scale_pass;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_toggle = action_toggle_shader_filter_pass;
   else if (!strcmp(label, "video_shader_default_filter"))
      cbs->action_toggle = action_toggle_shader_filter_default;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_toggle = action_toggle_shader_num_passes;
   else if (!strcmp(label, "cheat_num_passes"))
      cbs->action_toggle = action_toggle_cheat_num_passes;
   else if (type == MENU_SETTINGS_VIDEO_RESOLUTION)
      cbs->action_toggle = action_toggle_video_resolution;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_toggle = core_setting_toggle;

   for (i = 0; i < MAX_USERS; i++)
   {
      char label_setting[PATH_MAX_LENGTH];
      snprintf(label_setting, sizeof(label_setting), "input_player%d_joypad_index", i + 1);

      if (!strcmp(label, label_setting))
         cbs->action_toggle = menu_action_setting_set;
   }
}

static void menu_entries_cbs_init_bind_refresh(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs || !driver.menu)
      return;

   cbs->action_refresh = action_refresh_default;
}

static void menu_entries_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs || !driver.menu)
      return;

   cbs->action_iterate = action_iterate_main;
}

static void menu_entries_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs || !driver.menu)
      return;

   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_input_desc;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_cheat;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_perf_counters;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_libretro_perf_counters;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_shader_preset_parameter;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_shader_parameter;
   else if (!strcmp(label, "cheat_num_passes"))
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_cheat_num_passes;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_get_representation = 
         menu_action_setting_disp_set_label_remap_file_load;
   else if (!strcmp(label, "video_shader_filter_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_filter_pass;
   else if (!strcmp(label, "video_shader_scale_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_scale_pass;
   else if (!strcmp(label, "video_shader_num_passes"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_num_passes;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_pass;
   else if (!strcmp(label, "video_shader_default_filter"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_shader_default_filter;
   else if (!strcmp(label, "configurations"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_configurations;
   else
   {
      switch (type)
      {
         case MENU_FILE_CORE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_core;
            break;
         case MENU_FILE_PLAIN:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_plain;
            break;
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_use_directory;
            break;
         case MENU_FILE_DIRECTORY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_directory;
            break;
         case MENU_FILE_CARCHIVE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_carchive;
            break;
         case MENU_FILE_OVERLAY:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_overlay;
            break;
         case MENU_FILE_FONT:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_font;
            break;
         case MENU_FILE_SHADER:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_shader;
            break;
         case MENU_FILE_SHADER_PRESET:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_shader_preset;
            break;
         case MENU_FILE_CONFIG:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_config;
            break;
         case MENU_FILE_IN_CARCHIVE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_in_carchive;
            break;
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_filter;
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_url;
            break;
         case MENU_FILE_RDB:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_rdb;
            break;
         case MENU_FILE_CURSOR:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_cursor;
            break;
         case MENU_FILE_CHEAT:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_cheat;
            break;
         case MENU_SETTING_SUBGROUP:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_file_subgroup;
            break;
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
         case MENU_SETTINGS_CUSTOM_BIND_ALL:
         case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_more;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_disk_index;
            break;
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            cbs->action_get_representation = 
               menu_action_setting_disp_set_label_menu_video_resolution;
            break;
         default:
            cbs->action_get_representation = menu_action_setting_disp_set_label;
            break;
      }
   }
}

static void menu_entries_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs || !driver.menu)
      return;

   cbs->action_deferred_push = deferred_push_default;

   if ((strlen(elem1) != 0) && !!strcmp(elem0, elem1))
   {
      if (is_settings_entry(elem0))
      {
         if (!g_settings.menu.collapse_subgroups_enable)
         {
            cbs->action_deferred_push = deferred_push_settings_subgroup;
            return;
         }
      }
   }

   if (strstr(label, "deferred_rdb_entry_detail"))
      cbs->action_deferred_push = deferred_push_rdb_entry_detail;
   else if (!strcmp(label, "core_updater_list"))
      cbs->action_deferred_push = deferred_push_core_updater_list;
   else if (!strcmp(label, "history_list"))
      cbs->action_deferred_push = deferred_push_history_list;
   else if (!strcmp(label, "database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list;
   else if (!strcmp(label, "cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list;
   else if (!strcmp(label, "cheat_file_load"))
      cbs->action_deferred_push = deferred_push_cheat_file_load;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_deferred_push = deferred_push_remap_file_load;
   else if (!strcmp(label, "content_actions"))
      cbs->action_deferred_push = deferred_push_content_actions;
   else if (!strcmp(label, "Shader Options"))
      cbs->action_deferred_push = deferred_push_shader_options;
   else if (type == MENU_SETTING_GROUP)
      cbs->action_deferred_push = deferred_push_category;
   else if (!strcmp(label, "deferred_core_list"))
      cbs->action_deferred_push = deferred_push_core_list_deferred;
   else if (!strcmp(label, "deferred_database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list_deferred;
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred;
   else if (
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear")
         )
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred_query_subsearch;
   else if (!strcmp(label, "core_information"))
      cbs->action_deferred_push = deferred_push_core_information;
   else if (!strcmp(label, "performance_counters"))
      cbs->action_deferred_push = deferred_push_performance_counters;
   else if (!strcmp(label, "core_counters"))
      cbs->action_deferred_push = deferred_push_core_counters;
   else if (!strcmp(label, "video_shader_preset_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_preset_parameters;
   else if (!strcmp(label, "video_shader_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_parameters;
   else if (!strcmp(label, "settings"))
      cbs->action_deferred_push = deferred_push_settings;
   else if (!strcmp(label, "frontend_counters"))
      cbs->action_deferred_push = deferred_push_frontend_counters;
   else if (!strcmp(label, "core_options"))
      cbs->action_deferred_push = deferred_push_core_options;
   else if (!strcmp(label, "core_cheat_options"))
      cbs->action_deferred_push = deferred_push_core_cheat_options;
   else if (!strcmp(label, "core_input_remapping_options"))
      cbs->action_deferred_push = deferred_push_core_input_remapping_options;
   else if (!strcmp(label, "disk_options"))
      cbs->action_deferred_push = deferred_push_disk_options;
   else if (!strcmp(label, "core_list"))
      cbs->action_deferred_push = deferred_push_core_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_deferred_push = deferred_push_configurations;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_deferred_push = deferred_push_video_shader_preset;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_deferred_push = deferred_push_video_shader_pass;
   else if (!strcmp(label, "video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "menu_wallpaper"))
      cbs->action_deferred_push = deferred_push_images;
   else if (!strcmp(label, "audio_dsp_plugin"))
      cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
   else if (!strcmp(label, "input_overlay"))
      cbs->action_deferred_push = deferred_push_input_overlay;
   else if (!strcmp(label, "input_osk_overlay"))
      cbs->action_deferred_push = deferred_push_input_osk_overlay;
   else if (!strcmp(label, "video_font_path"))
      cbs->action_deferred_push = deferred_push_video_font_path;
   else if (!strcmp(label, "game_history_path"))
      cbs->action_deferred_push = deferred_push_content_history_path;
   else if (!strcmp(label, "detect_core_list"))
      cbs->action_deferred_push = deferred_push_detect_core_list;
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

   if (!list)
      return;

   if (!(cbs = (menu_file_list_cbs_t*)list->list[idx].actiondata))
      return;

   menu_list_get_last_stack(driver.menu->menu_list,
         NULL, &menu_label, NULL);

   if (label)
      str_list = string_split(label, "|");

   if (str_list && str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list && str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

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
