/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <file/file_path.h>
#include "menu_backend.h"
#include "../menu_entries.h"
#include "../menu_input.h"

#include "../../input/input_autodetect.h"
#include "../../retroarch.h"
#include "../../config.def.h"

static int menu_message_toggle(unsigned action)
{
   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(driver.menu->message_contents);

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(driver.menu->menu_list);

   return 0;
}

static int menu_info_screen_iterate(unsigned action)
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
               driver.menu->list_mainmenu,
               list->list[driver.menu->selection_ptr].label)))
   {
      if (current_setting)
         strlcpy(needle, current_setting->name, sizeof(needle));
   }
   else
   {
      const char *label = NULL;
      menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr, NULL, &label,
            &info_type);

      if (label)
         strlcpy(needle, label, sizeof(needle));
   }

   setting_data_get_description(needle, msg, sizeof(msg));

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(driver.menu->menu_list);

   return 0;
}

static int menu_start_screen_iterate(unsigned action)
{
   unsigned i;
   char msg[PATH_MAX_LENGTH];

   if (!driver.menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

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

static int menu_viewport_iterate(unsigned action)
{
   int stride_x = 1, stride_y = 1;
   char msg[PATH_MAX_LENGTH];
   struct retro_game_geometry *geom = NULL;
   const char *base_msg = NULL;
   const char *label = NULL;
   unsigned type = 0;
   rarch_viewport_t *custom = (rarch_viewport_t*)
      &g_extern.console.screen.viewports.custom_vp;

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, &type);

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

static void menu_common_load_content(void)
{
   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_list_flush_stack(driver.menu->menu_list, MENU_SETTINGS);
   driver.menu->msg_force = true;
}

static int menu_archive_open(void)
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

static int menu_archive_load(void)
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
         menu_common_load_content();
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

static int menu_load_or_open_zip_iterate(unsigned action)
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
         menu_archive_open();
         break;
      case MENU_ACTION_CANCEL:
         menu_archive_load();
         break;
   }

   return 0;
}


static int menu_common_iterate(unsigned action)
{
   int ret = 0;
   unsigned type = 0;
   unsigned type_offset = 0;
   const char *label = NULL;
   const char *label_offset = NULL;
   const char *path_offset = NULL;
   unsigned scroll_speed = 0;
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, &type);

   mouse_iterate(action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (!strcmp(label, "help"))
      return menu_start_screen_iterate(action);
   else if (!strcmp(label, "message"))
      return menu_message_toggle(action);
   else if (!strcmp(label, "load_open_zip"))
   {
      switch (g_settings.archive.mode)
      {
         case 0:
            return menu_load_or_open_zip_iterate(action);
         case 1:
            return menu_archive_load();
         case 2:
            return menu_archive_open();
         default:
            break;
      }
   }
   else if (!strcmp(label, "info_screen"))
      return menu_info_screen_iterate(action);
   else if (
         type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
         !strcmp(label, "custom_viewport_2")
         )
      return menu_viewport_iterate(action);
   else if (type == MENU_SETTINGS_CUSTOM_BIND)
   {
      if (menu_input_bind_iterate(driver.menu))
         menu_list_pop_stack(driver.menu->menu_list);
      return 0;
   }
   else if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
   {
      if (menu_input_bind_iterate_keyboard(driver.menu))
         menu_list_pop_stack(driver.menu->menu_list);
      return 0;
   }

   menu_list_get_at_offset(driver.menu->menu_list->selection_buf,
         driver.menu->selection_ptr, &path_offset, &label_offset, &type_offset);

   if (driver.menu->need_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_REFRESH;

   scroll_speed = (max(driver.menu->scroll_accel, 2) - 2) / 4 + 1;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (driver.menu->selection_ptr >= scroll_speed)
               menu_navigation_set(driver.menu,
                     driver.menu->selection_ptr - scroll_speed, true);
         else
            menu_navigation_set(driver.menu,
                  menu_list_get_size(driver.menu->menu_list) - 1, true);
         break;

      case MENU_ACTION_DOWN:
         if (driver.menu->selection_ptr + scroll_speed < (menu_list_get_size(driver.menu->menu_list)))
            menu_navigation_set(driver.menu,
                  driver.menu->selection_ptr + scroll_speed, true);
         else
            menu_navigation_clear(driver.menu, false);
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
         menu_list_push_stack(driver.menu->menu_list, "", "info_screen",
               0, driver.menu->selection_ptr);
         break;

      case MENU_ACTION_REFRESH:
         menu_entries_deferred_push(driver.menu->menu_list->selection_buf,
               driver.menu->menu_list->menu_stack);

         driver.menu->need_refresh = false;
         break;

      case MENU_ACTION_MESSAGE:
         driver.menu->msg_force = true;
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

menu_ctx_driver_backend_t menu_ctx_backend_common = {
   menu_common_iterate,

   "menu_common",
};
