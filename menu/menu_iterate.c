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
#include <retro_inline.h>

#include "menu.h"
#include "menu_display.h"
#include "menu_hash.h"
#include "menu_setting.h"
#include "menu_navigation.h"

#include "../general.h"
#include "../input/input_common.h"
#include "../input/input_autodetect.h"

enum menu_state_changes
{
   MENU_STATE_RENDER_FRAMEBUFFER = 0,
   MENU_STATE_RENDER_MESSAGEBOX,
   MENU_STATE_BLIT,
   MENU_STATE_POP_STACK,
   MENU_STATE_POST_ITERATE
}; 

static int action_iterate_help(char *s, size_t len, const char *label)
{
   unsigned i;
   menu_handle_t *menu       = menu_driver_get_ptr();
   settings_t *settings      = config_get_ptr();

   switch (menu->help_screen_type)
   {
      case MENU_HELP_WELCOME:
         {
            static int64_t timeout_end;
            int64_t timeout;
            static bool timer_begin = false;
            static bool timer_end   = false;
            int64_t current         = retro_get_time_usec();

            if (!timer_begin)
            {
               timeout_end = retro_get_time_usec() +
                  3 /* seconds */ * 1000000;
               timer_begin = true;
               timer_end   = false;
            }

            timeout = (timeout_end - current) / 1000000;

            menu_hash_get_help(MENU_LABEL_WELCOME_TO_RETROARCH,
                  s, len);

            if (!timer_end && timeout <= 0)
            {
               timer_end   = true;
               timer_begin = false;
               timeout_end = 0;
               menu->help_screen_type = MENU_HELP_NONE;
               return 1;
            }
         }
         break;
      case MENU_HELP_CONTROLS:
         {
            char s2[PATH_MAX_LENGTH];
            const unsigned binds[] = {
               RETRO_DEVICE_ID_JOYPAD_UP,
               RETRO_DEVICE_ID_JOYPAD_DOWN,
               RETRO_DEVICE_ID_JOYPAD_A,
               RETRO_DEVICE_ID_JOYPAD_B,
               RETRO_DEVICE_ID_JOYPAD_SELECT,
               RETRO_DEVICE_ID_JOYPAD_START,
               RARCH_MENU_TOGGLE,
               RARCH_QUIT_KEY,
               RETRO_DEVICE_ID_JOYPAD_X,
               RETRO_DEVICE_ID_JOYPAD_Y,
            };
            char desc[ARRAY_SIZE(binds)][64] = {{0}};

            for (i = 0; i < ARRAY_SIZE(binds); i++)
            {
               const struct retro_keybind *keybind = (const struct retro_keybind*)
                  &settings->input.binds[0][binds[i]];
               const struct retro_keybind *auto_bind = (const struct retro_keybind*)
                  input_get_auto_bind(0, binds[i]);

               input_get_bind_string(desc[i], keybind, auto_bind, sizeof(desc[i]));
            }

            menu_hash_get_help(MENU_LABEL_VALUE_MENU_CONTROLS_PROLOG,
                  s2, sizeof(s2));

            snprintf(s, len,
                  "%s"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n",
                  s2,
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP),    desc[0],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN),  desc[1],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM),      desc[2],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),         desc[3],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO),         desc[4],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_START),        desc[5],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU),  desc[6],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT),         desc[7],
                  menu_hash_to_str(MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD), desc[8]
                  );
         }
         break;
      case MENU_HELP_WHAT_IS_A_CORE:
         menu_hash_get_help(MENU_LABEL_VALUE_WHAT_IS_A_CORE_DESC,
               s, len);
         break;
      case MENU_HELP_LOADING_CONTENT:
         menu_hash_get_help(MENU_LABEL_LOAD_CONTENT,
               s, len);
         break;
      case MENU_HELP_CHANGE_VIRTUAL_GAMEPAD:
         menu_hash_get_help(MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC,
               s, len);
         break;
      case MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         menu_hash_get_help(MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC,
               s, len);
         break;
      case MENU_HELP_SCANNING_CONTENT:
         menu_hash_get_help(MENU_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC,
               s, len);
         break;
      case MENU_HELP_EXTRACT:
         menu_hash_get_help(MENU_LABEL_VALUE_EXTRACTING_PLEASE_WAIT,
               s, len);
         break;
      case MENU_HELP_NONE:
      default:
         break;
   }


   return 0;
}

static int action_iterate_info(char *s, size_t len, const char *label)
{
   size_t selection;
   uint32_t label_hash              = 0;
   menu_file_list_cbs_t *cbs        = NULL;
   menu_list_t *menu_list           = menu_list_get_ptr();

   if (!menu_list)
      return 0;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, selection);

   if (cbs->setting)
   {
      char needle[PATH_MAX_LENGTH];
      strlcpy(needle, cbs->setting->name, sizeof(needle));
      label_hash       = menu_hash_calculate(needle);
   }

   return menu_hash_get_help(label_hash, s, len);
}

static int action_iterate_menu_viewport(char *s, size_t len,
      const char *label, unsigned action, uint32_t hash)
{
   size_t selection;
   int stride_x = 1, stride_y = 1;
   menu_displaylist_info_t info     = {0};
   struct retro_game_geometry *geom = NULL;
   const char *base_msg             = NULL;
   unsigned type                    = 0;
   video_viewport_t *custom         = video_viewport_get_custom();
   menu_display_t *disp             = menu_display_get_ptr();
   menu_list_t *menu_list           = menu_list_get_ptr();
   settings_t *settings             = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (!menu_list)
      return -1;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
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
         menu_entry_go_back();

         if (hash == MENU_LABEL_CUSTOM_VIEWPORT_2)
         {
            info.list          = menu_list->menu_stack;
            info.type          = MENU_SETTINGS_CUSTOM_VIEWPORT;
            info.directory_ptr = selection;

            menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_flush_stack(menu_list, NULL, 49);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !settings->video.scale_integer)
         {
            info.list          = menu_list->menu_stack;
            strlcpy(info.label,
                  menu_hash_to_str(MENU_LABEL_CUSTOM_VIEWPORT_2),
                  sizeof(info.label));
            info.type          = 0;
            info.directory_ptr = selection;

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
         if (disp)
            disp->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(menu_list, NULL, &label, &type, NULL);

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
         base_msg = menu_hash_to_str(MENU_LABEL_VALUE_CUSTOM_VIEWPORT_1);
      else if (hash == MENU_LABEL_CUSTOM_VIEWPORT_2)
         base_msg = menu_hash_to_str(MENU_LABEL_VALUE_CUSTOM_VIEWPORT_2);

      snprintf(s, len, "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height);
   }

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
   ITERATE_TYPE_VIEWPORT,
   ITERATE_TYPE_BIND
};

static enum action_iterate_type action_iterate_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_HELP:
      case MENU_LABEL_HELP_CONTROLS:
      case MENU_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_LABEL_HELP_LOADING_CONTENT:
      case MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
      case MENU_LABEL_HELP_SCANNING_CONTENT:
         return ITERATE_TYPE_HELP;
      case MENU_LABEL_INFO_SCREEN:
         return ITERATE_TYPE_INFO;
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

/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop. 
 **/
int menu_iterate(bool render_this_frame, unsigned action)
{
   size_t selection;
   menu_entry_t entry;
   enum action_iterate_type iterate_type;
   const char *label         = NULL;
   int ret                   = 0;
   uint32_t hash             = 0;
   menu_handle_t *menu       = menu_driver_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();

   menu_list_get_last_stack(menu_list, NULL, &label, NULL, NULL);

   if (!menu || !menu_list)
      return 0;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   menu->state = 0;

   menu->menu_state.msg[0]          = '\0';

   hash = menu_hash_calculate(label);
   
   iterate_type              = action_iterate_type(hash);

   if (action != MENU_ACTION_NOOP || menu_entries_needs_refresh() || menu_display_ctl(MENU_DISPLAY_CTL_UPDATE_PENDING, NULL))
   {
      if (render_this_frame)
         BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   }

   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = action_iterate_help(menu->menu_state.msg, sizeof(menu->menu_state.msg), label);
         if (render_this_frame)
            BIT64_SET(menu->state, MENU_STATE_BLIT);
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (ret == 1)
            action = MENU_ACTION_OK;
         break;
      case ITERATE_TYPE_BIND:
         if (menu_input_bind_iterate(menu->menu_state.msg, sizeof(menu->menu_state.msg)))
         {
            menu_list_pop_stack(menu_list, &selection);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
         }
         else
            BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         if (render_this_frame)
            BIT64_SET(menu->state, MENU_STATE_BLIT);
         break;
      case ITERATE_TYPE_VIEWPORT:
         ret = action_iterate_menu_viewport(menu->menu_state.msg, sizeof(menu->menu_state.msg), label, action, hash);
         if (render_this_frame)
            BIT64_SET(menu->state, MENU_STATE_BLIT);
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         break;
      case ITERATE_TYPE_INFO:
         ret = action_iterate_info(menu->menu_state.msg, sizeof(menu->menu_state.msg), label);
         if (render_this_frame)
            BIT64_SET(menu->state, MENU_STATE_BLIT);
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         break;
      case ITERATE_TYPE_DEFAULT:
         selection = max(min(selection, menu_list_get_size(menu_list)-1), 0);

         menu_entry_get(&entry,    selection, NULL, false);
         ret = menu_entry_action(&entry, selection, (enum menu_action)action);

         if (ret)
            goto end;

         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (render_this_frame)
            BIT64_SET(menu->state, MENU_STATE_BLIT);

         /* Have to defer it so we let settings refresh. */
         if (menu->push_help_screen)
         {
            menu_displaylist_info_t info = {0};

            info.list = menu_list->menu_stack;
            strlcpy(info.label,
                  menu_hash_to_str(MENU_LABEL_HELP),
                  sizeof(info.label));

            menu_displaylist_push_list(&info, DISPLAYLIST_HELP);
         }
         break;
   }

   if (BIT64_GET(menu->state, MENU_STATE_POP_STACK) && action == MENU_ACTION_OK)
   {
      size_t new_selection_ptr = selection;
      menu_list_pop_stack(menu_list, &new_selection_ptr);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
   }
   
   if (BIT64_GET(menu->state, MENU_STATE_POST_ITERATE))
      menu_input_post_iterate(&ret, action);

end:
   if (ret)
      return -1;
   return 0;
}

int menu_iterate_render(void)
{
   bool is_idle;
   const menu_ctx_driver_t *driver = menu_ctx_driver_get_ptr();
   menu_handle_t *menu       = menu_driver_get_ptr();

   if (!menu)
      return -1;

   if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER) != BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX))
      BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);

   if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER))
      menu_display_ctl(MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG, NULL);

   if (BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX) && menu->menu_state.msg[0] != '\0')
   {
      if (driver->render_messagebox)
         driver->render_messagebox(menu->menu_state.msg);
      if (ui_companion_is_on_foreground())
      {
         const ui_companion_driver_t *ui = ui_companion_get_ptr();
         if (ui->render_messagebox)
            ui->render_messagebox(menu->menu_state.msg);
      }
   }
      
   if (BIT64_GET(menu->state, MENU_STATE_BLIT))
   {
      menu_animation_ctl(MENU_ANIMATION_CTL_UPDATE_TIME, NULL);
      if (driver->render)
         driver->render();
   }

   rarch_main_ctl(RARCH_MAIN_CTL_IS_IDLE, &is_idle);

   if (menu_driver_alive() && !is_idle)
      menu_display_ctl(MENU_DISPLAY_CTL_LIBRETRO, NULL);

   menu_driver_set_texture();

   return 0;
}
