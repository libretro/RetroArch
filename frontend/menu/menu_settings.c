/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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
#include <string.h>
#include "../../file.h"
#include "menu_common.h"
#include "../../gfx/gfx_common.h"
#include "../../input/input_common.h"
#include "../../config.def.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2

static unsigned rgui_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 384, 240 },
   { 512, 240 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 530, 240 },
   { 640, 240 },
   { 512, 448 },
   { 640, 448 },
   { 640, 480 },
};

static unsigned rgui_current_gx_resolution = GX_RESOLUTIONS_640_480;
#else
#define MAX_GAMMA_SETTING 1
#endif

unsigned menu_type_is(unsigned type)
{
   unsigned ret = 0;
   bool type_found;

   type_found = 
      type == RGUI_SETTINGS ||
      type == RGUI_SETTINGS_CORE_OPTIONS ||
      type == RGUI_SETTINGS_VIDEO_OPTIONS ||
      type == RGUI_SETTINGS_SHADER_OPTIONS ||
      type == RGUI_SETTINGS_AUDIO_OPTIONS ||
      type == RGUI_SETTINGS_DISK_OPTIONS ||
      type == RGUI_SETTINGS_PATH_OPTIONS ||
      type == RGUI_SETTINGS_OPTIONS ||
      type == RGUI_SETTINGS_DRIVERS ||
      (type == RGUI_SETTINGS_INPUT_OPTIONS);

   if (type_found)
   {
      ret = RGUI_SETTINGS;
      return ret;
   }

   type_found = (type >= RGUI_SETTINGS_SHADER_0 &&
         type <= RGUI_SETTINGS_SHADER_LAST &&
         ((type - RGUI_SETTINGS_SHADER_0) % 3) == 0) ||
      type == RGUI_SETTINGS_SHADER_PRESET;

   if (type_found)
   {
      ret = RGUI_SETTINGS_SHADER_OPTIONS;
      return ret;
   }

   type_found = type == RGUI_BROWSER_DIR_PATH ||
      type == RGUI_SHADER_DIR_PATH ||
      type == RGUI_SAVESTATE_DIR_PATH ||
      type == RGUI_LIBRETRO_DIR_PATH ||
      type == RGUI_LIBRETRO_INFO_DIR_PATH ||
      type == RGUI_CONFIG_DIR_PATH ||
      type == RGUI_SAVEFILE_DIR_PATH ||
      type == RGUI_OVERLAY_DIR_PATH ||
      type == RGUI_SCREENSHOT_DIR_PATH ||
      type == RGUI_SYSTEM_DIR_PATH;

   if (type_found)
   {
      ret = RGUI_FILE_DIRECTORY;
      return ret;
   }

   return ret;
}

#ifdef HAVE_SHADER_MANAGER
static enum rarch_shader_type shader_manager_get_type(const struct gfx_shader *shader)
{
   unsigned i;
   // All shader types must be the same, or we cannot use it.
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type = gfx_shader_parse_type(shader->pass[i].source.cg,
            RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;

         default:
            return RARCH_SHADER_NONE;
      }
   }

   return type;
}

static int shader_manager_toggle_setting(void *data, unsigned setting, unsigned action)
{
   unsigned dist_shader, dist_filter, dist_scale;
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   dist_shader = setting - RGUI_SETTINGS_SHADER_0;
   dist_filter = setting - RGUI_SETTINGS_SHADER_0_FILTER;
   dist_scale  = setting - RGUI_SETTINGS_SHADER_0_SCALE;

   if (setting == RGUI_SETTINGS_SHADER_FILTER)
   {
      switch (action)
      {
         case RGUI_ACTION_START:
            g_settings.video.smooth = true;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
            g_settings.video.smooth = !g_settings.video.smooth;
            break;

         default:
            break;
      }
   }
   else if (setting == RGUI_SETTINGS_SHADER_APPLY || setting == RGUI_SETTINGS_SHADER_PASSES)
      return menu_set_settings(rgui, setting, action);
   else if ((dist_shader % 3) == 0 || setting == RGUI_SETTINGS_SHADER_PRESET)
   {
      dist_shader /= 3;
      struct gfx_shader_pass *pass = setting == RGUI_SETTINGS_SHADER_PRESET ? 
         &rgui->shader.pass[dist_shader] : NULL;
      switch (action)
      {
         case RGUI_ACTION_OK:
            file_list_push(rgui->menu_stack, g_settings.video.shader_dir, setting, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
            break;

         case RGUI_ACTION_START:
            if (pass)
               *pass->source.cg = '\0';
            break;

         default:
            break;
      }
   }
   else if ((dist_filter % 3) == 0)
   {
      dist_filter /= 3;
      struct gfx_shader_pass *pass = &rgui->shader.pass[dist_filter];
      switch (action)
      {
         case RGUI_ACTION_START:
            rgui->shader.pass[dist_filter].filter = RARCH_FILTER_UNSPEC;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
         {
            unsigned delta = action == RGUI_ACTION_LEFT ? 2 : 1;
            pass->filter = (enum gfx_filter_type)((pass->filter + delta) % 3);
            break;
         }

         default:
         break;
      }
   }
   else if ((dist_scale % 3) == 0)
   {
      dist_scale /= 3;
      struct gfx_shader_pass *pass = &rgui->shader.pass[dist_scale];
      switch (action)
      {
         case RGUI_ACTION_START:
            pass->fbo.scale_x = pass->fbo.scale_y = 0;
            pass->fbo.valid = false;
            break;

         case RGUI_ACTION_LEFT:
         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
         {
            unsigned current_scale = pass->fbo.scale_x;
            unsigned delta = action == RGUI_ACTION_LEFT ? 5 : 1;
            current_scale = (current_scale + delta) % 6;
            pass->fbo.valid = current_scale;
            pass->fbo.scale_x = pass->fbo.scale_y = current_scale;
            break;
         }

         default:
         break;
      }
   }

   return 0;
}
#endif

static int menu_core_setting_toggle(unsigned setting, unsigned action)
{
   unsigned index = setting - RGUI_SETTINGS_CORE_OPTION_START;
   switch (action)
   {
      case RGUI_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, index);
         break;

      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
         core_option_next(g_extern.system.core_options, index);
         break;

      case RGUI_ACTION_START:
         core_option_set_default(g_extern.system.core_options, index);
         break;

      default:
         break;
   }

   return 0;
}

int menu_settings_toggle_setting(void *data, unsigned setting, unsigned action, unsigned menu_type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
#ifdef HAVE_SHADER_MANAGER
   if (setting >= RGUI_SETTINGS_SHADER_FILTER && setting <= RGUI_SETTINGS_SHADER_LAST)
      return shader_manager_toggle_setting(rgui, setting, action);
#endif
   if (setting >= RGUI_SETTINGS_CORE_OPTION_START)
      return menu_core_setting_toggle(setting, action);

   return menu_set_settings(rgui, setting, action);
}

#ifdef HAVE_OSK
static bool osk_callback_enter_rsound(void *data)
{
   if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_SUCCESS)
         && driver.osk && driver.osk->get_text_buf)
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      wchar_t *text_buf = (wchar_t*)driver.osk->get_text_buf(driver.osk_data);
      int num = wcstombs(tmp_str, text_buf, sizeof(tmp_str));
      tmp_str[num] = 0;
      strlcpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device));
      goto do_exit;
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;

do_exit:
   g_extern.lifecycle_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_rsound_init(void *data)
{
   if (!driver.osk)
      return false;

   if (driver.osk->write_initial_msg)
      driver.osk->write_initial_msg(driver.osk_data, L"192.168.1.1");
   if (driver.osk->write_msg)
      driver.osk->write_msg(driver.osk_data, L"Enter IP address for the RSound Server.");
   if (driver.osk->start)
      driver.osk->start(driver.osk_data);

   return true;
}

static bool osk_callback_enter_filename(void *data)
{
   if (!driver.osk)
      return false;

   if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_SUCCESS))
   {
      RARCH_LOG("OSK - Applying input data.\n");
      char tmp_str[256];
      char filepath[PATH_MAX];
      int num = wcstombs(tmp_str, driver.osk->get_text_buf(driver.osk_data), sizeof(tmp_str));
      tmp_str[num] = 0;

      fill_pathname_join(filepath, g_settings.video.shader_dir, tmp_str, sizeof(filepath));
      strlcat(filepath, ".cgp", sizeof(filepath));
      RARCH_LOG("[osk_callback_enter_filename]: filepath is: %s.\n", filepath);
      config_file_t *conf = config_file_new(NULL);
      if (!conf)
         return false;
      gfx_shader_write_conf_cgp(conf, &rgui->shader);
      config_file_write(conf, filepath);
      config_file_free(conf);
      goto do_exit;
   }
   else if (g_extern.lifecycle_state & (1ULL << MODE_OSK_ENTRY_FAIL))
      goto do_exit;

   return false;
do_exit:
   g_extern.lifecycle_state &= ~((1ULL << MODE_OSK_ENTRY_SUCCESS) |
         (1ULL << MODE_OSK_ENTRY_FAIL));
   return true;
}

static bool osk_callback_enter_filename_init(void *data)
{
   if (!driver.osk)
      return false;

   if (driver.osk->write_initial_msg)
      driver.osk->write_initial_msg(driver.osk_data, L"Save Preset");
   if (driver.osk->write_msg)
      driver.osk->write_msg(driver.osk_data, L"Enter filename for preset.");
   if (driver.osk->start)
      driver.osk->start(driver.osk_data);

   return true;
}

#endif

int menu_set_settings(void *data, unsigned setting, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   unsigned port = rgui->current_pad;

   switch (setting)
   {
      case RGUI_START_SCREEN:
         if (action == RGUI_ACTION_OK)
            file_list_push(rgui->menu_stack, "", RGUI_START_SCREEN, 0);
         break;
      case RGUI_SETTINGS_REWIND_ENABLE:
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
         {
            g_settings.rewind_enable = !g_settings.rewind_enable;
            if (g_settings.rewind_enable)
               rarch_init_rewind();
            else
               rarch_deinit_rewind();
         }
         else if (action == RGUI_ACTION_START)
         {
            g_settings.rewind_enable = false;
            rarch_deinit_rewind();
         }
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_GPU_SCREENSHOT:
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
            g_settings.video.gpu_screenshot = !g_settings.video.gpu_screenshot;
         else if (action == RGUI_ACTION_START)
            g_settings.video.gpu_screenshot = true;
         break;
#endif
      case RGUI_SETTINGS_REWIND_GRANULARITY:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT)
            g_settings.rewind_granularity++;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.rewind_granularity > 1)
               g_settings.rewind_granularity--;
         }
         else if (action == RGUI_ACTION_START)
            g_settings.rewind_granularity = 1;
         break;
      case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT 
               || action == RGUI_ACTION_LEFT)
         {
            g_extern.config_save_on_exit = !g_extern.config_save_on_exit;
         }
         else if (action == RGUI_ACTION_START)
            g_extern.config_save_on_exit = true;
         break;
#if defined(HAVE_THREADS)
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT)
         {
            rarch_deinit_autosave();
            g_settings.autosave_interval += 10;
            if (g_settings.autosave_interval)
               rarch_init_autosave();
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.autosave_interval)
            {
               rarch_deinit_autosave();
               g_settings.autosave_interval -= min(10, g_settings.autosave_interval);
               if (g_settings.autosave_interval)
                  rarch_init_autosave();
            }
         }
         else if (action == RGUI_ACTION_START)
         {
            rarch_deinit_autosave();
            g_settings.autosave_interval = 0;
         }
         break;
#endif
      case RGUI_SETTINGS_SAVESTATE_SAVE:
      case RGUI_SETTINGS_SAVESTATE_LOAD:
         if (action == RGUI_ACTION_OK)
         {
            if (setting == RGUI_SETTINGS_SAVESTATE_SAVE)
               rarch_save_state();
            else
               rarch_load_state();
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         else if (action == RGUI_ACTION_START)
            g_extern.state_slot = 0;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_extern.state_slot != 0)
               g_extern.state_slot--;
         }
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.state_slot++;
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_SCREENSHOT:
         if (action == RGUI_ACTION_OK)
            rarch_take_screenshot();
         break;
#endif
      case RGUI_SETTINGS_RESTART_GAME:
         if (action == RGUI_ACTION_OK)
         {
            rarch_game_reset();
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         if (action == RGUI_ACTION_START)
            g_extern.audio_data.mute = false;
         else
            g_extern.audio_data.mute = !g_extern.audio_data.mute;
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         if (action == RGUI_ACTION_START)
         {
            g_settings.audio.rate_control_delta = rate_control_delta;
            g_settings.audio.rate_control = rate_control;
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.audio.rate_control_delta > 0.0)
               g_settings.audio.rate_control_delta -= 0.001;

            if (g_settings.audio.rate_control_delta < 0.0005)
            {
               g_settings.audio.rate_control = false;
               g_settings.audio.rate_control_delta = 0.0;
            }
            else
               g_settings.audio.rate_control = true;
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.audio.rate_control_delta < 0.2)
               g_settings.audio.rate_control_delta += 0.001;
            g_settings.audio.rate_control = true;
         }
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         if (action == RGUI_ACTION_START)
            g_settings.fps_show = false;
         else if (action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            g_settings.fps_show = !g_settings.fps_show;
         break;
      case RGUI_SETTINGS_DISK_INDEX:
         {
            const struct retro_disk_control_callback *control = &g_extern.system.disk_control;

            unsigned num_disks = control->get_num_images();
            unsigned current   = control->get_image_index();

            int step = 0;
            if (action == RGUI_ACTION_RIGHT || action == RGUI_ACTION_OK)
               step = 1;
            else if (action == RGUI_ACTION_LEFT)
               step = -1;

            if (step)
            {
               unsigned next_index = (current + num_disks + 1 + step) % (num_disks + 1);
               rarch_disk_control_set_eject(true, false);
               rarch_disk_control_set_index(next_index);
               rarch_disk_control_set_eject(false, false);
            }

            break;
         }
      case RGUI_SETTINGS_RESTART_EMULATOR:
         if (action == RGUI_ACTION_OK)
         {
#if defined(GEKKO) && defined(HW_RVL)
            fill_pathname_join(g_extern.fullpath, default_paths.core_dir, SALAMANDER_FILE,
                  sizeof(g_extern.fullpath));
#endif
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }
         break;
      case RGUI_SETTINGS_RESUME_GAME:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_QUIT_RARCH:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_SAVE_CONFIG:
         if (action == RGUI_ACTION_OK)
            menu_save_new_config();
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_PRESET:
         switch (action)
         {
            case RGUI_ACTION_OK:
               file_list_push(rgui->menu_stack, g_extern.overlay_dir, setting, rgui->selection_ptr);
               rgui->selection_ptr = 0;
               rgui->need_refresh = true;
               break;

#ifndef __QNX__ // FIXME: Why ifndef QNX?
            case RGUI_ACTION_START:
               if (driver.overlay)
                  input_overlay_free(driver.overlay);
               driver.overlay = NULL;
               *g_settings.input.overlay = '\0';
               break;
#endif

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_OVERLAY_OPACITY:
         {
            bool changed = true;
            switch (action)
            {
               case RGUI_ACTION_LEFT:
                  g_settings.input.overlay_opacity -= 0.01f;

                  if (g_settings.input.overlay_opacity < 0.0f)
                     g_settings.input.overlay_opacity = 0.0f;
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  g_settings.input.overlay_opacity += 0.01f;

                  if (g_settings.input.overlay_opacity > 1.0f)
                     g_settings.input.overlay_opacity = 1.0f;
                  break;

               case RGUI_ACTION_START:
                  g_settings.input.overlay_opacity = 1.0f;
                  break;

               default:
                  changed = false;
                  break;
            }

            if (changed && driver.overlay)
               input_overlay_set_alpha_mod(driver.overlay,
                     g_settings.input.overlay_opacity);
            break;
         }

      case RGUI_SETTINGS_OVERLAY_SCALE:
         {
            bool changed = true;
            switch (action)
            {
               case RGUI_ACTION_LEFT:
                  g_settings.input.overlay_scale -= 0.01f;

                  if (g_settings.input.overlay_scale < 0.01f) // Avoid potential divide by zero.
                     g_settings.input.overlay_scale = 0.01f;
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  g_settings.input.overlay_scale += 0.01f;

                  if (g_settings.input.overlay_scale > 2.0f)
                     g_settings.input.overlay_scale = 2.0f;
                  break;

               case RGUI_ACTION_START:
                  g_settings.input.overlay_scale = 1.0f;
                  break;

               default:
                  changed = false;
                  break;
            }

            if (changed && driver.overlay)
               input_overlay_set_scale_factor(driver.overlay,
                     g_settings.input.overlay_scale);
            break;
         }
#endif
         // controllers
      case RGUI_SETTINGS_BIND_PLAYER:
         if (action == RGUI_ACTION_START)
            rgui->current_pad = 0;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (rgui->current_pad != 0)
               rgui->current_pad--;
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (rgui->current_pad < MAX_PLAYERS - 1)
               rgui->current_pad++;
         }
#ifdef HAVE_RGUI
         if (port != rgui->current_pad)
            rgui->need_refresh = true;
#endif
         port = rgui->current_pad;
         break;
      case RGUI_SETTINGS_BIND_DEVICE:
         // If set_keybinds is supported, we do it more fancy, and scroll through
         // a list of supported devices directly.
         if (driver.input->set_keybinds)
         {
            g_settings.input.device[port] += DEVICE_LAST;
            if (action == RGUI_ACTION_START)
               g_settings.input.device[port] = 0;
            else if (action == RGUI_ACTION_LEFT)
               g_settings.input.device[port]--;
            else if (action == RGUI_ACTION_RIGHT)
               g_settings.input.device[port]++;

            // DEVICE_LAST can be 0, avoid modulo.
            if (g_settings.input.device[port] >= DEVICE_LAST)
               g_settings.input.device[port] -= DEVICE_LAST;

            unsigned keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS);

            driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port, 0,
                  keybind_action);
         }
         else
         {
            // When only straight g_settings.input.joypad_map[] style
            // mapping is supported.
            int *p = &g_settings.input.joypad_map[port];
            if (action == RGUI_ACTION_START)
               *p = port;
            else if (action == RGUI_ACTION_LEFT)
               (*p)--;
            else if (action == RGUI_ACTION_RIGHT)
               (*p)++;

            if (*p < -1)
               *p = -1;
            else if (*p >= MAX_PLAYERS)
               *p = MAX_PLAYERS - 1;
         }
         break;
      case RGUI_SETTINGS_BIND_DEVICE_TYPE:
         {
            static const unsigned device_types[] = {
               RETRO_DEVICE_NONE,
               RETRO_DEVICE_JOYPAD,
               RETRO_DEVICE_ANALOG,
               RETRO_DEVICE_MOUSE,
               RETRO_DEVICE_JOYPAD_MULTITAP,
               RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE,
               RETRO_DEVICE_LIGHTGUN_JUSTIFIER,
               RETRO_DEVICE_LIGHTGUN_JUSTIFIERS,
            };

            unsigned current_device, current_index, i;
            current_device = g_settings.input.libretro_device[port];
            current_index = 0;
            for (i = 0; i < ARRAY_SIZE(device_types); i++)
            {
               if (current_device == device_types[i])
               {
                  current_index = i;
                  break;
               }
            }

            bool updated = true;
            switch (action)
            {
               case RGUI_ACTION_START:
                  current_device = RETRO_DEVICE_JOYPAD;
                  break;

               case RGUI_ACTION_LEFT:
                  current_device = device_types[(current_index + ARRAY_SIZE(device_types) - 1) % ARRAY_SIZE(device_types)];
                  break;

               case RGUI_ACTION_RIGHT:
               case RGUI_ACTION_OK:
                  current_device = device_types[(current_index + 1) % ARRAY_SIZE(device_types)];
                  break;

               default:
                  updated = false;
            }

            if (updated)
            {
               g_settings.input.libretro_device[port] = current_device;
               pretro_set_controller_port_device(port, current_device);
            }

            break;
         }
      case RGUI_SETTINGS_CUSTOM_BIND_ALL:
         if (action == RGUI_ACTION_OK)
         {
            rgui->binds.target = &g_settings.input.binds[port][0];
            rgui->binds.begin = RGUI_SETTINGS_BIND_BEGIN;
            rgui->binds.last = RGUI_SETTINGS_BIND_LAST;
            file_list_push(rgui->menu_stack, "", RGUI_SETTINGS_CUSTOM_BIND, rgui->selection_ptr);
            menu_poll_bind_get_rested_axes(&rgui->binds);
            menu_poll_bind_state(&rgui->binds);
         }
         break;
      case RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         if (action == RGUI_ACTION_OK)
         {
            unsigned i;
            struct retro_keybind *target = &g_settings.input.binds[port][0];
            rgui->binds.begin = RGUI_SETTINGS_BIND_BEGIN;
            rgui->binds.last = RGUI_SETTINGS_BIND_LAST;
            for (i = RGUI_SETTINGS_BIND_BEGIN; i <= RGUI_SETTINGS_BIND_LAST; i++, target++)
            {
               target->joykey = NO_BTN;
               target->joyaxis = AXIS_NONE;
            }
         }
         break;
      case RGUI_SETTINGS_BIND_UP:
      case RGUI_SETTINGS_BIND_DOWN:
      case RGUI_SETTINGS_BIND_LEFT:
      case RGUI_SETTINGS_BIND_RIGHT:
      case RGUI_SETTINGS_BIND_A:
      case RGUI_SETTINGS_BIND_B:
      case RGUI_SETTINGS_BIND_X:
      case RGUI_SETTINGS_BIND_Y:
      case RGUI_SETTINGS_BIND_START:
      case RGUI_SETTINGS_BIND_SELECT:
      case RGUI_SETTINGS_BIND_L:
      case RGUI_SETTINGS_BIND_R:
      case RGUI_SETTINGS_BIND_L2:
      case RGUI_SETTINGS_BIND_R2:
      case RGUI_SETTINGS_BIND_L3:
      case RGUI_SETTINGS_BIND_R3:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
      case RGUI_SETTINGS_BIND_MENU_TOGGLE:
         if (driver.input->set_keybinds)
         {
            unsigned keybind_action = KEYBINDS_ACTION_NONE;

            if (action == RGUI_ACTION_START)
               keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);

            // FIXME: The array indices here look totally wrong ... Fixed it so it looks kind of sane for now.
            if (keybind_action != KEYBINDS_ACTION_NONE)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port,
                     setting - RGUI_SETTINGS_BIND_BEGIN, keybind_action); 
         }
         else
         {
            struct retro_keybind *bind = &g_settings.input.binds[port][setting - RGUI_SETTINGS_BIND_BEGIN];
            if (action == RGUI_ACTION_OK)
            {
               rgui->binds.begin = setting;
               rgui->binds.last = setting;
               rgui->binds.target = bind;
               rgui->binds.player = port;
               file_list_push(rgui->menu_stack, "", RGUI_SETTINGS_CUSTOM_BIND, rgui->selection_ptr);
               menu_poll_bind_get_rested_axes(&rgui->binds);
               menu_poll_bind_state(&rgui->binds);
            }
            else if (action == RGUI_ACTION_START)
            {
               bind->joykey = NO_BTN;
               bind->joyaxis = AXIS_NONE;
            }
         }
         break;
      case RGUI_BROWSER_DIR_PATH:
         if (action == RGUI_ACTION_START)
         {
            *g_settings.rgui_browser_directory = '\0';
            *rgui->base_path = '\0';
         }
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SCREENSHOT_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.screenshot_directory = '\0';
         break;
#endif
      case RGUI_SAVEFILE_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.savefile_dir = '\0';
         break;
#ifdef HAVE_OVERLAY
      case RGUI_OVERLAY_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.overlay_dir = '\0';
         break;
#endif
      case RGUI_SAVESTATE_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_extern.savestate_dir = '\0';
         break;
#ifdef HAVE_DYNAMIC
      case RGUI_LIBRETRO_DIR_PATH:
         if (action == RGUI_ACTION_START)
         {
            *rgui->libretro_dir = '\0';
            menu_init_core_info(rgui);
         }
         break;
#endif
      case RGUI_LIBRETRO_INFO_DIR_PATH:
         if (action == RGUI_ACTION_START)
         {
            *g_settings.libretro_info_path = '\0';
            menu_init_core_info(rgui);
         }
         break;
      case RGUI_CONFIG_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.rgui_config_directory = '\0';
         break;
      case RGUI_SHADER_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.video.shader_dir = '\0';
         break;
      case RGUI_SYSTEM_DIR_PATH:
         if (action == RGUI_ACTION_START)
            *g_settings.system_directory = '\0';
         break;
      case RGUI_SETTINGS_VIDEO_ROTATION:
         if (action == RGUI_ACTION_START)
         {
            g_settings.video.rotation = ORIENTATION_NORMAL;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.video.rotation > 0)
               g_settings.video.rotation--;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.video.rotation < LAST_ORIENTATION)
               g_settings.video.rotation++;
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         break;

      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            g_settings.video.smooth = video_smooth;
         else
            g_settings.video.smooth = !g_settings.video.smooth;

         if (driver.video_poke->set_filtering)
            driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         break;

      case RGUI_SETTINGS_DRIVER_VIDEO:
         if (action == RGUI_ACTION_LEFT)
            find_prev_video_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_video_driver();
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO:
         if (action == RGUI_ACTION_LEFT)
            find_prev_audio_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_audio_driver();
         break;
      case RGUI_SETTINGS_DRIVER_INPUT:
         if (action == RGUI_ACTION_LEFT)
            find_prev_input_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_input_driver();
         break;
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_DRIVER_CAMERA:
         if (action == RGUI_ACTION_LEFT)
            find_prev_camera_driver();
         else if (action == RGUI_ACTION_RIGHT)
            find_next_camera_driver();
         break;
#endif
      case RGUI_SETTINGS_VIDEO_GAMMA:
         if (action == RGUI_ACTION_START)
         {
            g_extern.console.screen.gamma_correction = 0;
            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if(g_extern.console.screen.gamma_correction > 0)
            {
               g_extern.console.screen.gamma_correction--;
               if (driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if(g_extern.console.screen.gamma_correction < MAX_GAMMA_SETTING)
            {
               g_extern.console.screen.gamma_correction++;
               if (driver.video_poke->apply_state_changes)
                  driver.video_poke->apply_state_changes(driver.video_data);
            }
         }
         break;

      case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
         if (action == RGUI_ACTION_START)
            g_settings.video.scale_integer = scale_integer;
         else if (action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT ||
               action == RGUI_ACTION_OK)
            g_settings.video.scale_integer = !g_settings.video.scale_integer;

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         if (action == RGUI_ACTION_START)
            g_settings.video.aspect_ratio_idx = aspect_ratio_idx;
         else if (action == RGUI_ACTION_LEFT)
         {
            if (g_settings.video.aspect_ratio_idx > 0)
               g_settings.video.aspect_ratio_idx--;
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_settings.video.aspect_ratio_idx < LAST_ASPECT_RATIO)
               g_settings.video.aspect_ratio_idx++;
         }

         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         break;

      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
         if (action == RGUI_ACTION_OK)
            rarch_set_fullscreen(!g_settings.video.fullscreen);
         break;

#if defined(GEKKO)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         if (action == RGUI_ACTION_LEFT)
         {
            if(rgui_current_gx_resolution > 0)
            {
               rgui_current_gx_resolution--;
               gx_set_video_mode(driver.video_data, rgui_gx_resolutions[rgui_current_gx_resolution][0], rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (rgui_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
            {
#ifdef HW_RVL
               if ((rgui_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
                  if (CONF_GetVideo() != CONF_VIDEO_PAL)
                     return 0;
#endif

               rgui_current_gx_resolution++;
               gx_set_video_mode(driver.video_data, rgui_gx_resolutions[rgui_current_gx_resolution][0],
                     rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
         break;
#elif defined(__CELLOS_LV2__)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         if (action == RGUI_ACTION_LEFT)
         {
            if (g_extern.console.screen.resolutions.current.idx)
            {
               g_extern.console.screen.resolutions.current.idx--;
               g_extern.console.screen.resolutions.current.id = 
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if (g_extern.console.screen.resolutions.current.idx + 1 < 
                  g_extern.console.screen.resolutions.count)
            {
               g_extern.console.screen.resolutions.current.idx++;
               g_extern.console.screen.resolutions.current.id = 
                  g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
            }
         }
         else if (action == RGUI_ACTION_OK)
         {
            if (g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx] == CELL_VIDEO_OUT_RESOLUTION_576)
            {
               if (g_extern.console.screen.pal_enable)
                  g_extern.lifecycle_state |= (1ULL<< MODE_VIDEO_PAL_ENABLE);
            }
            else
            {
               g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_ENABLE);
               g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
            }
            driver.video->restart();

            if (menu_ctx && menu_ctx->init_assets)
               menu_ctx->init_assets(rgui);
         }
         break;
      case RGUI_SETTINGS_VIDEO_PAL60:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
                     g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);
                  else
                     g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  driver.video->restart();
                  if (menu_ctx && menu_ctx->init_assets)
                     menu_ctx->init_assets(rgui);
               }
               break;
            case RGUI_ACTION_START:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_ENABLE))
               {
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE);

                  driver.video->restart();
                  if (menu_ctx && menu_ctx->init_assets)
                     menu_ctx->init_assets(rgui);
               }
               break;
         }
         break;
#endif
#ifdef HW_RVL
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
            g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         else
            g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
#endif

      case RGUI_SETTINGS_VIDEO_VSYNC:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.vsync = true;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.vsync = !g_settings.video.vsync;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_HARD_SYNC:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.hard_sync = false;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.hard_sync = !g_settings.video.hard_sync;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.black_frame_insertion = false;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.black_frame_insertion = !g_settings.video.black_frame_insertion;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.crop_overscan = true;
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.crop_overscan = !g_settings.video.crop_overscan;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
      {
         float *scale = setting == RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X ? &g_settings.video.xscale : &g_settings.video.yscale;
         float old_scale = *scale;

         switch (action)
         {
            case RGUI_ACTION_START:
               *scale = 3.0f;
               break;

            case RGUI_ACTION_LEFT:
               *scale -= 1.0f;
               break;

            case RGUI_ACTION_RIGHT:
               *scale += 1.0f;
               break;

            default:
               break;
         }

         *scale = roundf(*scale);
         *scale = max(*scale, 1.0f);

         if (old_scale != *scale && !g_settings.video.fullscreen)
            rarch_set_fullscreen(g_settings.video.fullscreen); // Reinit video driver.

         break;
      }

#ifdef HAVE_THREADS
      case RGUI_SETTINGS_VIDEO_THREADED:
      {
         bool old = g_settings.video.threaded;
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
            g_settings.video.threaded = !g_settings.video.threaded;
         else if (action == RGUI_ACTION_START)
            g_settings.video.threaded = false;

         if (g_settings.video.threaded != old)
            rarch_set_fullscreen(g_settings.video.fullscreen); // Reinit video driver.
         break;
      }
#endif

      case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
      {
         unsigned old = g_settings.video.swap_interval;
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.swap_interval = 1;
               break;

            case RGUI_ACTION_LEFT:
               g_settings.video.swap_interval--;
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               g_settings.video.swap_interval++;
               break;

            default:
               break;
         }

         g_settings.video.swap_interval = min(g_settings.video.swap_interval, 4);
         g_settings.video.swap_interval = max(g_settings.video.swap_interval, 1);
         if (old != g_settings.video.swap_interval && driver.video && driver.video_data)
            video_set_nonblock_state_func(false); // This will update the current swap interval. Since we're in RGUI now, always apply VSync.

         break;
      }

      case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_settings.video.hard_sync_frames = 0;
               break;

            case RGUI_ACTION_LEFT:
               if (g_settings.video.hard_sync_frames > 0)
                  g_settings.video.hard_sync_frames--;
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_settings.video.hard_sync_frames < 3)
                  g_settings.video.hard_sync_frames++;
               break;

            default:
               break;
         }
         break;

      case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
         switch (action)
         {
            case RGUI_ACTION_START:
               g_extern.measure_data.frame_time_samples_count = 0;
               break;

            case RGUI_ACTION_OK:
            {
               double refresh_rate = 0.0;
               double deviation = 0.0;
               unsigned sample_points = 0;
               if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
               {
                  driver_set_monitor_refresh_rate(refresh_rate);
                  // Incase refresh rate update forced non-block video.
                  video_set_nonblock_state_func(false);
               }
               break;
            }

            default:
               break;
         }
         break;
#ifdef HAVE_SHADER_MANAGER
      case RGUI_SETTINGS_SHADER_PASSES:
         switch (action)
         {
            case RGUI_ACTION_START:
               rgui->shader.passes = 0;
               break;

            case RGUI_ACTION_LEFT:
               if (rgui->shader.passes)
               {
                  rgui->shader.passes--;
                  rgui->need_refresh = true;
               }
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (rgui->shader.passes < RGUI_MAX_SHADERS)
               {
                  rgui->shader.passes++;
                  rgui->need_refresh = true;
               }
               break;

            default:
               break;
         }

#ifndef HAVE_RMENU
         rgui->need_refresh = true;
#endif
         break;
      case RGUI_SETTINGS_RSOUND_SERVER_IP_ADDRESS:
#if defined(HAVE_RSOUND) && defined(HAVE_OSK)
         if (action == RGUI_ACTION_OK)
         {
            g_extern.osk.cb_init     = osk_callback_enter_rsound_init;
            g_extern.osk.cb_callback = osk_callback_enter_rsound;
         }
#endif
         break;
      case RGUI_SETTINGS_SHADER_APPLY:
         {
            if (!driver.video->set_shader || action != RGUI_ACTION_OK)
               return 0;

            RARCH_LOG("Applying shader ...\n");

            enum rarch_shader_type type = shader_manager_get_type(&rgui->shader);

            if (rgui->shader.passes && type != RARCH_SHADER_NONE)
            {
               const char *conf_path = type == RARCH_SHADER_GLSL ? rgui->default_glslp : rgui->default_cgp;

               char config_directory[PATH_MAX];
               if (*g_extern.config_path)
                  fill_pathname_basedir(config_directory, g_extern.config_path, sizeof(config_directory));
               else
                  *config_directory = '\0';

               char cgp_path[PATH_MAX];
               const char *dirs[] = {
                  g_settings.video.shader_dir,
                  g_settings.rgui_config_directory,
                  config_directory,
               };

               config_file_t *conf = config_file_new(NULL);
               if (!conf)
                  return 0;
               gfx_shader_write_conf_cgp(conf, &rgui->shader);

               bool ret = false;
               unsigned d;
               for (d = 0; d < ARRAY_SIZE(dirs); d++)
               {
                  if (!*dirs[d])
                     continue;

                  fill_pathname_join(cgp_path, dirs[d], conf_path, sizeof(cgp_path));
                  if (config_file_write(conf, cgp_path))
                  {
                     RARCH_LOG("Saved shader preset to %s.\n", cgp_path);
                     shader_manager_set_preset(NULL, type, cgp_path);
                     ret = true;
                     break;
                  }
                  else
                     RARCH_LOG("Failed writing shader preset to %s.\n", cgp_path);
               }

               config_file_free(conf);
               if (!ret)
                  RARCH_ERR("Failed to save shader preset. Make sure config directory and/or shader dir are writable.\n");
            }
            else
            {
               type = gfx_shader_parse_type("", DEFAULT_SHADER_TYPE);
               if (type == RARCH_SHADER_NONE)
               {
#if defined(HAVE_GLSL)
                  type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
                  type = RARCH_SHADER_CG;
#endif
               }
               shader_manager_set_preset(NULL, type, NULL);
            }
         }
         break;
#endif
#ifdef _XBOX1
      case RGUI_SETTINGS_FLICKER_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
               if (g_extern.console.screen.flicker_filter_index > 0)
                  g_extern.console.screen.flicker_filter_index--;
               break;
            case RGUI_ACTION_RIGHT:
               if (g_extern.console.screen.flicker_filter_index < 5)
                  g_extern.console.screen.flicker_filter_index++;
               break;
            case RGUI_ACTION_START:
               g_extern.console.screen.flicker_filter_index = 0;
               break;
         }
         break;
      case RGUI_SETTINGS_SOFT_DISPLAY_FILTER:
         switch (action)
         {
            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
                  g_extern.lifecycle_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               else
                  g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
            case RGUI_ACTION_START:
               g_extern.lifecycle_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
               break;
         }
         break;
#endif
      case RGUI_SETTINGS_SHADER_PRESET_SAVE:
         if (action == RGUI_ACTION_OK)
         {
#ifdef HAVE_OSK
            g_extern.osk.cb_init = osk_callback_enter_filename_init;
            g_extern.osk.cb_callback = osk_callback_enter_filename;
#endif
         }
         break;
      default:
         break;
   }

   return 0;
}


void menu_set_settings_label(char *type_str, size_t type_str_size, unsigned *w, unsigned type)
{
   switch (type)
   {
      case RGUI_SETTINGS_VIDEO_ROTATION:
         strlcpy(type_str, rotation_lut[g_settings.video.rotation],
               type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         snprintf(type_str, type_str_size,
               (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
         break;
      case RGUI_SETTINGS_VIDEO_FILTER:
         if (g_settings.video.smooth)
            strlcpy(type_str, "Bilinear filtering", type_str_size);
         else
            strlcpy(type_str, "Point filtering", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_GAMMA:
         snprintf(type_str, type_str_size, "%d", g_extern.console.screen.gamma_correction);
         break;
      case RGUI_SETTINGS_VIDEO_VSYNC:
         strlcpy(type_str, g_settings.video.vsync ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC:
         strlcpy(type_str, g_settings.video.hard_sync ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
         strlcpy(type_str, g_settings.video.black_frame_insertion ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
         snprintf(type_str, type_str_size, "%u", g_settings.video.swap_interval);
         break;
      case RGUI_SETTINGS_VIDEO_THREADED:
         strlcpy(type_str, g_settings.video.threaded ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
         snprintf(type_str, type_str_size, "%.1fx", g_settings.video.xscale);
         break;
      case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
         snprintf(type_str, type_str_size, "%.1fx", g_settings.video.yscale);
         break;
      case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
         strlcpy(type_str, g_settings.video.crop_overscan ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
         snprintf(type_str, type_str_size, "%u", g_settings.video.hard_sync_frames);
         break;
      case RGUI_SETTINGS_DRIVER_VIDEO:
         strlcpy(type_str, g_settings.video.driver, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_AUDIO:
         strlcpy(type_str, g_settings.audio.driver, type_str_size);
         break;
      case RGUI_SETTINGS_DRIVER_INPUT:
         strlcpy(type_str, g_settings.input.driver, type_str_size);
         break;
#ifdef HAVE_CAMERA
      case RGUI_SETTINGS_DRIVER_CAMERA:
         strlcpy(type_str, g_settings.camera.driver, type_str_size);
         break;
#endif
      case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
         {
            double refresh_rate = 0.0;
            double deviation = 0.0;
            unsigned sample_points = 0;
            if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
               snprintf(type_str, type_str_size, "%.3f Hz (%.1f%% dev, %u samples)", refresh_rate, 100.0 * deviation, sample_points);
            else
               strlcpy(type_str, "N/A", type_str_size);
            break;
         }
      case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
         strlcpy(type_str, g_settings.video.scale_integer ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         strlcpy(type_str, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, type_str_size);
         break;
#if defined(GEKKO)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         strlcpy(type_str, gx_get_video_mode(), type_str_size);
         break;
#elif defined(__CELLOS_LV2__)
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         {
               unsigned width = gfx_ctx_get_resolution_width(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               unsigned height = gfx_ctx_get_resolution_height(g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx]);
               snprintf(type_str, type_str_size, "%dx%d", width, height);
         }
         break;
      case RGUI_SETTINGS_VIDEO_PAL60:
         if (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_PAL_TEMPORAL_ENABLE))
            strlcpy(type_str, "ON", sizeof(type_str));
         else
            strlcpy(type_str, "OFF", sizeof(type_str));
         break;
#endif
      case RGUI_FILE_PLAIN:
         strlcpy(type_str, "(FILE)", type_str_size);
         *w = 6;
         break;
      case RGUI_FILE_DIRECTORY:
         strlcpy(type_str, "(DIR)", type_str_size);
         *w = 5;
         break;
      case RGUI_SETTINGS_REWIND_ENABLE:
         strlcpy(type_str, g_settings.rewind_enable ? "ON" : "OFF", type_str_size);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_GPU_SCREENSHOT:
         strlcpy(type_str, g_settings.video.gpu_screenshot ? "ON" : "OFF", type_str_size);
         break;
#endif
      case RGUI_SETTINGS_REWIND_GRANULARITY:
         snprintf(type_str, type_str_size, "%u", g_settings.rewind_granularity);
         break;
      case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
         strlcpy(type_str, g_extern.config_save_on_exit ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
         if (g_settings.autosave_interval)
            snprintf(type_str, type_str_size, "%u seconds", g_settings.autosave_interval);
         else
            strlcpy(type_str, "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_SAVESTATE_SAVE:
      case RGUI_SETTINGS_SAVESTATE_LOAD:
         snprintf(type_str, type_str_size, "%d", g_extern.state_slot);
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         strlcpy(type_str, g_extern.audio_data.mute ? "ON" : "OFF", type_str_size);
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         snprintf(type_str, type_str_size, "%.3f", g_settings.audio.rate_control_delta);
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         snprintf(type_str, type_str_size, (g_settings.fps_show) ? "ON" : "OFF");
         break;
      case RGUI_BROWSER_DIR_PATH:
         strlcpy(type_str, *g_settings.rgui_browser_directory ? g_settings.rgui_browser_directory : "<default>", type_str_size);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SCREENSHOT_DIR_PATH:
         strlcpy(type_str, *g_settings.screenshot_directory ? g_settings.screenshot_directory : "<ROM dir>", type_str_size);
         break;
#endif
      case RGUI_SAVEFILE_DIR_PATH:
         strlcpy(type_str, *g_extern.savefile_dir ? g_extern.savefile_dir : "<ROM dir>", type_str_size);
         break;
#ifdef HAVE_OVERLAY
      case RGUI_OVERLAY_DIR_PATH:
         strlcpy(type_str, *g_extern.overlay_dir ? g_extern.overlay_dir : "<default>", type_str_size);
         break;
#endif
      case RGUI_SAVESTATE_DIR_PATH:
         strlcpy(type_str, *g_extern.savestate_dir ? g_extern.savestate_dir : "<ROM dir>", type_str_size);
         break;
#ifdef HAVE_DYNAMIC
      case RGUI_LIBRETRO_DIR_PATH:
         strlcpy(type_str, *rgui->libretro_dir ? rgui->libretro_dir : "<None>", type_str_size);
         break;
#endif
      case RGUI_LIBRETRO_INFO_DIR_PATH:
         strlcpy(type_str, *g_settings.libretro_info_path ? g_settings.libretro_info_path : "<Core dir>", type_str_size);
         break;
      case RGUI_CONFIG_DIR_PATH:
         strlcpy(type_str, *g_settings.rgui_config_directory ? g_settings.rgui_config_directory : "<default>", type_str_size);
         break;
      case RGUI_SHADER_DIR_PATH:
         strlcpy(type_str, *g_settings.video.shader_dir ? g_settings.video.shader_dir : "<default>", type_str_size);
         break;
      case RGUI_SYSTEM_DIR_PATH:
         strlcpy(type_str, *g_settings.system_directory ? g_settings.system_directory : "<ROM dir>", type_str_size);
         break;
      case RGUI_SETTINGS_DISK_INDEX:
         {
            const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
            unsigned images = control->get_num_images();
            unsigned current = control->get_image_index();
            if (current >= images)
               strlcpy(type_str, "No Disk", type_str_size);
            else
               snprintf(type_str, type_str_size, "%u", current + 1);
            break;
         }
      case RGUI_SETTINGS_CONFIG:
         if (*g_extern.config_path)
            fill_pathname_base(type_str, g_extern.config_path, type_str_size);
         else
            strlcpy(type_str, "<default>", type_str_size);
         break;
      case RGUI_SETTINGS_OPEN_FILEBROWSER:
      case RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE:
      case RGUI_SETTINGS_OPEN_HISTORY:
      case RGUI_SETTINGS_CORE_OPTIONS:
      case RGUI_SETTINGS_CUSTOM_VIEWPORT:
      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
      case RGUI_SETTINGS_VIDEO_OPTIONS:
      case RGUI_SETTINGS_AUDIO_OPTIONS:
      case RGUI_SETTINGS_DISK_OPTIONS:
      case RGUI_SETTINGS_SAVE_CONFIG:
#ifdef HAVE_SHADER_MANAGER
      case RGUI_SETTINGS_SHADER_OPTIONS:
      case RGUI_SETTINGS_SHADER_PRESET:
#endif
      case RGUI_SETTINGS_SHADER_PRESET_SAVE:
      case RGUI_SETTINGS_CORE:
      case RGUI_SETTINGS_DISK_APPEND:
      case RGUI_SETTINGS_INPUT_OPTIONS:
      case RGUI_SETTINGS_PATH_OPTIONS:
      case RGUI_SETTINGS_OPTIONS:
      case RGUI_SETTINGS_DRIVERS:
      case RGUI_SETTINGS_CUSTOM_BIND_ALL:
      case RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
      case RGUI_START_SCREEN:
         strlcpy(type_str, "...", type_str_size);
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_PRESET:
         strlcpy(type_str, path_basename(g_settings.input.overlay), type_str_size);
         break;
      case RGUI_SETTINGS_OVERLAY_OPACITY:
         snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_opacity);
         break;
      case RGUI_SETTINGS_OVERLAY_SCALE:
         snprintf(type_str, type_str_size, "%.2f", g_settings.input.overlay_scale);
         break;
#endif
      case RGUI_SETTINGS_BIND_PLAYER:
         snprintf(type_str, type_str_size, "#%d", rgui->current_pad + 1);
         break;
      case RGUI_SETTINGS_BIND_DEVICE:
         {
            int map = g_settings.input.joypad_map[rgui->current_pad];
            if (map >= 0 && map < MAX_PLAYERS)
            {
               const char *device_name = g_settings.input.device_names[map];
               if (*device_name)
                  strlcpy(type_str, device_name, type_str_size);
               else
                  snprintf(type_str, type_str_size, "N/A (port #%u)", map);
            }
            else
               strlcpy(type_str, "Disabled", type_str_size);
            break;
         }
      case RGUI_SETTINGS_BIND_DEVICE_TYPE:
         {
            const char *name;
            switch (g_settings.input.libretro_device[rgui->current_pad])
            {
               case RETRO_DEVICE_NONE: name = "None"; break;
               case RETRO_DEVICE_JOYPAD: name = "Joypad"; break;
               case RETRO_DEVICE_ANALOG: name = "Joypad w/ Analog"; break;
               case RETRO_DEVICE_JOYPAD_MULTITAP: name = "Multitap"; break;
               case RETRO_DEVICE_MOUSE: name = "Mouse"; break;
               case RETRO_DEVICE_LIGHTGUN_JUSTIFIER: name = "Justifier"; break;
               case RETRO_DEVICE_LIGHTGUN_JUSTIFIERS: name = "Justifiers"; break;
               case RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE: name = "SuperScope"; break;
               default: name = "Unknown"; break;
            }

            strlcpy(type_str, name, type_str_size);
            break;
         }
      case RGUI_SETTINGS_BIND_UP:
      case RGUI_SETTINGS_BIND_DOWN:
      case RGUI_SETTINGS_BIND_LEFT:
      case RGUI_SETTINGS_BIND_RIGHT:
      case RGUI_SETTINGS_BIND_A:
      case RGUI_SETTINGS_BIND_B:
      case RGUI_SETTINGS_BIND_X:
      case RGUI_SETTINGS_BIND_Y:
      case RGUI_SETTINGS_BIND_START:
      case RGUI_SETTINGS_BIND_SELECT:
      case RGUI_SETTINGS_BIND_L:
      case RGUI_SETTINGS_BIND_R:
      case RGUI_SETTINGS_BIND_L2:
      case RGUI_SETTINGS_BIND_R2:
      case RGUI_SETTINGS_BIND_L3:
      case RGUI_SETTINGS_BIND_R3:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
      case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
      case RGUI_SETTINGS_BIND_MENU_TOGGLE:
         {
            unsigned id = type - RGUI_SETTINGS_BIND_B;
            struct platform_bind key_label;
            strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
            key_label.joykey = g_settings.input.binds[rgui->current_pad][id].joykey;

            if (driver.input->set_keybinds)
            {
               driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
               strlcpy(type_str, key_label.desc, type_str_size);
            }
            else
            {
               const struct retro_keybind *bind = &g_settings.input.binds[rgui->current_pad][type - RGUI_SETTINGS_BIND_BEGIN];
               input_get_bind_string(type_str, bind, type_str_size);
            }
            break;
         }
      case RGUI_SETTINGS_AUDIO_VOLUME_LEVEL:
#ifdef RARCH_CONSOLE
         strlcpy(type_str, (g_extern.console.sound.volume_level) ? "Loud" : "Normal", type_str_size);
         break;
#endif
      case RGUI_SETTINGS_RSOUND_SERVER_IP_ADDRESS:
         strlcpy(type_str, g_settings.audio.device, type_str_size);
         break;
#ifdef _XBOX1
      case RGUI_SETTINGS_FLICKER_FILTER:
         snprintf(type_str, sizeof(type_str), "%d", g_extern.console.screen.flicker_filter_index);
         break;
      case RGUI_SETTINGS_SOFT_DISPLAY_FILTER:
         snprintf(type_str, sizeof(type_str),
               (g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
         break;
#endif
      default:
         type_str[0] = 0;
         *w = 0;
         break;
   }
}

