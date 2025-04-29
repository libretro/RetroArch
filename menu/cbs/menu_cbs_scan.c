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

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_setting.h"
#include "../../input/input_remapping.h"

#include "../../input/input_driver.h"

#include "../../configuration.h"
#include "../../tasks/tasks_internal.h"

#ifndef BIND_ACTION_SCAN
#define BIND_ACTION_SCAN(cbs, name) (cbs)->action_scan = (name)
#endif

#ifdef HAVE_LIBRETRODB
void handle_dbscan_finished(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   if (menu_st->driver_ctx->environ_cb)
      menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
            NULL, menu_st->userdata);
}

int action_scan_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
#if IOS
   char dir_path[DIR_MAX_LENGTH];
#endif
   char fullpath[PATH_MAX_LENGTH];
   const char *menu_path          = NULL;
   settings_t *settings           = config_get_ptr();
   bool show_hidden_files         = settings->bools.show_hidden_files;
   const char *directory_playlist = settings->paths.directory_playlist;
   const char *path_content_db    = settings->paths.path_content_database;

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

#if IOS
   fill_pathname_expand_special(dir_path, menu_path, sizeof(dir_path));
   menu_path = dir_path;
#endif

   fill_pathname_join_special(fullpath, menu_path, path, sizeof(fullpath));

   task_push_dbscan(
         directory_playlist,
         path_content_db,
         fullpath, false,
         show_hidden_files,
         handle_dbscan_finished);

   return 0;
}

int action_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
#if IOS
   char dir_path[DIR_MAX_LENGTH];
#endif
   char fullpath[PATH_MAX_LENGTH];
   const char *menu_path          = NULL;
   settings_t *settings           = config_get_ptr();
   bool show_hidden_files         = settings->bools.show_hidden_files;
   const char *directory_playlist = settings->paths.directory_playlist;
   const char *path_content_db    = settings->paths.path_content_database;

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);

#if IOS
   fill_pathname_expand_special(dir_path, menu_path, sizeof(dir_path));
   menu_path = dir_path;
#endif

   if (path)
      fill_pathname_join_special(fullpath, menu_path, path, sizeof(fullpath));
   else
      strlcpy(fullpath, menu_path, sizeof(fullpath));

   task_push_dbscan(
         directory_playlist,
         path_content_db,
         fullpath, true,
         show_hidden_files,
         handle_dbscan_finished);

   return 0;
}
#endif

int action_switch_thumbnail(const char *path,
      const char *label, unsigned type, size_t idx)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t selection           = menu_st->selection_ptr;
   const char *menu_ident     = menu_driver_ident();
   settings_t *settings       = config_get_ptr();
   bool switch_enabled        = true;
#ifdef HAVE_RGUI
   switch_enabled             = !string_is_equal(menu_ident, "rgui");
#endif

   if (!settings)
      return -1;

   /* RGUI has its own cycling for thumbnails in order to allow
    * cycling all images in fullscreen mode.
    * For other menu drivers, we cycle through available thumbnail
    * types and skip if already visible. */
   if (switch_enabled)
   {
      if (settings->uints.gfx_thumbnails == 0)
      {
         configuration_set_uint(settings,
               settings->uints.menu_left_thumbnails,
               settings->uints.menu_left_thumbnails + 1);

         if (settings->uints.gfx_thumbnails == settings->uints.menu_left_thumbnails)
            configuration_set_uint(settings,
                  settings->uints.menu_left_thumbnails,
                  settings->uints.menu_left_thumbnails + 1);

         if (settings->uints.menu_left_thumbnails > 3)
            configuration_set_uint(settings,
                  settings->uints.menu_left_thumbnails, 1);

         if (settings->uints.gfx_thumbnails == settings->uints.menu_left_thumbnails)
            configuration_set_uint(settings,
                  settings->uints.menu_left_thumbnails,
                  settings->uints.menu_left_thumbnails + 1);
      }
      else
      {
         configuration_set_uint(settings,
               settings->uints.gfx_thumbnails,
               settings->uints.gfx_thumbnails + 1);

         if (settings->uints.gfx_thumbnails == settings->uints.menu_left_thumbnails)
            configuration_set_uint(settings,
                  settings->uints.gfx_thumbnails,
                  settings->uints.gfx_thumbnails + 1);

         if (settings->uints.gfx_thumbnails > 3)
            configuration_set_uint(settings,
                  settings->uints.gfx_thumbnails, 1);

         if (settings->uints.gfx_thumbnails == settings->uints.menu_left_thumbnails)
            configuration_set_uint(settings,
                  settings->uints.gfx_thumbnails,
                  settings->uints.gfx_thumbnails + 1);
      }

      if (menu_st->driver_ctx)
      {
         if (menu_st->driver_ctx->update_thumbnail_path)
         {
            menu_st->driver_ctx->update_thumbnail_path(
                  menu_st->userdata, (unsigned)selection, 'L');
            menu_st->driver_ctx->update_thumbnail_path(
                  menu_st->userdata, (unsigned)selection, 'R');
         }
         if (menu_st->driver_ctx->update_thumbnail_image)
            menu_st->driver_ctx->update_thumbnail_image(menu_st->userdata);
      }
   }

   return 0;
}

static int action_scan_input_desc(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label         = NULL;
   unsigned key                   = 0;
   unsigned inp_desc_user         = 0;
   struct retro_keybind *target   = NULL;

   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL, NULL);

   if (string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST)))
   {
      settings_t *settings = config_get_ptr();
      inp_desc_user        = atoi(label);
      /* Skip 'Device Type', 'Analog to Digital Type' and 'Mapped Port' */
      key                  = (unsigned)(idx - 3);
      /* Select the reorderer bind */
      key                  =
            (key < RARCH_ANALOG_BIND_LIST_END) ? input_config_bind_order[key] : key;

      if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
            && type <= MENU_SETTINGS_INPUT_DESC_END)
         settings->uints.input_remap_ids[inp_desc_user][key] = RARCH_UNMAPPED;
      else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
            && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
         settings->uints.input_keymapper_ids[inp_desc_user][key] = RETROK_UNKNOWN;

      return 0;
   }
   else if (string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST)))
   {
      unsigned char player_no_str = atoi(&label[1]);

      inp_desc_user      = (unsigned)(player_no_str - 1);
      /* This hardcoded value may cause issues if any entries are added on
         top of the input binds */
      key                = (unsigned)(idx - 8);
      /* Select the reorderer bind */
      key                =
            (key < RARCH_ANALOG_BIND_LIST_END) ? input_config_bind_order[key] : key;
   }
   else
      key = input_config_translate_str_to_bind_id(label);

   target = &input_config_binds[inp_desc_user][key];

   if (target)
   {
      /* Clear mapping bit */
      input_keyboard_mapping_bits(0, target->key);

      target->key     = RETROK_UNKNOWN;
      target->joykey  = NO_BTN;
      target->joyaxis = AXIS_NONE;
      target->mbutton = NO_BTN;
   }

   return 0;
}

static int action_scan_video_font_path(const char *path,
      const char *label, unsigned type, size_t idx)
{
   settings_t *settings       = config_get_ptr();

   strlcpy(settings->paths.path_font, "null", sizeof(settings->paths.path_font));
   command_event(CMD_EVENT_REINIT, NULL);

   return 0;
}

#ifdef HAVE_XMB
static int action_scan_video_xmb_font(const char *path,
      const char *label, unsigned type, size_t idx)
{
   settings_t *settings       = config_get_ptr();

   strlcpy(settings->paths.path_menu_xmb_font, "null", sizeof(settings->paths.path_menu_xmb_font));
   command_event(CMD_EVENT_REINIT, NULL);

   return 0;
}
#endif

static int menu_cbs_init_bind_scan_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   switch (type)
   {
#ifdef HAVE_LIBRETRODB
      case FILE_TYPE_DIRECTORY:
         BIND_ACTION_SCAN(cbs, action_scan_directory);
         return 0;
      case FILE_TYPE_CARCHIVE:
      case FILE_TYPE_PLAIN:
         BIND_ACTION_SCAN(cbs, action_scan_file);
         return 0;
#endif
      case FILE_TYPE_RPL_ENTRY:
         BIND_ACTION_SCAN(cbs, action_switch_thumbnail);
         return 0;

      case FILE_TYPE_NONE:
      default:
         break;
   }

   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_SCAN(cbs, action_scan_input_desc);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_SCAN(cbs, action_scan_input_desc);
   }

   return -1;
}

int menu_cbs_init_bind_scan(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SCAN(cbs, NULL);

   if (cbs->setting)
   {
      switch (cbs->setting->type)
      {
         case ST_BIND:
            BIND_ACTION_SCAN(cbs, action_scan_input_desc);
            return 0;
         case ST_PATH:
            if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_PATH)))
            {
               BIND_ACTION_SCAN(cbs, action_scan_video_font_path);
               return 0;
            }
#ifdef HAVE_XMB
            else if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_XMB_FONT)))
            {
               BIND_ACTION_SCAN(cbs, action_scan_video_xmb_font);
               return 0;
            }
#endif
            break;
         default:
         case ST_NONE:
            break;
      }
   }

   menu_cbs_init_bind_scan_compare_type(cbs, type);

   return -1;
}
