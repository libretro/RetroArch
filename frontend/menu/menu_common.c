/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include <limits.h>
#include "menu_common.h"

#include "../../performance.h"
#include "../../file.h"
#include "menu_context.h"

#include "../../compat/posix_string.h"

rgui_handle_t *rgui;
const menu_ctx_driver_t *menu_ctx;

#ifdef HAVE_SHADER_MANAGER
void shader_manager_init(rgui_handle_t *rgui)
{
   memset(&rgui->shader, 0, sizeof(rgui->shader));
   config_file_t *conf = NULL;

   // In a multi-config setting, we can't have conflicts on rgui.cgp/rgui.glslp.
   if (*g_extern.config_path)
   {
      fill_pathname_base(rgui->default_glslp, g_extern.config_path, sizeof(rgui->default_glslp));
      path_remove_extension(rgui->default_glslp);
      strlcat(rgui->default_glslp, ".glslp", sizeof(rgui->default_glslp));
      fill_pathname_base(rgui->default_cgp, g_extern.config_path, sizeof(rgui->default_cgp));
      path_remove_extension(rgui->default_cgp);
      strlcat(rgui->default_cgp, ".cgp", sizeof(rgui->default_cgp));
   }
   else
   {
      strlcpy(rgui->default_glslp, "rgui.glslp", sizeof(rgui->default_glslp));
      strlcpy(rgui->default_cgp, "rgui.cgp", sizeof(rgui->default_cgp));
   }

   char cgp_path[PATH_MAX];

   const char *ext = path_get_extension(g_settings.video.shader_path);
   if (strcmp(ext, "glslp") == 0 || strcmp(ext, "cgp") == 0)
   {
      conf = config_file_new(g_settings.video.shader_path);
      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, &rgui->shader))
            gfx_shader_resolve_relative(&rgui->shader, g_settings.video.shader_path);
         config_file_free(conf);
      }
   }
   else if (strcmp(ext, "glsl") == 0 || strcmp(ext, "cg") == 0)
   {
      strlcpy(rgui->shader.pass[0].source.cg, g_settings.video.shader_path,
            sizeof(rgui->shader.pass[0].source.cg));
      rgui->shader.passes = 1;
   }
   else
   {
      const char *shader_dir = *g_settings.video.shader_dir ?
         g_settings.video.shader_dir : g_settings.system_directory;

      fill_pathname_join(cgp_path, shader_dir, "rgui.glslp", sizeof(cgp_path));
      conf = config_file_new(cgp_path);

      if (!conf)
      {
         fill_pathname_join(cgp_path, shader_dir, "rgui.cgp", sizeof(cgp_path));
         conf = config_file_new(cgp_path);
      }

      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, &rgui->shader))
            gfx_shader_resolve_relative(&rgui->shader, cgp_path);
         config_file_free(conf);
      }
   }
}

void shader_manager_get_str(struct gfx_shader *shader,
      char *type_str, size_t type_str_size, unsigned type)
{
   if (type == RGUI_SETTINGS_SHADER_APPLY)
      *type_str = '\0';
   else if (type == RGUI_SETTINGS_SHADER_PASSES)
      snprintf(type_str, type_str_size, "%u", shader->passes);
   else
   {
      unsigned pass = (type - RGUI_SETTINGS_SHADER_0) / 3;
      switch ((type - RGUI_SETTINGS_SHADER_0) % 3)
      {
         case 0:
            if (*shader->pass[pass].source.cg)
               fill_pathname_base(type_str,
                     shader->pass[pass].source.cg, type_str_size);
            else
               strlcpy(type_str, "N/A", type_str_size);
            break;

         case 1:
            switch (shader->pass[pass].filter)
            {
               case RARCH_FILTER_LINEAR:
                  strlcpy(type_str, "Linear", type_str_size);
                  break;

               case RARCH_FILTER_NEAREST:
                  strlcpy(type_str, "Nearest", type_str_size);
                  break;

               case RARCH_FILTER_UNSPEC:
                  strlcpy(type_str, "Don't care", type_str_size);
                  break;
            }
            break;

         case 2:
         {
            unsigned scale = shader->pass[pass].fbo.scale_x;
            if (!scale)
               strlcpy(type_str, "Don't care", type_str_size);
            else
               snprintf(type_str, type_str_size, "%ux", scale);
            break;
         }
      }
   }
}
#endif

#ifdef HAVE_FILEBROWSER
static bool directory_parse(void *data, const char *path)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   struct string_list *list = dir_list_new(path,
         filebrowser->current_dir.extensions, true);
   if(!list || list->size < 1)
      return false;
   
   dir_list_sort(list, true);

   filebrowser->current_dir.ptr   = 0;
   strlcpy(filebrowser->current_dir.directory_path,
         path, sizeof(filebrowser->current_dir.directory_path));

   if (filebrowser->list)
      dir_list_free(filebrowser->list);

   filebrowser->list = list;

   return true;

}

static void filebrowser_free(void *data)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;

   dir_list_free(filebrowser->list);
   filebrowser->list = NULL;
   filebrowser->current_dir.ptr   = 0;
   free(filebrowser);
}

void filebrowser_set_root_and_ext(void *data, const char *ext, const char *root_dir)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   
   if (ext)
      strlcpy(filebrowser->current_dir.extensions, ext,
            sizeof(filebrowser->current_dir.extensions));

   strlcpy(filebrowser->current_dir.root_dir,
         root_dir, sizeof(filebrowser->current_dir.root_dir));
   filebrowser_iterate(filebrowser, FILEBROWSER_ACTION_RESET);
}

#define GET_CURRENT_PATH(browser) (browser->list->elems[browser->current_dir.ptr].data)

bool filebrowser_iterate(void *data, unsigned action)
{
   filebrowser_t *filebrowser = (filebrowser_t*)data;
   bool ret = true;
   unsigned entries_to_scroll = 19;

   switch(action)
   {
      case FILEBROWSER_ACTION_UP:
         filebrowser->current_dir.ptr--;
         if (filebrowser->current_dir.ptr >= filebrowser->list->size)
            filebrowser->current_dir.ptr = filebrowser->list->size - 1;
         break;
      case FILEBROWSER_ACTION_DOWN:
         filebrowser->current_dir.ptr++;
         if (filebrowser->current_dir.ptr >= filebrowser->list->size)
            filebrowser->current_dir.ptr = 0;
         break;
      case FILEBROWSER_ACTION_LEFT:
         if (filebrowser->current_dir.ptr <= 5)
            filebrowser->current_dir.ptr = 0;
         else
            filebrowser->current_dir.ptr -= 5;
         break;
      case FILEBROWSER_ACTION_RIGHT:
         filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 5, 
         filebrowser->list->size-1));
         break;
      case FILEBROWSER_ACTION_SCROLL_UP:
         if (filebrowser->current_dir.ptr <= entries_to_scroll)
            filebrowser->current_dir.ptr= 0;
         else
            filebrowser->current_dir.ptr -= entries_to_scroll;
         break;
      case FILEBROWSER_ACTION_SCROLL_DOWN:
         filebrowser->current_dir.ptr = (min(filebrowser->current_dir.ptr + 
         entries_to_scroll, filebrowser->list->size-1));
         break;
      case FILEBROWSER_ACTION_OK:
         ret = directory_parse(filebrowser, GET_CURRENT_PATH(filebrowser));
         break;
      case FILEBROWSER_ACTION_CANCEL:
         fill_pathname_parent_dir(filebrowser->current_dir.directory_path,
               filebrowser->current_dir.directory_path,
               sizeof(filebrowser->current_dir.directory_path));

         ret = directory_parse(filebrowser, filebrowser->current_dir.directory_path);
         break;
      case FILEBROWSER_ACTION_RESET:
         ret = directory_parse(filebrowser, filebrowser->current_dir.root_dir);
         break;
      case FILEBROWSER_ACTION_RESET_CURRENT_DIR:
         ret = directory_parse(filebrowser, filebrowser->current_dir.directory_path);
         break;
      case FILEBROWSER_ACTION_PATH_ISDIR:
         ret = filebrowser->list->elems[filebrowser->current_dir.ptr].attr.b;
         break;
      case FILEBROWSER_ACTION_NOOP:
      default:
         break;
   }

   if (ret)
      strlcpy(filebrowser->current_dir.path, GET_CURRENT_PATH(filebrowser),
         sizeof(filebrowser->current_dir.path));

   return ret;
}

void filebrowser_update(void *data, uint64_t action_ori, const char *extensions)
{
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;
   bool ret = true;

   switch (action_ori)
   {
      case RGUI_ACTION_DOWN:
         action = FILEBROWSER_ACTION_DOWN;
         break;
      case RGUI_ACTION_UP:
         action = FILEBROWSER_ACTION_UP;
         break;
      case RGUI_ACTION_RIGHT:
         action = FILEBROWSER_ACTION_RIGHT;
         break;
      case RGUI_ACTION_LEFT:
         action = FILEBROWSER_ACTION_LEFT;
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         action = FILEBROWSER_ACTION_SCROLL_DOWN;
         break;
      case RGUI_ACTION_SCROLL_UP:
         action = FILEBROWSER_ACTION_SCROLL_UP;
         break;
      case RGUI_ACTION_CANCEL:
         {
            char tmp_str[PATH_MAX];
            fill_pathname_parent_dir(tmp_str, rgui->browser->current_dir.directory_path, sizeof(tmp_str));

            if (tmp_str[0] != '\0')
               action = FILEBROWSER_ACTION_CANCEL;
         }
         break;
      case RGUI_ACTION_START:
         action = FILEBROWSER_ACTION_RESET;
         filebrowser_set_root_and_ext(rgui->browser, NULL, default_paths.filesystem_root_dir);
         strlcpy(rgui->browser->current_dir.extensions, extensions,
               sizeof(rgui->browser->current_dir.extensions));
#ifdef HAVE_RMENU_XUI
         filebrowser_fetch_directory_entries(1ULL << RETRO_DEVICE_ID_JOYPAD_B);
#endif
         break;
   }

   if (action != FILEBROWSER_ACTION_NOOP)
      ret = filebrowser_iterate(rgui->browser, action);

   if (!ret)
      msg_queue_push(g_extern.msg_queue, "ERROR - Failed to open directory.", 1, 180);
}
#endif

struct rgui_file
{
   char *path;
   unsigned type;
   size_t directory_ptr;
};

void rgui_list_push(void *userdata,
      const char *path, unsigned type, size_t directory_ptr)
{
   rgui_list_t *list = (rgui_list_t*)userdata;

   if (!list)
      return;

   if (list->size >= list->capacity)
   {
      list->capacity++;
      list->capacity *= 2;
      list->list = (struct rgui_file*)realloc(list->list, list->capacity * sizeof(struct rgui_file));
   }

   list->list[list->size].path = strdup(path);
   list->list[list->size].type = type;
   list->list[list->size].directory_ptr = directory_ptr;
   list->size++;
}

void rgui_list_pop(rgui_list_t *list, size_t *directory_ptr)
{
   if (!(list->size == 0))
      free(list->list[--list->size].path);

   if (directory_ptr)
      *directory_ptr = list->list[list->size].directory_ptr;
}

void rgui_list_free(rgui_list_t *list)
{
   for (size_t i = 0; i < list->size; i++)
      free(list->list[i].path);
   free(list->list);
   free(list);
}

void rgui_list_clear(rgui_list_t *list)
{
   for (size_t i = 0; i < list->size; i++)
      free(list->list[i].path);
   list->size = 0;
}

void rgui_list_get_at_offset(const rgui_list_t *list, size_t index,
      const char **path, unsigned *file_type)
{
   if (path)
      *path = list->list[index].path;
   if (file_type)
      *file_type = list->list[index].type;
}

void rgui_list_get_last(const rgui_list_t *list,
      const char **path, unsigned *file_type)
{
   if (list->size)
      rgui_list_get_at_offset(list, list->size - 1, path, file_type);
}

void menu_rom_history_push(const char *path,
      const char *core_path,
      const char *core_name)
{
   if (rgui->history)
      rom_history_push(rgui->history, path, core_path, core_name);
}

void menu_rom_history_push_current(void)
{
   // g_extern.fullpath can be relative here.
   // Ensure we're pushing absolute path.

   char tmp[PATH_MAX];

   // We loaded a zip, and fullpath points to the extracted file.
   // Look at basename instead.
   if (g_extern.rom_file_temporary)
      snprintf(tmp, sizeof(tmp), "%s.zip", g_extern.basename);
   else
      strlcpy(tmp, g_extern.fullpath, sizeof(tmp));

   if (*tmp)
      path_resolve_realpath(tmp, sizeof(tmp));

   menu_rom_history_push(*tmp ? tmp : NULL,
         g_settings.libretro,
         g_extern.system.info.library_name);
}

void load_menu_game_prepare(void)
{
   if (*g_extern.fullpath || rgui->load_no_rom)
   {
      if (*g_extern.fullpath &&
            g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

      menu_rom_history_push(*g_extern.fullpath ? g_extern.fullpath : NULL,
            g_settings.libretro,
            rgui->info.library_name ? rgui->info.library_name : "");
   }

#ifdef HAVE_RGUI
   // redraw RGUI frame
   rgui->old_input_state = rgui->trigger_state = 0;
   rgui->do_held = false;
   rgui->msg_force = true;

   if (menu_ctx && menu_ctx->iterate)
      menu_ctx->iterate(rgui, RGUI_ACTION_NOOP);
#endif

   // Draw frame for loading message
   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, rgui->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   if (driver.video)
      rarch_render_cached_frame();

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);
}

void load_menu_game_history(unsigned game_index)
{
   const char *path = NULL;
   const char *core_path = NULL;
   const char *core_name = NULL;

   rom_history_get_index(rgui->history,
         game_index, &path, &core_path, &core_name);

   rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)core_path);

   if (path)
      rgui->load_no_rom = false;
   else
      rgui->load_no_rom = true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

#if defined(HAVE_DYNAMIC)
   libretro_free_system_info(&rgui->info);
   libretro_get_system_info(g_settings.libretro, &rgui->info, NULL);
#endif
}

static void menu_init_history(void)
{
   if (rgui->history)
   {
      rom_history_free(rgui->history);
      rgui->history = NULL;
   }

   if (*g_extern.config_path)
   {
      char history_path[PATH_MAX];
      if (*g_settings.game_history_path)
         strlcpy(history_path, g_settings.game_history_path, sizeof(history_path));
      else
      {
         fill_pathname_resolve_relative(history_path, g_extern.config_path,
               ".retroarch-game-history.txt", sizeof(history_path));
      }

      RARCH_LOG("[RGUI]: Opening history: %s.\n", history_path);
      rgui->history = rom_history_init(history_path, g_settings.game_history_size);
   }
}

static void menu_update_libretro_info(void)
{
#ifdef HAVE_DYNAMIC
   *rgui->libretro_dir = '\0';
   libretro_free_system_info(&rgui->info);
   if (path_is_directory(g_settings.libretro))
      strlcpy(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
   else if (*g_settings.libretro)
   {
      fill_pathname_basedir(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
      libretro_get_system_info(g_settings.libretro, &rgui->info, NULL);
   }
#else
   retro_get_system_info(&rgui->info);
#endif
}

bool load_menu_game(void)
{
   if (g_extern.main_is_init)
      rarch_main_deinit();

   struct rarch_main_wrap args = {0};

   args.verbose       = g_extern.verbose;
   args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
   args.sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
   args.state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
   args.rom_path      = *g_extern.fullpath ? g_extern.fullpath : NULL;
   args.libretro_path = *g_settings.libretro ? g_settings.libretro : NULL;
   args.no_rom        = rgui->load_no_rom;
   rgui->load_no_rom  = false;

   if (rarch_main_init_wrap(&args) == 0)
   {
      RARCH_LOG("rarch_main_init_wrap() succeeded.\n");
      // Update menu state which depends on config.
      menu_update_libretro_info();
      menu_init_history();
      shader_manager_init(rgui);
      return true;
   }
   else
   {
      char name[PATH_MAX];
      char msg[PATH_MAX];
      fill_pathname_base(name, g_extern.fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      msg_queue_push(g_extern.msg_queue, msg, 1, 90);
      rgui->msg_force = true;
      RARCH_ERR("rarch_main_init_wrap() failed.\n");
      return false;
   }
}

void menu_init(void)
{
   if (!menu_ctx_init_first(&menu_ctx, &rgui))
   {
      RARCH_ERR("Could not initialize menu.\n");
      rarch_fail(1, "menu_init()");
   }

   rgui->trigger_state = 0;
   rgui->old_input_state = 0;
   rgui->do_held = false;
   rgui->frame_buf_show = true;
   rgui->current_pad = 0;

   menu_update_libretro_info();

#ifdef HAVE_FILEBROWSER
   if (!(strlen(g_settings.rgui_browser_directory) > 0))
      strlcpy(g_settings.rgui_browser_directory, default_paths.filebrowser_startup_dir,
            sizeof(g_settings.rgui_browser_directory));

   rgui->browser = (filebrowser_t*)calloc(1, sizeof(*(rgui->browser)));

   if (rgui->browser == NULL)
   {
      RARCH_ERR("Could not initialize filebrowser.\n");
      rarch_fail(1, "menu_init()");
   }

   strlcpy(rgui->browser->current_dir.extensions, rgui->info.valid_extensions,
         sizeof(rgui->browser->current_dir.extensions));

   // Look for zips to extract as well.
   if (*rgui->info.valid_extensions)
   {
      strlcat(rgui->browser->current_dir.extensions, "|zip",
         sizeof(rgui->browser->current_dir.extensions));
   }

   strlcpy(rgui->browser->current_dir.root_dir, g_settings.rgui_browser_directory,
         sizeof(rgui->browser->current_dir.root_dir));

   filebrowser_iterate(rgui->browser, FILEBROWSER_ACTION_RESET);
#endif

#ifdef HAVE_SHADER_MANAGER
   shader_manager_init(rgui);
#endif

   menu_init_history();
   rgui->last_time = rarch_get_time_usec();
}

void menu_free(void)
{
   if (menu_ctx && menu_ctx->free)
      menu_ctx->free(rgui);

#ifdef HAVE_FILEBROWSER
   filebrowser_free(rgui->browser);
#endif

   rom_history_free(rgui->history);

   free(rgui);
}

void menu_ticker_line(char *buf, size_t len, unsigned index, const char *str, bool selected)
{
   size_t str_len = strlen(str);
   if (str_len <= len)
   {
      strlcpy(buf, str, len + 1);
      return;
   }

   if (!selected)
   {
      strlcpy(buf, str, len + 1 - 3);
      strlcat(buf, "...", len + 1);
   }
   else
   {
      // Wrap long strings in options with some kind of ticker line.
      unsigned ticker_period = 2 * (str_len - len) + 4;
      unsigned phase = index % ticker_period;

      unsigned phase_left_stop = 2;
      unsigned phase_left_moving = phase_left_stop + (str_len - len);
      unsigned phase_right_stop = phase_left_moving + 2;

      unsigned left_offset = phase - phase_left_stop;
      unsigned right_offset = (str_len - len) - (phase - phase_right_stop);

      // Ticker period: [Wait at left (2 ticks), Progress to right (type_len - w), Wait at right (2 ticks), Progress to left].
      if (phase < phase_left_stop)
         strlcpy(buf, str, len + 1);
      else if (phase < phase_left_moving)
         strlcpy(buf, str + left_offset, len + 1);
      else if (phase < phase_right_stop)
         strlcpy(buf, str + str_len - len, len + 1);
      else
         strlcpy(buf, str + right_offset, len + 1);
   }
}

#ifndef HAVE_RMENU_XUI
#if defined(HAVE_RMENU) || defined(HAVE_RGUI)
static uint64_t rgui_input(void)
{
   uint64_t input_state = 0;

   static const struct retro_keybind *binds[] = { g_settings.input.binds[0] };

   for (unsigned i = 0; i < RETRO_DEVICE_ID_JOYPAD_R2; i++)
   {
      input_state |= input_input_state_func(binds,
            0, RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
#ifdef HAVE_OVERLAY
      input_state |= (driver.overlay_state.buttons & (UINT64_C(1) << i)) ? (1ULL << i) : 0;
#endif
   }

   input_state |= input_key_pressed_func(RARCH_MENU_TOGGLE) ? (1ULL << RARCH_MENU_TOGGLE) : 0;

   rgui->trigger_state = input_state & ~rgui->old_input_state;

   rgui->do_held = (input_state & (
            (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_R)
#if defined(HAVE_RMENU)
            | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP)
            | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN)
            | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT)
            | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT)
            | (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP)
            | (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN)
            | (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT)
            | (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT)
#endif
            )) && !(input_state & (1ULL << RARCH_MENU_TOGGLE));

   return input_state;
}

bool menu_iterate(void)
{
   rarch_time_t time, delta, target_msec, sleep_msec;
   rgui_action_t action;
   static bool initial_held = true;
   static bool first_held = false;
   uint64_t input_state = 0;
   int input_entry_ret = 0;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_PREINIT))
   {
      rgui->need_refresh = true;
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_PREINIT);
      rgui->old_input_state |= 1ULL << RARCH_MENU_TOGGLE;
   }

   rarch_input_poll();
#ifdef HAVE_OVERLAY
   rarch_check_overlay();
#endif

   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      goto deinit;
   }

   input_state = rgui_input();

   if (rgui->do_held)
   {
      if (!first_held)
      {
         first_held = true;
         rgui->delay_timer = initial_held ? 12 : 6;
         rgui->delay_count = 0;
      }

      if (rgui->delay_count >= rgui->delay_timer)
      {
         first_held = false;
         rgui->trigger_state = input_state;
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
   }

   rgui->delay_count++;
   rgui->old_input_state = input_state;

   action = RGUI_ACTION_NOOP;

   // don't run anything first frame, only capture held inputs for old_input_state
   if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      action = RGUI_ACTION_UP;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
      action = RGUI_ACTION_SCROLL_UP;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
      action = RGUI_ACTION_SCROLL_DOWN;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      action = RGUI_ACTION_CANCEL;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      action = RGUI_ACTION_OK;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      action = RGUI_ACTION_START;

   if (menu_ctx && menu_ctx->iterate)
      input_entry_ret = menu_ctx->iterate(rgui, action);

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, rgui->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   rarch_render_cached_frame();

   // Throttle in case VSync is broken (avoid 1000+ FPS RGUI).
   time = rarch_get_time_usec();
   delta = (time - rgui->last_time) / 1000;
   target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   sleep_msec = target_msec - delta;
   if (sleep_msec > 0)
      rarch_sleep((unsigned int)sleep_msec);
   rgui->last_time = rarch_get_time_usec();

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);

   if (rgui_input_postprocess(rgui, rgui->old_input_state) || input_entry_ret)
      goto deinit;

   return true;

deinit:
   return false;
}
#endif
#endif

// Quite intrusive and error prone.
// Likely to have lots of small bugs.
// Cleanly exit the main loop to ensure that all the tiny details get set properly.
// This should mitigate most of the smaller bugs.
bool menu_replace_config(const char *path)
{
   if (strcmp(path, g_extern.config_path) == 0)
      return false;

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;

   // Load dummy core.
   *g_extern.fullpath = '\0';
   *g_settings.libretro = '\0'; // Load core in new config.
   g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
   rgui->load_no_rom = false;

   return true;
}

// Save a new config to a file. Filename is based on heuristics to avoid typing.
bool menu_save_new_config(void)
{
   char config_dir[PATH_MAX];
   *config_dir = '\0';

   if (*g_settings.rgui_config_directory)
      strlcpy(config_dir, g_settings.rgui_config_directory, sizeof(config_dir));
   else if (*g_extern.config_path) // Fallback
      fill_pathname_basedir(config_dir, g_extern.config_path, sizeof(config_dir));
   else
   {
      const char *msg = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_ERR("%s\n", msg);
      return false;
   }

   bool found_path = false;
   char config_name[PATH_MAX];
   char config_path[PATH_MAX];
   if (*g_settings.libretro && !path_is_directory(g_settings.libretro) && path_file_exists(g_settings.libretro)) // Infer file name based on libretro core.
   {
      // In case of collision, find an alternative name.
      for (unsigned i = 0; i < 16; i++)
      {
         fill_pathname_base(config_name, g_settings.libretro, sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));

         char tmp[64];
         *tmp = '\0';
         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   // Fallback to system time ...
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));
   }

   char msg[512];
   bool ret;
   if (config_save_file(config_path))
   {
      strlcpy(g_extern.config_path, config_path, sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".", config_path);
      RARCH_LOG("%s\n", msg);
      ret = true;
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".", config_path);
      RARCH_ERR("%s\n", msg);
      ret = false;
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   return ret;
}


