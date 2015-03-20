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
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_navigation.h"

#include "../retroarch.h"

static int shader_action_parameter_toggle(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_parameter *param = NULL;
   struct video_shader *shader = video_shader_driver_get_current_shader();

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
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   struct video_shader_parameter *param = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!(shader = menu->shader))
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
      unsigned action, bool wraparound)
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
      unsigned action, bool wraparound)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / RARCH_FIRST_CUSTOM_BIND;
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * RARCH_FIRST_CUSTOM_BIND);
   settings_t *settings = config_get_ptr();

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] > 0)
            settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset]--;
         break;
      case MENU_ACTION_RIGHT:
         if (settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] < RARCH_FIRST_CUSTOM_BIND)
            settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
         break;
   }

   return 0;
}

static int action_toggle_save_state(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   settings_t *settings = config_get_ptr();

   switch (action)
   {
      case MENU_ACTION_LEFT:
         /* Slot -1 is (auto) slot. */
         if (settings->state_slot >= 0)
            settings->state_slot--;
         break;
      case MENU_ACTION_RIGHT:
         settings->state_slot++;
         break;
   }

   return 0;
}

static int action_toggle_scroll(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   unsigned scroll_speed = 0, fast_scroll_speed = 0;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   scroll_speed      = (max(menu->navigation.scroll.acceleration, 2) - 2) / 4 + 1;
   fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (menu->navigation.selection_ptr > fast_scroll_speed)
            menu_navigation_set(&menu->navigation,
                  menu->navigation.selection_ptr - fast_scroll_speed, true);
         else
            menu_navigation_clear(&menu->navigation, false);
         break;
      case MENU_ACTION_RIGHT:
         if (menu->navigation.selection_ptr + fast_scroll_speed < (menu_list_get_size(menu->menu_list)))
         {
            menu_navigation_set(&menu->navigation,
                  menu->navigation.selection_ptr + fast_scroll_speed, true);
         }
         else
         {
            if ((menu_list_get_size(menu->menu_list) > 0))
            {
               menu_navigation_set_last(&menu->navigation);
            }
         }
         break;
   }

   return 0;
}

static int action_toggle_mainmenu(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   menu_file_list_cbs_t *cbs = NULL;
   unsigned push_list = 0;
   driver_t *driver = driver_get_ptr();
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (file_list_get_size(menu->menu_list->menu_stack) == 1)
   {

      if (!strcmp(driver->menu_ctx->ident, "xmb"))
      {
         menu->navigation.selection_ptr = 0;
         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (menu->categories.selection_ptr == 0)
                  break;
               push_list = 1;
               break;
            case MENU_ACTION_RIGHT:
               if (menu->categories.selection_ptr == (menu->categories.size - 1))
                  break;
               push_list = 1;
               break;
         }
      }
   }
   else 
      push_list = 2;

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu->menu_list->selection_buf,
            menu->navigation.selection_ptr);

   switch (push_list)
   {
      case 1:
         if (driver->menu_ctx->list_cache)
            driver->menu_ctx->list_cache(true, action);

         if (cbs && cbs->action_content_list_switch)
            return cbs->action_content_list_switch(
                  menu->menu_list->selection_buf,
                  menu->menu_list->menu_stack,
                  "",
                  "",
                  0);

         break;
      case 2:
         action_toggle_scroll(0, "", action, false);
         break;
      case 0:
      default:
         break;
   }

   return 0;
}

static int action_toggle_shader_scale_pass(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   
   shader = menu->shader;
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned current_scale   = shader_pass->fbo.scale_x;
            unsigned delta           = (action == MENU_ACTION_LEFT) ? 5 : 1;
            current_scale            = (current_scale + delta) % 6;

            shader_pass->fbo.valid   = current_scale;
            shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;

         }
         break;
   }
#endif
   return 0;
}

static int action_toggle_shader_filter_pass(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   
   shader = menu->shader;
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         {
            unsigned delta = (action == MENU_ACTION_LEFT) ? 2 : 1;
            shader_pass->filter = ((shader_pass->filter + delta) % 3);

         }
         break;
   }
#endif
   return 0;
}

static int action_toggle_shader_filter_default(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_setting_find("video_smooth");
   if (setting)
      menu_setting_handler(setting, action);
#endif
   return 0;
}

static int action_toggle_cheat_num_passes(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   unsigned new_size = 0;
   cheat_manager_t *cheat = g_extern.cheat;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!cheat)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (cheat->size)
            new_size = cheat->size - 1;
         menu->need_refresh = true;
         break;

      case MENU_ACTION_RIGHT:
         new_size = cheat->size + 1;
         menu->need_refresh = true;
         break;
   }

   if (menu->need_refresh)
      cheat_manager_realloc(cheat, new_size);

   return 0;
}

static int action_toggle_shader_num_passes(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   
   shader = menu->shader;
   if (!shader)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (shader->passes)
            shader->passes--;
         menu->need_refresh = true;
         break;

      case MENU_ACTION_RIGHT:
         if ((shader->passes < GFX_MAX_SHADERS))
            shader->passes++;
         menu->need_refresh = true;
         break;
   }

   if (menu->need_refresh)
      video_shader_resolve_parameters(NULL, menu->shader);

#endif
   return 0;
}

static int action_toggle_video_resolution(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   driver_t *driver = driver_get_ptr();

#if defined(__CELLOS_LV2__)
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
#else
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (driver->video_data && driver->video_poke &&
               driver->video_poke->get_video_output_prev)
         {
            driver->video_poke->get_video_output_prev(driver->video_data);
         }
         break;
      case MENU_ACTION_RIGHT:
         if (driver->video_data && driver->video_poke &&
               driver->video_poke->get_video_output_next)
         {
            driver->video_poke->get_video_output_next(driver->video_data);
         }
         break;
   }
#endif

   return 0;
}

static int core_setting_toggle(unsigned type, const char *label,
      unsigned action, bool wraparound)
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
      unsigned action, bool wraparound)
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


void menu_entries_cbs_init_bind_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label)
{
   int i;

   if (!cbs)
      return;

   if (label)
   {
      if (menu_entries_common_is_settings_entry(elem0))
      {
         cbs->action_toggle = action_toggle_scroll;
         return;
      }
   }

   cbs->action_toggle = menu_setting_set;

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
      case MENU_FILE_SHADER_PRESET:
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
         cbs->action_toggle = menu_setting_set;
   }
}
