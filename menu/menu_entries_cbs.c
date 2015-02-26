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
#include "menu_shader.h"
#include "menu_navigation.h"

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

#include "../gfx/video_viewport.h"

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH];
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_pop_stack(menu->menu_list);

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path, NULL, &type);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         menu->menu_list,
         cat_path,
         menu_label,
         type,
         menu->navigation.selection_ptr);

   return 0;
}

void menu_entries_common_load_content(bool persist)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return;

   rarch_main_command(persist ? RARCH_CMD_LOAD_CONTENT_PERSIST : RARCH_CMD_LOAD_CONTENT);

   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
   menu->msg_force = true;
}

static int archive_load(void)
{
   int ret;
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_pop_stack(menu->menu_list);

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path, NULL, &type);

   ret = rarch_defer_core(g_extern.core_info, menu_path, path, menu_label,
         menu->deferred_path, sizeof(menu->deferred_path));

   switch (ret)
   {
      case -1:
         rarch_main_command(RARCH_CMD_LOAD_CORE);
         menu_entries_common_load_content(false);
         break;
      case 0:
         menu_list_push_stack_refresh(
               menu->menu_list,
               g_settings.libretro_directory,
               "deferred_core_list",
               0,
               menu->navigation.selection_ptr);
         break;
   }

   return 0;
}

static int load_or_open_zip_iterate(unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

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

static int shader_action_parameter_toggle(unsigned type, const char *label,
      unsigned action)
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
      unsigned action)
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
            menu_navigation_set(&menu->navigation,
                  menu->navigation.selection_ptr + fast_scroll_speed, true);
         else
         {
            if ((menu_list_get_size(menu->menu_list) > 0))
                  menu_navigation_set_last(&menu->navigation);
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
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (file_list_get_size(menu->menu_list->menu_stack) == 1)
   {
      if (!strcmp(driver.menu_ctx->ident, "xmb"))
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
         if (driver.menu_ctx->list_cache)
            driver.menu_ctx->list_cache(true, action);

         if (cbs && cbs->action_content_list_switch)
            return cbs->action_content_list_switch(
                  menu->menu_list->selection_buf,
                  menu->menu_list->menu_stack,
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
      unsigned action)
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
      unsigned action)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_setting_find("video_smooth");
   if (setting)
      menu_setting_handler(setting, action);
#endif
   return 0;
}

static int action_toggle_cheat_num_passes(unsigned type, const char *label,
      unsigned action)
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
      unsigned action)
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
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (driver.video_data && driver.video_poke &&
               driver.video_poke->get_video_output_prev)
            driver.video_poke->get_video_output_prev(driver.video_data);
         break;
      case MENU_ACTION_RIGHT:
         if (driver.video_data && driver.video_poke &&
               driver.video_poke->get_video_output_next)
            driver.video_poke->get_video_output_next(driver.video_data);
         break;
   }

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

static inline struct video_shader *shader_manager_get_current_shader(const char *label, unsigned type)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return NULL;

   if (!strcmp(label, "video_shader_preset_parameters"))
      return menu->shader;
   else if (!strcmp(label, "video_shader_parameters"))
      return video_shader_driver_get_current_shader();
   return NULL;
}

static int action_bind_up_or_down_generic(unsigned type, const char *label,
      unsigned action)
{
   unsigned scroll_speed  = 0;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   scroll_speed = (max(menu->navigation.scroll.acceleration, 2) - 2) / 4 + 1;

   if (menu_list_get_size(menu->menu_list) <= 0)
      return 0;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (menu->navigation.selection_ptr >= scroll_speed)
               menu_navigation_set(&menu->navigation,
                     menu->navigation.selection_ptr - scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_set(&menu->navigation, 
                     menu_list_get_size(menu->menu_list) - 1, true);
            else
               menu_navigation_set(&menu->navigation, 0, true);
         }
         break;
      case MENU_ACTION_DOWN:
         if (menu->navigation.selection_ptr + scroll_speed < (menu_list_get_size(menu->menu_list)))
            menu_navigation_set(&menu->navigation,
                  menu->navigation.selection_ptr + scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_clear(&menu->navigation, false);
            else
               menu_navigation_set(&menu->navigation,
                     menu_list_get_size(menu->menu_list) - 1, true);
         }
         break;
   }

   return 0;
}

static int action_refresh_default(file_list_t *list, file_list_t *menu_list)
{
   int ret = 0;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   ret = menu_entries_deferred_push(list, menu_list);

   menu->need_refresh = false;

   return ret;
}

static int mouse_post_iterate(menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!menu->mouse.enable)
      return 0;

   if (menu->mouse.ptr <= menu_list_get_size(menu->menu_list)-1)
      menu_navigation_set(&menu->navigation, menu->mouse.ptr, false);

   if (menu->mouse.left)
   {
      if (!menu->mouse.oldleft)
      {
         menu->mouse.oldleft = true;

         if (cbs && cbs->action_ok)
            return cbs->action_ok(path, label, type,
                  menu->navigation.selection_ptr);
      }
   }
   else
      menu->mouse.oldleft = false;

   if (menu->mouse.right)
   {
      if (!menu->mouse.oldright)
      {
         menu->mouse.oldright = true;
         menu_list_pop_stack(menu->menu_list);
      }
   }
   else
      menu->mouse.oldright = false;

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
      RETRO_DEVICE_ID_JOYPAD_X,
   };
   char desc[ARRAY_SIZE(binds)][64];
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
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
         "Toggle Keyboard: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Core).\n"
         "Load a content file (Load Content).     \n"
         " \n"
         "See Path Settings to set directories for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
      desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6], desc[7]);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      menu_list_pop(menu->menu_list->menu_stack, NULL);

   return 0;
}

static int action_iterate_info(const char *label, unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   char needle[PATH_MAX_LENGTH];
   unsigned info_type = 0;
   rarch_setting_t *current_setting = NULL;
   file_list_t *list = NULL;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;

   list = (file_list_t*)menu->menu_list->selection_buf;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   current_setting = (rarch_setting_t*)setting_data_find_setting(
         menu->list_settings,
         list->list[menu->navigation.selection_ptr].label);

   if (current_setting)
      strlcpy(needle, current_setting->name, sizeof(needle));
   else if ((current_setting = (rarch_setting_t*)setting_data_find_setting(
               menu->list_settings,
               list->list[menu->navigation.selection_ptr].label)))
   {
      if (current_setting)
         strlcpy(needle, current_setting->name, sizeof(needle));
   }
   else
   {
      const char *lbl = NULL;
      menu_list_get_at_offset(list,
            menu->navigation.selection_ptr, NULL, &lbl,
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
      menu_list_pop(menu->menu_list->menu_stack, &menu->navigation.selection_ptr);

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
   video_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list, NULL, NULL, &type);

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
         menu_list_pop_stack(menu->menu_list);

         if (!strcmp(label, "custom_viewport_2"))
         {
            menu_list_push_stack(menu->menu_list, "", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  menu->navigation.selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(menu->menu_list);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            menu_list_push_stack(menu->menu_list, "",
                  "custom_viewport_2", 0, menu->navigation.selection_ptr);
         }
         break;

      case MENU_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            video_viewport_t vp;

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
         menu->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(menu->menu_list, NULL, &label, &type);

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
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;
   if (menu_input_bind_iterate())
      menu_list_pop_stack(menu->menu_list);
   return 0;
}

static int action_iterate_custom_bind_keyboard(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;
   if (menu_input_bind_iterate_keyboard())
      menu_list_pop_stack(menu->menu_list);
   return 0;
}

static int action_iterate_message(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(menu->message_contents);

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(menu->menu_list);

   return 0;
}

static int mouse_iterate(unsigned action)
{
   const struct retro_keybind *binds[MAX_USERS];
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!menu->mouse.enable)
      return 0;

   menu->mouse.dx = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   menu->mouse.dy = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

   menu->mouse.x += menu->mouse.dx;
   menu->mouse.y += menu->mouse.dy;

   if (menu->mouse.x < 5)
      menu->mouse.x = 5;
   if (menu->mouse.y < 5)
      menu->mouse.y = 5;
   if (menu->mouse.x > menu->frame_buf.width - 5)
      menu->mouse.x = menu->frame_buf.width - 5;
   if (menu->mouse.y > menu->frame_buf.height - 5)
      menu->mouse.y = menu->frame_buf.height - 5;

   menu->mouse.left = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

   menu->mouse.right = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

   menu->mouse.wheelup = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP)
         || menu->mouse.y == 5;

   menu->mouse.wheeldown = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
         || menu->mouse.y == menu->frame_buf.height - 5;

   return 0;
}

static int action_iterate_main(const char *label, unsigned action)
{
   int ret = 0;
   unsigned type_offset = 0;
   const char *label_offset = NULL;
   const char *path_offset = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu->menu_list->selection_buf,
            menu->navigation.selection_ptr);

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path_offset, &label_offset, &type_offset);

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

   if (menu->need_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_REFRESH;

   switch (action)
   {
      case MENU_ACTION_UP:
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_up_or_down)
            ret = cbs->action_up_or_down(type_offset, label_offset, action);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_navigation_descend_alphabet(&menu->navigation, &menu->navigation.selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ascend_alphabet(&menu->navigation, &menu->navigation.selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            return cbs->action_cancel(path_offset, label_offset, type_offset, menu->navigation.selection_ptr);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            return cbs->action_ok(path_offset, label_offset, type_offset, menu->navigation.selection_ptr);
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
            ret = cbs->action_refresh(menu->menu_list->selection_buf,
                  menu->menu_list->menu_stack);
         break;

      case MENU_ACTION_MESSAGE:
         menu->msg_force = true;
         break;

      case MENU_ACTION_SEARCH:
         menu_input_search_start();
         break;

      case MENU_ACTION_TEST:
#if 0
         menu->rdl = database_info_write_rdl_init("/home/twinaphex/roms");

         if (!menu->rdl)
            return -1;
#endif
         break;

      default:
         break;
   }

   if (ret)
      return ret;

   ret = mouse_post_iterate(cbs, path_offset, label_offset, type_offset, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   /* Have to defer it so we let settings refresh. */
   if (menu->push_start_screen)
   {
      menu_list_push_stack(menu->menu_list, "", "help", 0, 0);
      menu->push_start_screen = false;
   }

   return ret;
}

static int action_select_default(unsigned type, const char *label,
      unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;
   menu_list_push_stack(menu->menu_list, "", "info_screen",
         0, menu->navigation.selection_ptr);
   return 0;
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
   unsigned pass = 0;
   static const char *modes[] = {
      "Don't care",
      "Linear",
      "Nearest"
   };
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)pass;
   (void)modes;
   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

  pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);

  strlcpy(type_str, modes[menu->shader->pass[pass].filter],
        type_str_size);
#endif
}

static void menu_action_setting_disp_set_label_filter(
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
   strlcpy(type_str, "N/A", type_str_size);

   if (*g_settings.video.softfilter_plugin)
   strlcpy(type_str, path_basename(g_settings.video.softfilter_plugin),
         type_str_size);
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
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   snprintf(type_str, type_str_size, "%u", menu->shader->passes);
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
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)pass;
   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);
   strlcpy(type_str, "N/A", type_str_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (*menu->shader->pass[pass].source.path)
      fill_pathname_base(type_str,
            menu->shader->pass[pass].source.path, type_str_size);
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
   shader = video_shader_driver_get_current_shader();

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
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   (void)menu;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   param = &menu->shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0];

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
   unsigned pass = 0;
   unsigned scale_value = 0;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   *type_str = '\0';
   *w = 19;
   strlcpy(path_buf, path, path_buf_size);

   (void)pass;
   (void)scale_value;
   (void)menu;

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
   if (!menu->shader)
      return;

   pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
   scale_value = menu->shader->pass[pass].fbo.scale_x;

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

   if (driver.video_data && driver.video_poke &&
         driver.video_poke->get_video_output_size)
   {
      driver.video_poke->get_video_output_size(driver.video_data,
            &width, &height);
      snprintf(type_str, type_str_size, "%ux%u", width, height);
   }
   else
      strlcpy(type_str, "N/A", type_str_size);
}

static void menu_action_setting_generic_disp_set_label(
      unsigned *w, char *type_str, size_t type_str_size,
      const char *path, const char *label,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';

   if (label)
      strlcpy(type_str, label, type_str_size);
   *w = strlen(type_str);

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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FILE)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, NULL, path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(DIR)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(COMP)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(SHADER)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(PRESET)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CFILE)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(OVERLAY)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CONFIG)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FONT)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(FILTER)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(URL)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(RDB)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CURSOR)", path_buf, path_buf_size);
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
   menu_action_setting_generic_disp_set_label(w, type_str, type_str_size,
         path, "(CHEAT)", path_buf, path_buf_size);
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

static void menu_entries_cbs_init_bind_content_list_switch(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_content_list_switch = deferred_push_content_list;
}

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

static void menu_entries_cbs_init_bind_refresh(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (cbs)
      cbs->action_refresh = action_refresh_default;
}

static void menu_entries_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (cbs)
      cbs->action_iterate = action_iterate_main;
}

static void menu_entries_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
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
   else if (!strcmp(label, "video_filter"))
      cbs->action_get_representation =
         menu_action_setting_disp_set_label_filter;
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
