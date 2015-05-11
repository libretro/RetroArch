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
#include <retro_inline.h>
#include "menu.h"
#include "menu_entry.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_navigation.h"

#include "../retroarch.h"

#include "../input/input_autodetect.h"

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH];
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type      = 0;
   menu_navigation_t *nav = menu_navigation_get_ptr();
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!menu_list || !nav)
      return -1;

   menu_list_pop_stack(menu_list);

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu_list->selection_buf,
         nav->selection_ptr, &path, NULL, &type);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         menu_list,
         cat_path,
         menu_label,
         type,
         nav->selection_ptr);

   return 0;
}

static int archive_load(void)
{
   int ret;
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type      = 0;
   menu_handle_t *menu    = menu_driver_get_ptr();
   settings_t *settings   = config_get_ptr();
   global_t      *global  = global_get_ptr();
   size_t selected        = menu_navigation_get_current_selection();

   if (!menu)
      return -1;

   menu_list_pop_stack(menu->menu_list);

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         selected, &path, NULL, &type);

   ret = rarch_defer_core(global->core_info, menu_path, path, menu_label,
         menu->deferred_path, sizeof(menu->deferred_path));

   switch (ret)
   {
      case -1:
         event_command(EVENT_CMD_LOAD_CORE);
         menu_entries_common_load_content(false);
         break;
      case 0:
         menu_list_push_stack_refresh(
               menu->menu_list,
               settings->libretro_directory,
               "deferred_core_list",
               0, selected);
         break;
   }

   return 0;
}

static int load_or_open_zip_iterate(unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   snprintf(msg, sizeof(msg), "Opening compressed file\n"
         " \n"

         " - OK to open as Folder\n"
         " - Cancel/Back to Load \n");

   menu_driver_render_messagebox(msg);

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

static int action_iterate_help(const char *label, unsigned action)
{
   int ret;
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
   };
   char desc[ARRAY_SIZE(binds)][64];
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu    = menu_driver_get_ptr();
   settings_t *settings   = config_get_ptr();

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

   menu_driver_render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      menu_list_pop(menu->menu_list->menu_stack, NULL);

   menu_input_post_iterate(&ret, action);

   return ret;
}

static int action_iterate_info(const char *label, unsigned action)
{
   int ret = 0;
   char msg[PATH_MAX_LENGTH];
   char needle[PATH_MAX_LENGTH];
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
      menu_list_get_at_offset(list, selection, NULL, &lbl, &info_type);

      if (lbl)
         strlcpy(needle, lbl, sizeof(needle));
   }

   setting_get_description(needle, msg, sizeof(msg));

   menu_driver_render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      menu_list_pop(menu_list->menu_stack, &menu->navigation.selection_ptr);

   menu_input_post_iterate(&ret, action);

   return ret;
}

static int action_iterate_load_open_zip(const char *label, unsigned action)
{
   settings_t *settings   = config_get_ptr();

   switch (settings->archive.mode)
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
   const char *base_msg             = NULL;
   unsigned type                    = 0;
   global_t      *global            = global_get_ptr();
   video_viewport_t *custom         = &global->console.screen.viewports.custom_vp;
   menu_handle_t *menu              = menu_driver_get_ptr();
   menu_list_t *menu_list           = menu_list_get_ptr();
   settings_t *settings             = config_get_ptr();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu_list, NULL, NULL, &type);

   geom = (struct retro_game_geometry*)&global->system.av_info.geometry;

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

         if (!strcmp(label, "custom_viewport_2"))
         {
            menu_list_push(menu_list->menu_stack, "", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  menu->navigation.selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(menu_list);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !settings->video.scale_integer)
            menu_list_push(menu_list->menu_stack, "",
                  "custom_viewport_2", 0, menu->navigation.selection_ptr);
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

   menu_list_get_last_stack(menu_list, NULL, &label, &type);

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

   menu_driver_render_messagebox(msg);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   event_command(EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

static int action_iterate_custom_bind(const char *label, unsigned action)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   if (menu_input_bind_iterate())
      menu_list_pop_stack(menu_list);
   return 0;
}

static int action_iterate_custom_bind_keyboard(const char *label, unsigned action)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   if (menu_input_bind_iterate_keyboard())
      menu_list_pop_stack(menu_list);
   return 0;
}

static int action_iterate_message(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return -1;

   menu_driver_render_messagebox(menu->message_contents);

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(menu->menu_list);

   return 0;
}

static int action_iterate_switch(const char *label, unsigned action)
{
   menu_entry_t entry;
   size_t selected           = menu_navigation_get_current_selection();

   menu_entry_get(&entry,    selected, NULL, false);

   return menu_entry_action(&entry, selected, action);
}

static int action_iterate_main(const char *label, unsigned action)
{
   int ret                   = 0;
   menu_handle_t *menu       = menu_driver_get_ptr();
   global_t *global          = global_get_ptr();
   if (!menu)
      return 0;

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
      if (global->menu.bind_mode_keyboard)
         return action_iterate_custom_bind_keyboard(label, action);
      else
         return action_iterate_custom_bind(label, action);
   }

   if (menu->need_refresh && !menu->nonblocking_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_REFRESH;

   ret = action_iterate_switch(label, action);

   if (ret)
      return ret;

   menu_input_post_iterate(&ret, action);

   menu_driver_render();

   /* Have to defer it so we let settings refresh. */
   if (menu->push_start_screen)
   {
      menu_list_t *menu_list    = menu_list_get_ptr();

      menu_list_push(menu_list->menu_stack, "", "help", 0, 0);
      menu->push_start_screen = false;
   }

   return ret;
}

void menu_entries_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (cbs)
      cbs->action_iterate = action_iterate_main;
}
