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

#include <compat/strl.h>
#include <file/file_path.h>
#include <rhash.h>
#include <retro_inline.h>

#include "../menu.h"
#include "../menu_display.h"
#include "../menu_hash.h"
#include "../menu_entry.h"
#include "../menu_setting.h"
#include "../menu_input.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"

#include "../../general.h"
#include "../../retroarch.h"
#include "../../input/input_common.h"
#include "../../input/input_autodetect.h"

extern char detect_content_path[PATH_MAX_LENGTH];

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t info   = {0};
   const char *menu_path          = NULL;
   const char *menu_label         = NULL;
   const char* path               = NULL;
   unsigned int type              = 0;
   size_t entry_idx               = 0;
   menu_navigation_t *nav         = menu_navigation_get_ptr();
   menu_list_t *menu_list         = menu_list_get_ptr();

   if (!menu_list || !nav)
      return -1;

   menu_list_pop_stack(menu_list);

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   if (menu_list_get_size(menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu_list->selection_buf,
         nav->selection_ptr, &path, NULL, &type, &entry_idx);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));

   fill_pathname_join(detect_content_path, menu_path, path,
         sizeof(detect_content_path));

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = nav->selection_ptr;
   strlcpy(info.path, cat_path, sizeof(info.path));
   strlcpy(info.label, menu_label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int archive_load(void)
{
   int ret = 0;
   menu_displaylist_info_t info = {0};
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   size_t entry_idx       = 0;
   settings_t *settings   = config_get_ptr();
   global_t      *global  = global_get_ptr();
   size_t selected        = menu_navigation_get_current_selection();
   menu_handle_t *menu    = menu_driver_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!menu || !menu_list)
      return -1;

   menu_list_pop_stack(menu_list);

   menu_list_get_last_stack(menu_list, &menu_path, &menu_label, NULL, NULL);

   if (menu_list_get_size(menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu_list->selection_buf,
         selected, &path, NULL, NULL, &entry_idx);

   ret = rarch_defer_core(global->core_info, menu_path, path, menu_label,
         menu->deferred_path, sizeof(menu->deferred_path));

   fill_pathname_join(detect_content_path, menu_path, path,
         sizeof(detect_content_path));

   switch (ret)
   {
      case -1:
         event_command(EVENT_CMD_LOAD_CORE);
         menu_common_load_content(false);
         break;
      case 0:
         info.list          = menu_list->menu_stack;
         info.type          = 0;
         info.directory_ptr = selected;
         strlcpy(info.path, settings->libretro_directory, sizeof(info.path));
         strlcpy(info.label, "deferred_core_list", sizeof(info.label));

         ret = menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
         break;
   }

   return ret;
}

static int load_or_open_zip_iterate(char *s, size_t len, unsigned action)
{
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   snprintf(s, len, "Opening compressed file\n"
         " \n"

         " - OK to open as Folder\n"
         " - Cancel/Back to Load \n");

   menu_driver_render_messagebox(s);

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

static int action_iterate_help(char *s, size_t len, const char *label)
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
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_Y,
   };
   char desc[ARRAY_SIZE(binds)][64] = {{0}};
   menu_handle_t *menu              = menu_driver_get_ptr();
   settings_t *settings             = config_get_ptr();

   if (!menu)
      return 0;

   menu_driver_render();

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      const struct retro_keybind *keybind = (const struct retro_keybind*)
         &settings->input.binds[0][binds[i]];
      const struct retro_keybind *auto_bind = (const struct retro_keybind*)
         input_get_auto_bind(0, binds[i]);

      input_get_bind_string(desc[i], keybind, auto_bind, sizeof(desc[i]));
   }

   snprintf(s, len,
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
         "Toggle Keyboard: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Core).\n"
         "Load a content file (Load Content).     \n"
         " \n"
         "See Path Settings to set directories \n"
         "for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
      desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6], desc[7]);

   return 0;
}

static int action_iterate_info(char *s, size_t len, const char *label)
{
   int ret = 0;
   char needle[PATH_MAX_LENGTH]     = {0};
   unsigned info_type               = 0;
   rarch_setting_t *current_setting = NULL;
   file_list_t *list                = NULL;
   menu_handle_t *menu              = menu_driver_get_ptr();
   menu_list_t *menu_list           = menu_list_get_ptr();
   size_t selection                 = menu_navigation_get_current_selection();
   if (!menu)
      return 0;

   list = (file_list_t*)menu_list->selection_buf;

   menu_driver_render();

   current_setting = menu_setting_find(list->list[selection].label);

   if (current_setting)
      strlcpy(needle, current_setting->name, sizeof(needle));
   else if ((current_setting = menu_setting_find(list->list[selection].label)))
   {
      if (current_setting)
         strlcpy(needle, current_setting->name, sizeof(needle));
   }
   else
   {
      const char *lbl = NULL;
      menu_list_get_at_offset(list, selection, NULL, &lbl, &info_type, NULL);

      if (lbl)
         strlcpy(needle, lbl, sizeof(needle));
   }

   setting_get_description(needle, s, len);

   return ret;
}

static int action_iterate_load_open_zip(const char *label, char *s, size_t len, unsigned action)
{
   settings_t *settings   = config_get_ptr();

   switch (settings->archive.mode)
   {
      case 0:
         return load_or_open_zip_iterate(s, len, action);
      case 1:
         return archive_load();
      case 2:
         return archive_open();
      default:
         break;
   }

   return 0;
}

static int action_iterate_menu_viewport(char *s, size_t len, const char *label, unsigned action, uint32_t hash)
{
   int stride_x = 1, stride_y = 1;
   menu_displaylist_info_t info     = {0};
   struct retro_game_geometry *geom = NULL;
   const char *base_msg             = NULL;
   unsigned type                    = 0;
   video_viewport_t *custom         = video_viewport_get_custom();
   menu_handle_t *menu              = menu_driver_get_ptr();
   menu_navigation_t *nav           = menu_navigation_get_ptr();
   menu_list_t *menu_list           = menu_list_get_ptr();
   settings_t *settings             = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu_list, NULL, NULL, &type, NULL);

   geom = (struct retro_game_geometry*)&av_info->geometry;

   if (settings->video.scale_integer)
   {
      stride_x = geom->base_width;
      stride_y = geom->base_height;
   }

   switch (action)
   {
      case MENU_ACTION_UP:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y      -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
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

         event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_LEFT:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x     -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
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

         event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_CANCEL:
         menu_list_pop_stack(menu_list);

         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_2)
         {
            info.list          = menu_list->menu_stack;
            info.type          = MENU_SETTINGS_CUSTOM_VIEWPORT;
            info.directory_ptr = nav->selection_ptr;

            menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(menu_list);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !settings->video.scale_integer)
         {
            info.list          = menu_list->menu_stack;
            strlcpy(info.label, "custom_viewport_2", sizeof(info.label));
            info.type          = 0;
            info.directory_ptr = nav->selection_ptr;

            menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         }
         break;

      case MENU_ACTION_START:
         if (!settings->video.scale_integer)
         {
            video_viewport_t vp;
            video_driver_viewport_info(&vp);

            if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
            {
               custom->width  += custom->x;
               custom->height += custom->y;
               custom->x       = 0;
               custom->y       = 0;
            }
            else
            {
               custom->width   = vp.full_width - custom->x;
               custom->height  = vp.full_height - custom->y;
            }

            event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
         }
         break;

      case MENU_ACTION_MESSAGE:
         menu->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(menu_list, NULL, &label, &type, NULL);

   menu_driver_render();

   if (settings->video.scale_integer)
   {
      custom->x     = 0;
      custom->y     = 0;
      custom->width = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;
      base_msg       = "Set scale";
       
      snprintf(s, len, "%s (%4ux%4u, %u x %u scale)",
            base_msg,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height);
   }
   else
   {
      if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (hash == MENU_LABEL_CUSTOM_VIEWPORT_2)
         base_msg = "Set Bottom-Right Corner";

      snprintf(s, len, "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height);
   }

   menu_driver_render_messagebox(s);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

enum action_iterate_type
{
   ITERATE_TYPE_DEFAULT = 0,
   ITERATE_TYPE_HELP,
   ITERATE_TYPE_INFO,
   ITERATE_TYPE_ZIP,
   ITERATE_TYPE_MESSAGE,
   ITERATE_TYPE_VIEWPORT,
   ITERATE_TYPE_BIND,
};

static enum action_iterate_type action_iterate_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_HELP:
         return ITERATE_TYPE_HELP;
      case MENU_LABEL_INFO_SCREEN:
         return ITERATE_TYPE_INFO;
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return ITERATE_TYPE_ZIP;
      case MENU_LABEL_MESSAGE:
         return ITERATE_TYPE_MESSAGE;
      case MENU_LABEL_CUSTOM_VIEWPORT_1:
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
         return ITERATE_TYPE_VIEWPORT;
      case MENU_LABEL_CUSTOM_BIND:
      case MENU_LABEL_CUSTOM_BIND_ALL:
      case MENU_LABEL_CUSTOM_BIND_DEFAULTS:
         return ITERATE_TYPE_BIND;
   }

   return ITERATE_TYPE_DEFAULT;
}

static int action_iterate_main(const char *label, unsigned action)
{
   menu_entry_t entry;
   static bool did_messagebox = false;
   char msg[PATH_MAX_LENGTH]      = {0};
   enum action_iterate_type iterate_type;
   size_t selected, *pop_selected = false;
   bool do_messagebox        = false;
   bool do_pop_stack         = false;
   bool do_post_iterate      = false;
   bool do_render            = false;
   int ret                   = 0;
   menu_handle_t *menu       = menu_driver_get_ptr();
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   uint32_t hash             = djb2_calculate(label);
   if (!menu || !menu_list)
      return 0;
   
   iterate_type              = action_iterate_type(hash);

   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = action_iterate_help(msg, sizeof(msg), label);
         pop_selected    = NULL;
         do_messagebox   = true;
         do_pop_stack    = true;
         do_post_iterate = true;
         break;
      case ITERATE_TYPE_BIND:
         if (menu_input_bind_iterate())
            menu_list_pop_stack(menu_list);
         break;
      case ITERATE_TYPE_VIEWPORT:
         ret = action_iterate_menu_viewport(msg, sizeof(msg), label, action, hash);
         break;
      case ITERATE_TYPE_INFO:
         ret = action_iterate_info(msg, sizeof(msg), label);
         pop_selected    = &nav->selection_ptr;
         do_messagebox   = true;
         do_pop_stack    = true;
         do_post_iterate = true;
         break;
      case ITERATE_TYPE_ZIP:
         ret = action_iterate_load_open_zip(label, msg, sizeof(msg), action);
         break;
      case ITERATE_TYPE_MESSAGE:
         strlcpy(msg, menu->message_contents, sizeof(msg));
         pop_selected    = &nav->selection_ptr;
         do_messagebox   = true;
         do_pop_stack    = true;
         break;
      case ITERATE_TYPE_DEFAULT:
         selected           = menu_navigation_get_current_selection();
         menu_entry_get(&entry,    selected, NULL, false);
         ret = menu_entry_action(&entry, selected, (enum menu_action)action);

         if (ret)
            return ret;

         do_post_iterate = true;
         do_render       = true;

         /* Have to defer it so we let settings refresh. */
         if (menu->push_start_screen)
         {
            menu_displaylist_info_t info = {0};

            info.list = menu_list->menu_stack;
            strlcpy(info.label, "help", sizeof(info.label));

            menu_displaylist_push_list(&info, DISPLAYLIST_HELP);
         }
         break;
   }

   did_messagebox = did_messagebox != do_messagebox;

   if (did_messagebox)
      menu_display_fb_set_dirty();

   if (do_messagebox)
      menu_driver_render_messagebox(msg);

   if (do_pop_stack && action == MENU_ACTION_OK)
      menu_list_pop(menu_list->menu_stack, pop_selected);
   
   if (do_post_iterate)
      menu_input_post_iterate(&ret, action);

   if (do_render)
      menu_driver_render();

   return ret;
}

int menu_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_iterate = action_iterate_main;

   return -1;
}
