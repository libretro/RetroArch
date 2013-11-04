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
#include <ctype.h>
#include "menu_common.h"

#include "../../gfx/gfx_common.h"
#include "../../performance.h"
#include "../../driver.h"
#include "../../file.h"
#include "../../file_ext.h"
#include "menu_context.h"
#include "../../input/input_common.h"

#include "../../compat/posix_string.h"

rgui_handle_t *rgui;
const menu_ctx_driver_t *menu_ctx;

//forward decl
static int rgui_iterate(void *data, unsigned action);

#if defined(HAVE_RGUI)
#define menu_iterate_func(a, b) rgui_iterate(a, b)
#elif defined(HAVE_RMENU)
#define menu_iterate_func(a, b) rmenu_iterate(a, b)
#elif defined(HAVE_RMENU_XUI)
#define menu_iterate_func(a, b) rmenu_xui_iterate(a, b)
#endif

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

void shader_manager_set_preset(struct gfx_shader *shader, enum rarch_shader_type type, const char *path)
{
   RARCH_LOG("Setting RGUI shader: %s.\n", path ? path : "N/A (stock)");
   bool ret = video_set_shader_func(type, path);
   if (ret)
   {
      // Makes sure that we use RGUI CGP shader on driver reinit.
      // Only do this when the cgp actually works to avoid potential errors.
      strlcpy(g_settings.video.shader_path, path ? path : "",
            sizeof(g_settings.video.shader_path));
      g_settings.video.shader_enable = true;

      if (path && shader)
      {
         // Load stored CGP into RGUI menu on success.
         // Used when a preset is directly loaded.
         // No point in updating when the CGP was created from RGUI itself.
         config_file_t *conf = config_file_new(path);
         if (conf)
         {
            gfx_shader_read_conf_cgp(conf, shader);
            gfx_shader_resolve_relative(shader, path);
            config_file_free(conf);
         }

         rgui->need_refresh = true;
      }
   }
   else
   {
      RARCH_ERR("Setting RGUI CGP failed.\n");
      g_settings.video.shader_enable = false;
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

   if (g_extern.system.no_game || *tmp)
      menu_rom_history_push(*tmp ? tmp : NULL,
            g_settings.libretro,
            g_extern.system.info.library_name);
}

void load_menu_game_prepare(void)
{
   if (*g_extern.fullpath || rgui->load_no_rom)
   {
      if (*g_extern.fullpath)
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

#ifdef RARCH_CONSOLE
      if (g_extern.system.no_game || *g_extern.fullpath)
#endif
      menu_rom_history_push(*g_extern.fullpath ? g_extern.fullpath : NULL,
            g_settings.libretro,
            rgui->info.library_name ? rgui->info.library_name : "");
   }

#ifdef HAVE_RGUI
   // redraw RGUI frame
   rgui->old_input_state = rgui->trigger_state = 0;
   rgui->do_held = false;
   rgui->msg_force = true;

   if (menu_ctx)
      menu_iterate_func(rgui, RGUI_ACTION_NOOP);
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
   *rgui->libretro_dir = '\0';
#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   if (path_is_directory(g_settings.libretro))
      strlcpy(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
   else if (*g_settings.libretro)
   {
      fill_pathname_basedir(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
#ifdef HAVE_DYNAMIC
      libretro_get_system_info(g_settings.libretro, &rgui->info, NULL);
#endif
   }

#ifndef HAVE_DYNAMIC
   retro_get_system_info(&rgui->info);
#endif

   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
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
#ifdef HAVE_SHADER_MANAGER
      shader_manager_init(rgui);
#endif
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

   strlcpy(rgui->base_path, g_settings.rgui_browser_directory, sizeof(rgui->base_path));
   rgui->menu_stack = (rgui_list_t*)calloc(1, sizeof(rgui_list_t));
   rgui->selection_buf = (rgui_list_t*)calloc(1, sizeof(rgui_list_t));
   rgui_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);
   rgui->selection_ptr = 0;
   rgui->push_start_screen = g_settings.rgui_show_start_screen;
   g_settings.rgui_show_start_screen = false;
   menu_populate_entries(rgui, RGUI_SETTINGS);

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

   // Look for zips to extract as well.
   if (*rgui->info.valid_extensions)
   {
      strlcpy(rgui->browser->current_dir.extensions, rgui->info.valid_extensions,
            sizeof(rgui->browser->current_dir.extensions));
      strlcat(rgui->browser->current_dir.extensions, "|zip",
         sizeof(rgui->browser->current_dir.extensions));
   }

   strlcpy(rgui->browser->current_dir.root_dir, g_settings.rgui_browser_directory,
         sizeof(rgui->browser->current_dir.root_dir));

   filebrowser_iterate(rgui->browser, RGUI_ACTION_START);
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

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   rgui_list_free(rgui->menu_stack);
   rgui_list_free(rgui->selection_buf);

#ifdef HAVE_FILEBROWSER
   filebrowser_free(rgui->browser);
#endif

   rom_history_free(rgui->history);
   core_info_list_free(rgui->core_info);

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

#if defined(HAVE_RMENU) || defined(HAVE_RGUI) || defined(HAVE_RMENU_XUI)
static uint64_t rgui_input(void)
{
   unsigned i;
   uint64_t input_state = 0;

   static const struct retro_keybind *binds[] = { g_settings.input.binds[0] };

   for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_R2; i++)
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
            )) && !(input_state & (1ULL << RARCH_MENU_TOGGLE));

   return input_state;
}

// This only makes sense for PC so far.
// Consoles use set_keybind callbacks instead.
static int rgui_custom_bind_iterate(rgui_handle_t *rgui, unsigned action)
{
   (void)action; // Have to ignore action here. Only bind that should work here is Quit RetroArch or something like that.

   if (menu_ctx && menu_ctx->render)
      menu_ctx->render(rgui);

   char msg[256];
   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)", input_config_bind_map[rgui->binds.begin - RGUI_SETTINGS_BIND_BEGIN].desc);

   if (menu_ctx && menu_ctx->render_messagebox)
      menu_ctx->render_messagebox(rgui, msg);

   struct rgui_bind_state binds = rgui->binds;
   menu_poll_bind_state(&binds);

   if ((binds.skip && !rgui->binds.skip) || menu_poll_find_trigger(&rgui->binds, &binds))
   {
      binds.begin++;
      if (binds.begin <= binds.last)
         binds.target++;
      else
         rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);

      // Avoid new binds triggering things right away.
      rgui->trigger_state = 0;
      rgui->old_input_state = -1ULL;
   }
   rgui->binds = binds;
   return 0;
}

static int rgui_start_screen_iterate(rgui_handle_t *rgui, unsigned action)
{
   if (menu_ctx && menu_ctx->render)
      menu_ctx->render(rgui);
   unsigned i;
   char msg[1024];

   char desc[6][64];
   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RARCH_MENU_TOGGLE,
      RARCH_QUIT_KEY,
   };

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      if (driver.input && driver.input->set_keybinds)
      {
         struct platform_bind key_label;
         strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
         key_label.joykey = g_settings.input.binds[0][binds[i]].joykey;
         driver.input->set_keybinds(&key_label, 0, 0, 0, 1ULL << KEYBINDS_ACTION_GET_BIND_LABEL);
         strlcpy(desc[i], key_label.desc, sizeof(desc[i]));
      }
      else
      {
         const struct retro_keybind *bind = &g_settings.input.binds[0][binds[i]];
         input_get_bind_string(desc[i], bind, sizeof(desc[i]));
      }
   }

   snprintf(msg, sizeof(msg),
         "-- Welcome to RetroArch / RGUI --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic RGUI controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "Enter/Exit RGUI: %-20s\n"
         " Exit RetroArch: %-20s\n"
         " \n"

         "To play a game:\n"
         "Load a libretro core (Core).\n"
         "Load a ROM (Load Game).     \n"
         " \n"

         "See Path Options to set directories\n"
         "for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
         desc[0], desc[1], desc[2], desc[3], desc[4], desc[5]);

   if (menu_ctx && menu_ctx->render_messagebox)
      menu_ctx->render_messagebox(rgui, msg);

   if (action == RGUI_ACTION_OK)
      rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
   return 0;
}

static int rgui_viewport_iterate(rgui_handle_t *rgui, unsigned action)
{
   rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;

   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, NULL, &menu_type);

   struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   int stride_x = g_settings.video.scale_integer ?
      geom->base_width : 1;
   int stride_y = g_settings.video.scale_integer ?
      geom->base_height : 1;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_DOWN:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y += stride_y;
            if (custom->height >= (unsigned)stride_y)
               custom->height -= stride_y;
         }
         else
            custom->height += stride_y;

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_LEFT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_RIGHT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x += stride_x;
            if (custom->width >= (unsigned)stride_x)
               custom->width -= stride_x;
         }
         else
            custom->width += stride_x;

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_CANCEL:
         rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
         {
            rgui_list_push(rgui->menu_stack, "",
                  RGUI_SETTINGS_CUSTOM_VIEWPORT,
                  rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_OK:
         rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            rgui_list_push(rgui->menu_stack, "",
                  RGUI_SETTINGS_CUSTOM_VIEWPORT_2,
                  rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            rarch_viewport_t vp;
            driver.video->viewport_info(driver.video_data, &vp);

            if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
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

            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   rgui_list_get_last(rgui->menu_stack, NULL, &menu_type);

   if (menu_ctx && menu_ctx->render)
      menu_ctx->render(rgui);

   const char *base_msg = NULL;
   char msg[64];

   if (g_settings.video.scale_integer)
   {
      custom->x = 0;
      custom->y = 0;
      custom->width = ((custom->width + geom->base_width - 1) / geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;

      base_msg = "Set scale";
      snprintf(msg, sizeof(msg), "%s (%4ux%4u, %u x %u scale)",
            base_msg,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height); 
   }
   else
   {
      if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
         base_msg = "Set Bottom-Right Corner";

      snprintf(msg, sizeof(msg), "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height); 
   }

   if (menu_ctx && menu_ctx->render_messagebox)
      menu_ctx->render_messagebox(rgui, msg);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   if (driver.video_poke->apply_state_changes)
      driver.video_poke->apply_state_changes(driver.video_data);

   return 0;
}

static int rgui_settings_iterate(rgui_handle_t *rgui, unsigned action)
{
   rgui->frame_buf_pitch = rgui->width * 2;
   unsigned type = 0;
   const char *label = NULL;
   if (action != RGUI_ACTION_REFRESH)
      rgui_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &label, &type);

   if (type == RGUI_SETTINGS_CORE)
   {
#if defined(HAVE_DYNAMIC)
      label = rgui->libretro_dir;
#elif defined(HAVE_LIBRETRO_MANAGEMENT)
      label = default_paths.core_dir;
#else
      label = ""; // Shouldn't happen ...
#endif
   }
   else if (type == RGUI_SETTINGS_CONFIG)
      label = g_settings.rgui_config_directory;
   else if (type == RGUI_SETTINGS_DISK_APPEND)
      label = rgui->base_path;

   const char *dir = NULL;
   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh)
      action = RGUI_ACTION_NOOP;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr > 0)
            rgui->selection_ptr--;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + 1 < rgui->selection_buf->size)
            rgui->selection_ptr++;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if ((type == RGUI_SETTINGS_OPEN_FILEBROWSER || type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE)
               && action == RGUI_ACTION_OK)
         {
            rgui->defer_core = type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE;
            rgui_list_push(rgui->menu_stack, rgui->base_path, RGUI_FILE_DIRECTORY, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if ((type == RGUI_SETTINGS_OPEN_HISTORY || menu_type_is(type) == RGUI_FILE_DIRECTORY) && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->menu_stack, "", type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if ((menu_type_is(type) == RGUI_SETTINGS || type == RGUI_SETTINGS_CORE || type == RGUI_SETTINGS_CONFIG || type == RGUI_SETTINGS_DISK_APPEND) && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->menu_stack, label, type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if (type == RGUI_SETTINGS_CUSTOM_VIEWPORT && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->menu_stack, "", type, rgui->selection_ptr);

            // Start with something sane.
            rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
            driver.video->viewport_info(driver.video_data, custom);
            aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
               (float)custom->width / custom->height;

            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data,
                     g_settings.video.aspect_ratio_idx);
         }
         else
         {
            int ret = rgui_settings_toggle_setting(rgui, type, action, menu_type);
            if (ret)
               return ret;
         }
         break;

      case RGUI_ACTION_REFRESH:
         rgui->selection_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh && !(menu_type == RGUI_FILE_DIRECTORY ||
            menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY ||
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY))
   {
      rgui->need_refresh = false;
      if (
               menu_type == RGUI_SETTINGS_INPUT_OPTIONS
            || menu_type == RGUI_SETTINGS_PATH_OPTIONS
            || menu_type == RGUI_SETTINGS_OPTIONS
            || menu_type == RGUI_SETTINGS_DRIVERS
            || menu_type == RGUI_SETTINGS_CORE_OPTIONS
            || menu_type == RGUI_SETTINGS_AUDIO_OPTIONS
            || menu_type == RGUI_SETTINGS_DISK_OPTIONS
            || menu_type == RGUI_SETTINGS_VIDEO_OPTIONS 
            || menu_type == RGUI_SETTINGS_SHADER_OPTIONS
            )
         menu_populate_entries(rgui, menu_type);
      else
         menu_populate_entries(rgui, RGUI_SETTINGS);
   }

   if (menu_ctx && menu_ctx->render)
      menu_ctx->render(rgui);

   // Have to defer it so we let settings refresh.
   if (rgui->push_start_screen)
   {
      rgui->push_start_screen = false;
      rgui_list_push(rgui->menu_stack, "", RGUI_START_SCREEN, 0);
   }

   return 0;
}

static inline void rgui_descend_alphabet(rgui_handle_t *rgui, size_t *ptr_out)
{
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == 0)
      return;
   size_t i = rgui->scroll_indices_size - 1;
   while (i && rgui->scroll_indices[i - 1] >= ptr)
      i--;
   *ptr_out = rgui->scroll_indices[i - 1];
}

static inline void rgui_ascend_alphabet(rgui_handle_t *rgui, size_t *ptr_out)
{
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == rgui->scroll_indices[rgui->scroll_indices_size - 1])
      return;
   size_t i = 0;
   while (i < rgui->scroll_indices_size - 1 && rgui->scroll_indices[i + 1] <= ptr)
      i++;
   *ptr_out = rgui->scroll_indices[i + 1];
}

static void rgui_flush_menu_stack_type(rgui_handle_t *rgui, unsigned final_type)
{
   unsigned type;
   type = 0;
   rgui->need_refresh = true;
   rgui_list_get_last(rgui->menu_stack, NULL, &type);
   while (type != final_type)
   {
      rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
      rgui_list_get_last(rgui->menu_stack, NULL, &type);
   }
}

static int rgui_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const char *dir = 0;
   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);
   int ret = 0;

   if (menu_ctx && menu_ctx->set_texture)
      menu_ctx->set_texture(rgui, false);

   if (menu_type == RGUI_START_SCREEN)
      return rgui_start_screen_iterate(rgui, action);
   else if (menu_type_is(menu_type) == RGUI_SETTINGS)
      return rgui_settings_iterate(rgui, action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
      return rgui_viewport_iterate(rgui, action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_BIND)
      return rgui_custom_bind_iterate(rgui, action);

   if (rgui->need_refresh && action != RGUI_ACTION_MESSAGE)
      action = RGUI_ACTION_NOOP;

   unsigned scroll_speed = (max(rgui->scroll_accel, 2) - 2) / 4 + 1;
   unsigned fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr >= scroll_speed)
            rgui->selection_ptr -= scroll_speed;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + scroll_speed < rgui->selection_buf->size)
            rgui->selection_ptr += scroll_speed;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_LEFT:
         if (rgui->selection_ptr > fast_scroll_speed)
            rgui->selection_ptr -= fast_scroll_speed;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_RIGHT:
         if (rgui->selection_ptr + fast_scroll_speed < rgui->selection_buf->size)
            rgui->selection_ptr += fast_scroll_speed;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_SCROLL_UP:
         rgui_descend_alphabet(rgui, &rgui->selection_ptr);
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         rgui_ascend_alphabet(rgui, &rgui->selection_ptr);
         break;
      
      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            rgui->need_refresh = true;
            rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_OK:
      {
         if (rgui->selection_buf->size == 0)
            return 0;

         const char *path = 0;
         unsigned type = 0;
         rgui_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &path, &type);

         if (
               menu_type_is(type) == RGUI_SETTINGS_SHADER_OPTIONS ||
               menu_type_is(type) == RGUI_FILE_DIRECTORY ||
               type == RGUI_SETTINGS_OVERLAY_PRESET ||
               type == RGUI_SETTINGS_CORE ||
               type == RGUI_SETTINGS_CONFIG ||
               type == RGUI_SETTINGS_DISK_APPEND ||
               type == RGUI_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            fill_pathname_join(cat_path, dir, path, sizeof(cat_path));

            rgui_list_push(rgui->menu_stack, cat_path, type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else
         {
#ifdef HAVE_SHADER_MANAGER
            if (menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
            {
               if (menu_type == RGUI_SETTINGS_SHADER_PRESET)
               {
                  char shader_path[PATH_MAX];
                  fill_pathname_join(shader_path, dir, path, sizeof(shader_path));
                  shader_manager_set_preset(&rgui->shader, gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
                        shader_path);
               }
               else
               {
                  unsigned pass = (menu_type - RGUI_SETTINGS_SHADER_0) / 3;
                  fill_pathname_join(rgui->shader.pass[pass].source.cg,
                        dir, path, sizeof(rgui->shader.pass[pass].source.cg));
               }

               // Pop stack until we hit shader manager again.
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_SHADER_OPTIONS);
            }
            else
#endif
            if (menu_type == RGUI_SETTINGS_DEFERRED_CORE)
            {
               // FIXME: Add for consoles.
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               strlcpy(g_extern.fullpath, rgui->deferred_path, sizeof(g_extern.fullpath));
#ifdef HAVE_DYNAMIC
               libretro_free_system_info(&rgui->info);
               libretro_get_system_info(g_settings.libretro, &rgui->info,
                     &rgui->load_no_rom);

               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
#else
               rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
               rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
               rgui->msg_force = true;
               ret = -1;
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_CORE)
            {
#if defined(HAVE_DYNAMIC)
               fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
               libretro_free_system_info(&rgui->info);
               libretro_get_system_info(g_settings.libretro, &rgui->info,
                     &rgui->load_no_rom);

               // No ROM needed for this core, load game immediately.
               if (rgui->load_no_rom)
               {
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
                  *g_extern.fullpath = '\0';
                  rgui->msg_force = true;
                  ret = -1;
               }

               // Core selection on non-console just updates directory listing.
               // Will take affect on new ROM load.
#elif defined(GEKKO) && defined(HW_RVL)
               rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)path);

               fill_pathname_join(g_extern.fullpath, default_paths.core_dir,
                     SALAMANDER_FILE, sizeof(g_extern.fullpath));
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
               g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
               ret = -1;
#endif

               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_CONFIG)
            {
               char config[PATH_MAX];
               fill_pathname_join(config, dir, path, sizeof(config));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
               rgui->msg_force = true;
               if (menu_replace_config(config))
               {
                  rgui->selection_ptr = 0; // Menu can shrink.
                  ret = -1;
               }
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
            {
               fill_pathname_join(g_settings.input.overlay, dir, path, sizeof(g_settings.input.overlay));

               if (driver.overlay)
                  input_overlay_free(driver.overlay);
               driver.overlay = input_overlay_new(g_settings.input.overlay);
               if (!driver.overlay)
                  RARCH_ERR("Failed to load overlay.\n");

               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_INPUT_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
            {
               char image[PATH_MAX];
               fill_pathname_join(image, dir, path, sizeof(image));
               rarch_disk_control_append_image(image);

               g_extern.lifecycle_mode_state |= 1ULL << MODE_GAME;

               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
               ret = -1;
            }
            else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
            {
               load_menu_game_history(rgui->selection_ptr);
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
               ret = -1;
            }
            else if (menu_type == RGUI_BROWSER_DIR_PATH)
            {
               strlcpy(g_settings.rgui_browser_directory, dir, sizeof(g_settings.rgui_browser_directory));
               strlcpy(rgui->base_path, dir, sizeof(rgui->base_path));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_SCREENSHOTS
            else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
            {
               strlcpy(g_settings.screenshot_directory, dir, sizeof(g_settings.screenshot_directory));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
            {
               strlcpy(g_extern.savefile_dir, dir, sizeof(g_extern.savefile_dir));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_OVERLAY
            else if (menu_type == RGUI_OVERLAY_DIR_PATH)
            {
               strlcpy(g_extern.overlay_dir, dir, sizeof(g_extern.overlay_dir));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
            {
               strlcpy(g_extern.savestate_dir, dir, sizeof(g_extern.savestate_dir));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_DYNAMIC
            else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
            {
               strlcpy(rgui->libretro_dir, dir, sizeof(rgui->libretro_dir));
               menu_init_core_info(rgui);
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_CONFIG_DIR_PATH)
            {
               strlcpy(g_settings.rgui_config_directory, dir, sizeof(g_settings.rgui_config_directory));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_LIBRETRO_INFO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_info_path, dir, sizeof(g_settings.libretro_info_path));
               menu_init_core_info(rgui);
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SHADER_DIR_PATH)
            {
               strlcpy(g_settings.video.shader_dir, dir, sizeof(g_settings.video.shader_dir));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SYSTEM_DIR_PATH)
            {
               strlcpy(g_settings.system_directory, dir, sizeof(g_settings.system_directory));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else
            {
               if (rgui->defer_core)
               {
                  fill_pathname_join(rgui->deferred_path, dir, path, sizeof(rgui->deferred_path));

                  const core_info_t *info = NULL;
                  size_t supported = 0;
                  if (rgui->core_info)
                     core_info_list_get_supported_cores(rgui->core_info, rgui->deferred_path, &info, &supported);

                  if (supported == 1) // Can make a decision right now.
                  {
                     strlcpy(g_extern.fullpath, rgui->deferred_path, sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, info->path, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
                     libretro_free_system_info(&rgui->info);
                     libretro_get_system_info(g_settings.libretro, &rgui->info,
                           &rgui->load_no_rom);
#else
                     rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
                     rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif

                     g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
                     rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
                     rgui->msg_force = true;
                     ret = -1;
                  }
                  else // Present a selection.
                  {
                     rgui_list_push(rgui->menu_stack, rgui->libretro_dir, RGUI_SETTINGS_DEFERRED_CORE, rgui->selection_ptr);
                     rgui->selection_ptr = 0;
                     rgui->need_refresh = true;
                  }
               }
               else
               {
                  fill_pathname_join(g_extern.fullpath, dir, path, sizeof(g_extern.fullpath));
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);

                  rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
                  rgui->msg_force = true;
                  ret = -1;
               }
            }
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->selection_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   // refresh values in case the stack changed
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh && (menu_type == RGUI_FILE_DIRECTORY ||
            menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS ||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY || 
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
            menu_type == RGUI_SETTINGS_DEFERRED_CORE ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY ||
            menu_type == RGUI_SETTINGS_DISK_APPEND))
   {
      rgui->need_refresh = false;
      menu_parse_and_resolve(rgui, menu_type);
   }

   if (menu_ctx && menu_ctx->render)
      menu_ctx->render(rgui);

   return ret;
}

bool menu_iterate(void)
{
   rarch_time_t time, delta, target_msec, sleep_msec;
   unsigned action;
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
         rgui->scroll_accel = min(rgui->scroll_accel + 1, 64);
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
      rgui->scroll_accel = 0;
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

   if (menu_ctx)
      input_entry_ret = menu_iterate_func(rgui, action);

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
      unsigned i;
      // In case of collision, find an alternative name.
      for (i = 0; i < 16; i++)
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

void menu_poll_bind_state(struct rgui_bind_state *state)
{
   unsigned i, b, a, h;
   memset(state->state, 0, sizeof(state->state));
   state->skip = input_input_state_func(NULL, 0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   input_joypad_poll(joypad);
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (b = 0; b < RGUI_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);
      for (h = 0; h < RGUI_MAX_HATS; h++)
      {
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h) ? HAT_UP_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h) ? HAT_DOWN_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h) ? HAT_LEFT_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h) ? HAT_RIGHT_MASK : 0;
      }
   }
}

void menu_poll_bind_get_rested_axes(struct rgui_bind_state *state)
{
   unsigned i, a;
   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] = input_joypad_axis_raw(joypad, i, a);
}

static bool menu_poll_find_trigger_pad(struct rgui_bind_state *state, struct rgui_bind_state *new_state, unsigned p)
{
   unsigned a, b, h;
   const struct rgui_bind_state_port *n = &new_state->state[p];
   const struct rgui_bind_state_port *o = &state->state[p];

   for (b = 0; b < RGUI_MAX_BUTTONS; b++)
   {
      if (n->buttons[b] && !o->buttons[b])
      {
         state->target->joykey = b;
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   // Axes are a bit tricky ...
   for (a = 0; a < RGUI_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] - new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] - new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000) // Take care of case where axis rests on +/- 0x7fff (e.g. 360 controller on Linux)
      {
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         // Lock the current axis.
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0 ? 0x7fff : -0x7fff; 
         return true;
      }

      if (locked_distance >= 20000) // Unlock the axis.
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < RGUI_MAX_HATS; h++)
   {
      uint16_t trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;
      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         state->target->joykey = HAT_MAP(h, sane_trigger);
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

bool menu_poll_find_trigger(struct rgui_bind_state *state, struct rgui_bind_state *new_state)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (menu_poll_find_trigger_pad(state, new_state, i))
      {
         g_settings.input.joypad_map[state->player] = i; // Update the joypad mapping automatically. More friendly that way.
         return true;
      }
   }
   return false;
}

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
   // TODO: Do something with this. Stub for now.
   (void)down;
   (void)keycode;
   (void)character;
   (void)key_modifiers;
}

static inline int rgui_list_get_first_char(rgui_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   rgui_list_get_alt_at_offset(buf, offset, &path);
   int ret = tolower(*path);
  
   // "Normalize" non-alphabetical entries so they are lumped together for purposes of jumping.
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static inline bool rgui_list_elem_is_dir(rgui_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   unsigned type = 0;
   rgui_list_get_at_offset(buf, offset, &path, &type);
   return type != RGUI_FILE_PLAIN;
}

static void rgui_build_scroll_indices(rgui_handle_t *rgui, rgui_list_t *buf)
{
   size_t i;
   int current;
   bool current_is_dir;

   rgui->scroll_indices_size = 0;
   if (!buf->size)
      return;

   rgui->scroll_indices[rgui->scroll_indices_size++] = 0;

   current = rgui_list_get_first_char(buf, 0);
   current_is_dir = rgui_list_elem_is_dir(buf, 0);

   for (i = 1; i < buf->size; i++)
   {
      int first;
      bool is_dir;

      first = rgui_list_get_first_char(buf, i);
      is_dir = rgui_list_elem_is_dir(buf, i);

      if ((current_is_dir && !is_dir) || (first > current))
         rgui->scroll_indices[rgui->scroll_indices_size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   rgui->scroll_indices[rgui->scroll_indices_size++] = buf->size - 1;
}

void menu_populate_entries(void *data, unsigned menu_type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   unsigned i, last;

   switch (menu_type)
   {
#ifdef HAVE_SHADER_MANAGER
      case RGUI_SETTINGS_SHADER_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Apply Shader Changes",
               RGUI_SETTINGS_SHADER_APPLY, 0);
         rgui_list_push(rgui->selection_buf, "Default Filter", RGUI_SETTINGS_SHADER_FILTER, 0);
         rgui_list_push(rgui->selection_buf, "Load Shader Preset",
               RGUI_SETTINGS_SHADER_PRESET, 0);
         rgui_list_push(rgui->selection_buf, "Shader Passes",
               RGUI_SETTINGS_SHADER_PASSES, 0);

         for (i = 0; i < rgui->shader.passes; i++)
         {
            char buf[64];

            snprintf(buf, sizeof(buf), "Shader #%u", i);
            rgui_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0 + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Filter", i);
            rgui_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0_FILTER + 3 * i, 0);

            snprintf(buf, sizeof(buf), "Shader #%u Scale", i);
            rgui_list_push(rgui->selection_buf, buf,
                  RGUI_SETTINGS_SHADER_0_SCALE + 3 * i, 0);
         }
         break;
#endif
      case RGUI_SETTINGS_VIDEO_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
#ifdef HAVE_SHADER_MANAGER
         rgui_list_push(rgui->selection_buf, "Shader Options", RGUI_SETTINGS_SHADER_OPTIONS, 0);
#endif
#ifdef GEKKO
         rgui_list_push(rgui->selection_buf, "Screen Resolution", RGUI_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
#ifndef HAVE_SHADER_MANAGER
         rgui_list_push(rgui->selection_buf, "Default Filter", RGUI_SETTINGS_VIDEO_FILTER, 0);
#endif
#ifdef HW_RVL
         rgui_list_push(rgui->selection_buf, "VI Trap filtering", RGUI_SETTINGS_VIDEO_SOFT_FILTER, 0);
         rgui_list_push(rgui->selection_buf, "Gamma", RGUI_SETTINGS_VIDEO_GAMMA, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Integer Scale", RGUI_SETTINGS_VIDEO_INTEGER_SCALE, 0);
         rgui_list_push(rgui->selection_buf, "Aspect Ratio", RGUI_SETTINGS_VIDEO_ASPECT_RATIO, 0);
         rgui_list_push(rgui->selection_buf, "Custom Ratio", RGUI_SETTINGS_CUSTOM_VIEWPORT, 0);
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         rgui_list_push(rgui->selection_buf, "Toggle Fullscreen", RGUI_SETTINGS_TOGGLE_FULLSCREEN, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Rotation", RGUI_SETTINGS_VIDEO_ROTATION, 0);
         rgui_list_push(rgui->selection_buf, "VSync", RGUI_SETTINGS_VIDEO_VSYNC, 0);
         rgui_list_push(rgui->selection_buf, "Hard GPU Sync", RGUI_SETTINGS_VIDEO_HARD_SYNC, 0);
         rgui_list_push(rgui->selection_buf, "Hard GPU Sync Frames", RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES, 0);
         rgui_list_push(rgui->selection_buf, "Black Frame Insertion", RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION, 0);
         rgui_list_push(rgui->selection_buf, "VSync Swap Interval", RGUI_SETTINGS_VIDEO_SWAP_INTERVAL, 0);
#ifdef HAVE_THREADS
         rgui_list_push(rgui->selection_buf, "Threaded Driver", RGUI_SETTINGS_VIDEO_THREADED, 0);
#endif
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         rgui_list_push(rgui->selection_buf, "Windowed Scale (X)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X, 0);
         rgui_list_push(rgui->selection_buf, "Windowed Scale (Y)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Crop Overscan (reload)", RGUI_SETTINGS_VIDEO_CROP_OVERSCAN, 0);
         rgui_list_push(rgui->selection_buf, "Estimated Monitor FPS", RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO, 0);
         break;
      case RGUI_SETTINGS_CORE_OPTIONS:
         rgui_list_clear(rgui->selection_buf);

         if (g_extern.system.core_options)
         {
            size_t i, opts;

            opts = core_option_size(g_extern.system.core_options);
            for (i = 0; i < opts; i++)
               rgui_list_push(rgui->selection_buf,
                     core_option_get_desc(g_extern.system.core_options, i), RGUI_SETTINGS_CORE_OPTION_START + i, 0);
         }
         else
            rgui_list_push(rgui->selection_buf, "No options available.", RGUI_SETTINGS_CORE_OPTION_NONE, 0);
         break;
      case RGUI_SETTINGS_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Rewind", RGUI_SETTINGS_REWIND_ENABLE, 0);
         rgui_list_push(rgui->selection_buf, "Rewind Granularity", RGUI_SETTINGS_REWIND_GRANULARITY, 0);
#ifdef HAVE_SCREENSHOTS
         rgui_list_push(rgui->selection_buf, "GPU Screenshots", RGUI_SETTINGS_GPU_SCREENSHOT, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Config Save On Exit", RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT, 0);
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         rgui_list_push(rgui->selection_buf, "SRAM Autosave", RGUI_SETTINGS_SRAM_AUTOSAVE, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Show Framerate", RGUI_SETTINGS_DEBUG_TEXT, 0);
         break;
      case RGUI_SETTINGS_DISK_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Disk Index", RGUI_SETTINGS_DISK_INDEX, 0);
         rgui_list_push(rgui->selection_buf, "Disk Image Append", RGUI_SETTINGS_DISK_APPEND, 0);
         break;
      case RGUI_SETTINGS_PATH_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Browser Directory", RGUI_BROWSER_DIR_PATH, 0);
         rgui_list_push(rgui->selection_buf, "Config Directory", RGUI_CONFIG_DIR_PATH, 0);
#ifdef HAVE_DYNAMIC
         rgui_list_push(rgui->selection_buf, "Core Directory", RGUI_LIBRETRO_DIR_PATH, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Core Info Directory", RGUI_LIBRETRO_INFO_DIR_PATH, 0);
#ifdef HAVE_SHADER_MANAGER
         rgui_list_push(rgui->selection_buf, "Shader Directory", RGUI_SHADER_DIR_PATH, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Savestate Directory", RGUI_SAVESTATE_DIR_PATH, 0);
         rgui_list_push(rgui->selection_buf, "Savefile Directory", RGUI_SAVEFILE_DIR_PATH, 0);
#ifdef HAVE_OVERLAY
         rgui_list_push(rgui->selection_buf, "Overlay Directory", RGUI_OVERLAY_DIR_PATH, 0);
#endif
         rgui_list_push(rgui->selection_buf, "System Directory", RGUI_SYSTEM_DIR_PATH, 0);
#ifdef HAVE_SCREENSHOTS
         rgui_list_push(rgui->selection_buf, "Screenshot Directory", RGUI_SCREENSHOT_DIR_PATH, 0);
#endif
         break;
      case RGUI_SETTINGS_INPUT_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
#ifdef HAVE_OVERLAY
         rgui_list_push(rgui->selection_buf, "Overlay Preset", RGUI_SETTINGS_OVERLAY_PRESET, 0);
         rgui_list_push(rgui->selection_buf, "Overlay Opacity", RGUI_SETTINGS_OVERLAY_OPACITY, 0);
         rgui_list_push(rgui->selection_buf, "Overlay Scale", RGUI_SETTINGS_OVERLAY_SCALE, 0);
#endif
         rgui_list_push(rgui->selection_buf, "Player", RGUI_SETTINGS_BIND_PLAYER, 0);
         rgui_list_push(rgui->selection_buf, "Device", RGUI_SETTINGS_BIND_DEVICE, 0);
         rgui_list_push(rgui->selection_buf, "Device Type", RGUI_SETTINGS_BIND_DEVICE_TYPE, 0);

         rgui_list_push(rgui->selection_buf, "Configure All (RetroPad)", RGUI_SETTINGS_CUSTOM_BIND_ALL, 0);
         rgui_list_push(rgui->selection_buf, "Default All (RetroPad)", RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL, 0);

         if (rgui->current_pad == 0)
            rgui_list_push(rgui->selection_buf, "RGUI Menu Toggle", RGUI_SETTINGS_BIND_MENU_TOGGLE, 0);

         last = (driver.input && driver.input->set_keybinds) ? RGUI_SETTINGS_BIND_R3 : RGUI_SETTINGS_BIND_LAST;
         for (i = RGUI_SETTINGS_BIND_BEGIN; i <= last; i++)
            rgui_list_push(rgui->selection_buf, input_config_bind_map[i - RGUI_SETTINGS_BIND_BEGIN].desc, i, 0);
         break;
      case RGUI_SETTINGS_AUDIO_OPTIONS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Mute Audio", RGUI_SETTINGS_AUDIO_MUTE, 0);
         rgui_list_push(rgui->selection_buf, "Rate Control Delta", RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA, 0);
         break;
      case RGUI_SETTINGS_DRIVERS:
         rgui_list_clear(rgui->selection_buf);
         rgui_list_push(rgui->selection_buf, "Video driver", RGUI_SETTINGS_DRIVER_VIDEO, 0);
         rgui_list_push(rgui->selection_buf, "Audio driver", RGUI_SETTINGS_DRIVER_AUDIO, 0);
         rgui_list_push(rgui->selection_buf, "Input driver", RGUI_SETTINGS_DRIVER_INPUT, 0);
         break;
      case RGUI_SETTINGS:
         rgui_list_clear(rgui->selection_buf);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
         rgui_list_push(rgui->selection_buf, "Core", RGUI_SETTINGS_CORE, 0);
#endif
         if (rgui->history)
            rgui_list_push(rgui->selection_buf, "Load Game (History)", RGUI_SETTINGS_OPEN_HISTORY, 0);

         if (rgui->core_info && core_info_list_num_info_files(rgui->core_info))
            rgui_list_push(rgui->selection_buf, "Load Game (Detect Core)", RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, 0);

         if (rgui->info.library_name || g_extern.system.info.library_name)
         {
            char load_game_core_msg[64];
            snprintf(load_game_core_msg, sizeof(load_game_core_msg), "Load Game (%s)",
                  rgui->info.library_name ? rgui->info.library_name : g_extern.system.info.library_name);
            rgui_list_push(rgui->selection_buf, load_game_core_msg, RGUI_SETTINGS_OPEN_FILEBROWSER, 0);
         }

         rgui_list_push(rgui->selection_buf, "Core Options", RGUI_SETTINGS_CORE_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Video Options", RGUI_SETTINGS_VIDEO_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Audio Options", RGUI_SETTINGS_AUDIO_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Input Options", RGUI_SETTINGS_INPUT_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Path Options", RGUI_SETTINGS_PATH_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Settings", RGUI_SETTINGS_OPTIONS, 0);
         rgui_list_push(rgui->selection_buf, "Drivers", RGUI_SETTINGS_DRIVERS, 0);

         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            if (g_extern.system.disk_control.get_num_images)
               rgui_list_push(rgui->selection_buf, "Disk Options", RGUI_SETTINGS_DISK_OPTIONS, 0);
            rgui_list_push(rgui->selection_buf, "Save State", RGUI_SETTINGS_SAVESTATE_SAVE, 0);
            rgui_list_push(rgui->selection_buf, "Load State", RGUI_SETTINGS_SAVESTATE_LOAD, 0);
#ifdef HAVE_SCREENSHOTS
            rgui_list_push(rgui->selection_buf, "Take Screenshot", RGUI_SETTINGS_SCREENSHOT, 0);
#endif
            rgui_list_push(rgui->selection_buf, "Resume Game", RGUI_SETTINGS_RESUME_GAME, 0);
            rgui_list_push(rgui->selection_buf, "Restart Game", RGUI_SETTINGS_RESTART_GAME, 0);

         }
#ifndef HAVE_DYNAMIC
         rgui_list_push(rgui->selection_buf, "Restart RetroArch", RGUI_SETTINGS_RESTART_EMULATOR, 0);
#endif
         rgui_list_push(rgui->selection_buf, "RetroArch Config", RGUI_SETTINGS_CONFIG, 0);
         rgui_list_push(rgui->selection_buf, "Save New Config", RGUI_SETTINGS_SAVE_CONFIG, 0);
         rgui_list_push(rgui->selection_buf, "Help", RGUI_START_SCREEN, 0);
         rgui_list_push(rgui->selection_buf, "Quit RetroArch", RGUI_SETTINGS_QUIT_RARCH, 0);
         break;
   }
}

void menu_parse_and_resolve(void *data, unsigned menu_type)
{
   const core_info_t *info = NULL;
   const char *dir;
   size_t i, list_size;
   rgui_list_t *list;
   rgui_handle_t *rgui;

   rgui = (rgui_handle_t*)data;
   dir = NULL;

   rgui_list_clear(rgui->selection_buf);

   // parsing switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_OPEN_HISTORY:
         /* History parse */
         list_size = rom_history_size(rgui->history);

         for (i = 0; i < list_size; i++)
         {
            const char *path, *core_path, *core_name;
            char fill_buf[PATH_MAX];

            path = NULL;
            core_path = NULL;
            core_name = NULL;

            rom_history_get_index(rgui->history, i,
                  &path, &core_path, &core_name);

            if (path)
            {
               char path_short[PATH_MAX];
               fill_pathname(path_short, path_basename(path), "", sizeof(path_short));

               snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
                     path_short, core_name);
            }
            else
               strlcpy(fill_buf, core_name, sizeof(fill_buf));

            rgui_list_push(rgui->selection_buf, fill_buf, RGUI_FILE_PLAIN, 0);
         }
         break;
      case RGUI_SETTINGS_DEFERRED_CORE:
         break;
      default:
         {
            /* Directory parse */
            rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

            if (!*dir)
            {
#if defined(GEKKO)
#ifdef HW_RVL
               rgui_list_push(rgui->selection_buf, "sd:/", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "usb:/", menu_type, 0);
#endif
               rgui_list_push(rgui->selection_buf, "carda:/", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "cardb:/", menu_type, 0);
#elif defined(_XBOX1)
               rgui_list_push(rgui->selection_buf, "C:\\", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "D:\\", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "E:\\", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "F:\\", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "G:\\", menu_type, 0);
#elif defined(_WIN32)
               unsigned drives = GetLogicalDrives();
               char drive[] = " :\\";
               for (i = 0; i < 32; i++)
               {
                  drive[0] = 'A' + i;
                  if (drives & (1 << i))
                     rgui_list_push(rgui->selection_buf, drive, menu_type, 0);
               }
#elif defined(__CELLOS_LV2__)
               rgui_list_push(rgui->selection_buf, "app_home:/", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "dev_hdd0:/", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "dev_hdd1:/", menu_type, 0);
               rgui_list_push(rgui->selection_buf, "host_root:/", menu_type, 0);
#else
               rgui_list_push(rgui->selection_buf, "/", menu_type, 0);
#endif
               return;
            }
#if defined(GEKKO) && defined(HW_RVL)
            LWP_MutexLock(gx_device_mutex);
            int dev = gx_get_device_from_path(dir);

            if (dev != -1 && !gx_devices[dev].mounted && gx_devices[dev].interface->isInserted())
               fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

            LWP_MutexUnlock(gx_device_mutex);
#endif

            const char *exts;
            char ext_buf[1024];
            if (menu_type == RGUI_SETTINGS_CORE)
               exts = EXT_EXECUTABLES;
            else if (menu_type == RGUI_SETTINGS_CONFIG)
               exts = "cfg";
            else if (menu_type == RGUI_SETTINGS_SHADER_PRESET)
               exts = "cgp|glslp";
            else if (menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
               exts = "cg|glsl";
            else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
               exts = "cfg";
            else if (menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               exts = ""; // we ignore files anyway
            else if (rgui->defer_core)
               exts = rgui->core_info ? core_info_list_get_all_extensions(rgui->core_info) : "";
            else if (rgui->info.valid_extensions)
            {
               exts = ext_buf;
               if (*rgui->info.valid_extensions)
                  snprintf(ext_buf, sizeof(ext_buf), "%s|zip", rgui->info.valid_extensions);
               else
                  *ext_buf = '\0';
            }
            else
               exts = g_extern.system.valid_extensions;

            struct string_list *list = dir_list_new(dir, exts, true);
            if (!list)
               return;

            dir_list_sort(list, true);

            if (menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               rgui_list_push(rgui->selection_buf, "<Use this directory>", RGUI_FILE_USE_DIRECTORY, 0);

            for (i = 0; i < list->size; i++)
            {
               bool is_dir = list->elems[i].attr.b;

               if ((menu_type_is(menu_type) == RGUI_FILE_DIRECTORY) && !is_dir)
                  continue;

#ifdef HAVE_LIBRETRO_MANAGEMENT
               if (menu_type == RGUI_SETTINGS_CORE && (is_dir || strcasecmp(list->elems[i].data, SALAMANDER_FILE) == 0))
                  continue;
#endif

               // Need to preserve slash first time.
               const char *path = list->elems[i].data;
               if (*dir)
                  path = path_basename(path);

               // Push menu_type further down in the chain.
               // Needed for shader manager currently.
               rgui_list_push(rgui->selection_buf, path,
                     is_dir ? menu_type : RGUI_FILE_PLAIN, 0);
            }

            string_list_free(list);
         }
   }

   // resolving switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_CORE:
         dir = NULL;
         list = (rgui_list_t*)rgui->selection_buf;
         rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);
         list_size = list->size;
         for (i = 0; i < list_size; i++)
         {
            const char *path;
            unsigned type = 0;
            rgui_list_get_at_offset(list, i, &path, &type);
            if (type != RGUI_FILE_PLAIN)
               continue;

            char core_path[PATH_MAX];
            fill_pathname_join(core_path, dir, path, sizeof(core_path));

            char display_name[256];
            if (rgui->core_info &&
                  core_info_list_get_display_name(rgui->core_info,
                     core_path, display_name, sizeof(display_name)))
               rgui_list_set_alt_at_offset(list, i, display_name);
         }
         rgui_list_sort_on_alt(rgui->selection_buf);
         break;
      case RGUI_SETTINGS_DEFERRED_CORE:
         core_info_list_get_supported_cores(rgui->core_info, rgui->deferred_path, &info, &list_size);
         for (i = 0; i < list_size; i++)
         {
            rgui_list_push(rgui->selection_buf, info[i].path, RGUI_FILE_PLAIN, 0);
            rgui_list_set_alt_at_offset(rgui->selection_buf, i, info[i].display_name);
         }
         rgui_list_sort_on_alt(rgui->selection_buf);
         break;
      default:
         (void)0;
   }

   rgui->scroll_indices_size = 0;
   if (menu_type != RGUI_SETTINGS_OPEN_HISTORY)
      rgui_build_scroll_indices(rgui, rgui->selection_buf);

   // Before a refresh, we could have deleted a file on disk, causing
   // selection_ptr to suddendly be out of range. Ensure it doesn't overflow.
   if (rgui->selection_ptr >= rgui->selection_buf->size && rgui->selection_buf->size)
      rgui->selection_ptr = rgui->selection_buf->size - 1;
   else if (!rgui->selection_buf->size)
      rgui->selection_ptr = 0;
}

void menu_init_core_info(rgui_handle_t *rgui)
{
   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
}
