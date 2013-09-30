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

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "rgui.h"
#include "menu_context.h"
#include "utils/file_list.h"
#include "../../general.h"
#include "../../file_ext.h"
#include "../../gfx/gfx_common.h"
#include "../../config.def.h"
#include "../../file.h"
#include "../../dynamic.h"
#include "../../compat/posix_string.h"
#include "../../gfx/shader_parse.h"
#include "../../performance.h"

#ifdef HAVE_OPENGL
#include "../../gfx/gl_common.h"
#endif

#include "../../screenshot.h"

#ifdef GEKKO
unsigned rgui_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
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

unsigned rgui_current_gx_resolution = GX_RESOLUTIONS_640_480;
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#define HAVE_SHADER_MANAGER
#endif

unsigned RGUI_WIDTH = 320;
unsigned RGUI_HEIGHT = 240;
uint16_t menu_framebuf[400 * 240];

#ifdef HAVE_SHADER_MANAGER
static int shader_manager_toggle_setting(rgui_handle_t *rgui, unsigned setting, rgui_action_t action);
#endif

static void rgui_flush_menu_stack_type(rgui_handle_t *rgui, unsigned final_type)
{
   rgui->need_refresh = true;
   unsigned type = 0;
   rgui_list_get_last(rgui->menu_stack, NULL, &type);
   while (type != final_type)
   {
      rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
      rgui_list_get_last(rgui->menu_stack, NULL, &type);
   }
}

static bool menu_type_is_settings(unsigned type)
{
   return type == RGUI_SETTINGS ||
      type == RGUI_SETTINGS_CORE_OPTIONS ||
      type == RGUI_SETTINGS_VIDEO_OPTIONS ||
#ifdef HAVE_SHADER_MANAGER
      type == RGUI_SETTINGS_SHADER_OPTIONS ||
#endif
      type == RGUI_SETTINGS_AUDIO_OPTIONS ||
      type == RGUI_SETTINGS_DISK_OPTIONS ||
      type == RGUI_SETTINGS_PATH_OPTIONS ||
      type == RGUI_SETTINGS_OPTIONS ||
      (type == RGUI_SETTINGS_INPUT_OPTIONS);
}

#ifdef HAVE_SHADER_MANAGER
static bool menu_type_is_shader_browser(unsigned type)
{
   return (type >= RGUI_SETTINGS_SHADER_0 &&
         type <= RGUI_SETTINGS_SHADER_LAST &&
         ((type - RGUI_SETTINGS_SHADER_0) % 3) == 0) ||
      type == RGUI_SETTINGS_SHADER_PRESET;
}
#endif

static bool menu_type_is_directory_browser(unsigned type)
{
   return type == RGUI_BROWSER_DIR_PATH ||
#ifdef HAVE_SHADER_MANAGER
      type == RGUI_SHADER_DIR_PATH ||
#endif
      type == RGUI_SAVESTATE_DIR_PATH ||
#ifdef HAVE_DYNAMIC
      type == RGUI_LIBRETRO_DIR_PATH ||
#endif
      type == RGUI_CONFIG_DIR_PATH ||
      type == RGUI_SAVEFILE_DIR_PATH ||
#ifdef HAVE_OVERLAY
      type == RGUI_OVERLAY_DIR_PATH ||
#endif
#ifdef HAVE_SCREENSHOTS
      type == RGUI_SCREENSHOT_DIR_PATH ||
#endif
      type == RGUI_SYSTEM_DIR_PATH;
}

static void rgui_settings_populate_entries(rgui_handle_t *rgui);

#include "rguidisp_bitmap.c"

static void *rgui_init(void)
{
   uint16_t *framebuf = menu_framebuf;
   size_t framebuf_pitch = RGUI_WIDTH * sizeof(uint16_t);

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   rgui->frame_buf = framebuf;
   rgui->frame_buf_pitch = framebuf_pitch;

   bool ret = rguidisp_init_font(rgui);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      /* TODO - should be refactored - perhaps don't do rarch_fail but instead
       * exit program */
      g_extern.lifecycle_mode_state &= ~((1ULL << MODE_MENU) | (1ULL << MODE_GAME));
      return NULL;
   }

   strlcpy(rgui->base_path, g_settings.rgui_browser_directory, sizeof(rgui->base_path));

   rgui->menu_stack = (rgui_list_t*)calloc(1, sizeof(rgui_list_t));
   rgui->selection_buf = (rgui_list_t*)calloc(1, sizeof(rgui_list_t));
   rgui_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);
   rgui->selection_ptr = 0;
   rgui_settings_populate_entries(rgui);

   // Make sure that custom viewport is something sane incase we use it
   // before it's configured.
   rarch_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
   if (driver.video_data && (!custom->width || !custom->height))
   {
      driver.video->viewport_info(driver.video_data, custom);
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (float)custom->width / custom->height;
   }
   else if (DEFAULT_ASPECT_RATIO > 0.0f)
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value = DEFAULT_ASPECT_RATIO;
   else
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value = 4.0f / 3.0f; // Something arbitrary

   return rgui;
}

static void rgui_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (rgui->alloc_font)
      free((uint8_t*)rgui->font);

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   rgui_list_free(rgui->menu_stack);
   rgui_list_free(rgui->selection_buf);
}

static int rgui_core_setting_toggle(unsigned setting, rgui_action_t action)
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

static int rgui_settings_toggle_setting(rgui_handle_t *rgui, unsigned setting, rgui_action_t action, unsigned menu_type)
{
#ifdef HAVE_SHADER_MANAGER
   if (setting >= RGUI_SETTINGS_SHADER_FILTER && setting <= RGUI_SETTINGS_SHADER_LAST)
      return shader_manager_toggle_setting(rgui, setting, action);
#endif
   if (setting >= RGUI_SETTINGS_CORE_OPTION_START)
      return rgui_core_setting_toggle(setting, action);

   return menu_set_settings(setting, action);
}

static void rgui_settings_audio_options_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);
   rgui_list_push(rgui->selection_buf, "Mute Audio", RGUI_SETTINGS_AUDIO_MUTE, 0);
   rgui_list_push(rgui->selection_buf, "Rate Control Delta", RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA, 0);
}

static void rgui_settings_disc_options_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);
   rgui_list_push(rgui->selection_buf, "Disk Index", RGUI_SETTINGS_DISK_INDEX, 0);
   rgui_list_push(rgui->selection_buf, "Disk Image Append", RGUI_SETTINGS_DISK_APPEND, 0);
}

static void rgui_settings_options_populate_entries(rgui_handle_t *rgui)
{
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
   rgui_list_push(rgui->selection_buf, "Debug Info Messages", RGUI_SETTINGS_DEBUG_TEXT, 0);
}

static void rgui_settings_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);

#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
   rgui_list_push(rgui->selection_buf, "Core", RGUI_SETTINGS_CORE, 0);
#endif
   if (rgui->history)
      rgui_list_push(rgui->selection_buf, "Load Game (History)", RGUI_SETTINGS_OPEN_HISTORY, 0);
   rgui_list_push(rgui->selection_buf, "Load Game", RGUI_SETTINGS_OPEN_FILEBROWSER, 0);
   rgui_list_push(rgui->selection_buf, "Core Options", RGUI_SETTINGS_CORE_OPTIONS, 0);
   rgui_list_push(rgui->selection_buf, "Video Options", RGUI_SETTINGS_VIDEO_OPTIONS, 0);
   rgui_list_push(rgui->selection_buf, "Audio Options", RGUI_SETTINGS_AUDIO_OPTIONS, 0);
   rgui_list_push(rgui->selection_buf, "Input Options", RGUI_SETTINGS_INPUT_OPTIONS, 0);
   rgui_list_push(rgui->selection_buf, "Path Options", RGUI_SETTINGS_PATH_OPTIONS, 0);
   rgui_list_push(rgui->selection_buf, "Settings", RGUI_SETTINGS_OPTIONS, 0);

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
   rgui_list_push(rgui->selection_buf, "Quit RetroArch", RGUI_SETTINGS_QUIT_RARCH, 0);
}

static void rgui_settings_core_options_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);

   if (g_extern.system.core_options)
   {
      size_t opts = core_option_size(g_extern.system.core_options);
      for (size_t i = 0; i < opts; i++)
         rgui_list_push(rgui->selection_buf,
               core_option_get_desc(g_extern.system.core_options, i), RGUI_SETTINGS_CORE_OPTION_START + i, 0);
   }
   else
      rgui_list_push(rgui->selection_buf, "No options available.", RGUI_SETTINGS_CORE_OPTION_NONE, 0);
}

static void rgui_settings_video_options_populate_entries(rgui_handle_t *rgui)
{
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
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
   rgui_list_push(rgui->selection_buf, "Windowed Scale (X)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X, 0);
   rgui_list_push(rgui->selection_buf, "Windowed Scale (Y)", RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y, 0);
#endif
   rgui_list_push(rgui->selection_buf, "Crop Overscan (reload)", RGUI_SETTINGS_VIDEO_CROP_OVERSCAN, 0);
   rgui_list_push(rgui->selection_buf, "Estimated Monitor FPS", RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO, 0);
}

#ifdef HAVE_SHADER_MANAGER
static void rgui_settings_shader_manager_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);
   rgui_list_push(rgui->selection_buf, "Apply Shader Changes",
         RGUI_SETTINGS_SHADER_APPLY, 0);
   rgui_list_push(rgui->selection_buf, "Default Filter", RGUI_SETTINGS_SHADER_FILTER, 0);
   rgui_list_push(rgui->selection_buf, "Load Shader Preset",
         RGUI_SETTINGS_SHADER_PRESET, 0);
   rgui_list_push(rgui->selection_buf, "Shader Passes",
         RGUI_SETTINGS_SHADER_PASSES, 0);

   for (unsigned i = 0; i < rgui->shader.passes; i++)
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
}

static int shader_manager_toggle_setting(rgui_handle_t *rgui, unsigned setting, rgui_action_t action)
{
   unsigned dist_shader = setting - RGUI_SETTINGS_SHADER_0;
   unsigned dist_filter = setting - RGUI_SETTINGS_SHADER_0_FILTER;
   unsigned dist_scale  = setting - RGUI_SETTINGS_SHADER_0_SCALE;

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
      return menu_set_settings(setting, action);
   else if ((dist_shader % 3) == 0 || setting == RGUI_SETTINGS_SHADER_PRESET)
   {
      dist_shader /= 3;
      struct gfx_shader_pass *pass = setting == RGUI_SETTINGS_SHADER_PRESET ? 
         &rgui->shader.pass[dist_shader] : NULL;
      switch (action)
      {
         case RGUI_ACTION_OK:
            rgui_list_push(rgui->menu_stack, g_settings.video.shader_dir, setting, rgui->selection_ptr);
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

static void rgui_settings_path_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);
   rgui_list_push(rgui->selection_buf, "Browser Directory", RGUI_BROWSER_DIR_PATH, 0);
   rgui_list_push(rgui->selection_buf, "Config Directory", RGUI_CONFIG_DIR_PATH, 0);
#ifdef HAVE_DYNAMIC
   rgui_list_push(rgui->selection_buf, "Core Directory", RGUI_LIBRETRO_DIR_PATH, 0);
#endif
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
}

static void rgui_settings_controller_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->selection_buf);
#ifdef HAVE_OVERLAY
   rgui_list_push(rgui->selection_buf, "Overlay Preset", RGUI_SETTINGS_OVERLAY_PRESET, 0);
   rgui_list_push(rgui->selection_buf, "Overlay Opacity", RGUI_SETTINGS_OVERLAY_OPACITY, 0);
   rgui_list_push(rgui->selection_buf, "Overlay Scale", RGUI_SETTINGS_OVERLAY_SCALE, 0);
#endif
   rgui_list_push(rgui->selection_buf, "Player", RGUI_SETTINGS_BIND_PLAYER, 0);
   rgui_list_push(rgui->selection_buf, "Device", RGUI_SETTINGS_BIND_DEVICE, 0);
   rgui_list_push(rgui->selection_buf, "Device Type", RGUI_SETTINGS_BIND_DEVICE_TYPE, 0);

   if (driver.input && driver.input->set_keybinds)
   {
      rgui_list_push(rgui->selection_buf, "DPad Emulation", RGUI_SETTINGS_BIND_DPAD_EMULATION, 0);
      rgui_list_push(rgui->selection_buf, "Up", RGUI_SETTINGS_BIND_UP, 0);
      rgui_list_push(rgui->selection_buf, "Down", RGUI_SETTINGS_BIND_DOWN, 0);
      rgui_list_push(rgui->selection_buf, "Left", RGUI_SETTINGS_BIND_LEFT, 0);
      rgui_list_push(rgui->selection_buf, "Right", RGUI_SETTINGS_BIND_RIGHT, 0);
      rgui_list_push(rgui->selection_buf, "A", RGUI_SETTINGS_BIND_A, 0);
      rgui_list_push(rgui->selection_buf, "B", RGUI_SETTINGS_BIND_B, 0);
      rgui_list_push(rgui->selection_buf, "X", RGUI_SETTINGS_BIND_X, 0);
      rgui_list_push(rgui->selection_buf, "Y", RGUI_SETTINGS_BIND_Y, 0);
      rgui_list_push(rgui->selection_buf, "Start", RGUI_SETTINGS_BIND_START, 0);
      rgui_list_push(rgui->selection_buf, "Select", RGUI_SETTINGS_BIND_SELECT, 0);
      rgui_list_push(rgui->selection_buf, "L", RGUI_SETTINGS_BIND_L, 0);
      rgui_list_push(rgui->selection_buf, "R", RGUI_SETTINGS_BIND_R, 0);
      rgui_list_push(rgui->selection_buf, "L2", RGUI_SETTINGS_BIND_L2, 0);
      rgui_list_push(rgui->selection_buf, "R2", RGUI_SETTINGS_BIND_R2, 0);
      rgui_list_push(rgui->selection_buf, "L3", RGUI_SETTINGS_BIND_L3, 0);
      rgui_list_push(rgui->selection_buf, "R3", RGUI_SETTINGS_BIND_R3, 0);
   }
}

static int rgui_viewport_iterate(rgui_handle_t *rgui, rgui_action_t action)
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

   render_text(rgui);

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
   render_messagebox(rgui, msg);

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

static int rgui_settings_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rgui->frame_buf_pitch = RGUI_WIDTH * 2;
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
         if (type == RGUI_SETTINGS_OPEN_FILEBROWSER && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->menu_stack, rgui->base_path, RGUI_FILE_DIRECTORY, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if ((type == RGUI_SETTINGS_OPEN_HISTORY || menu_type_is_directory_browser(type)) && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->menu_stack, "", type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if ((menu_type_is_settings(type) || type == RGUI_SETTINGS_CORE || type == RGUI_SETTINGS_CONFIG || type == RGUI_SETTINGS_DISK_APPEND) && action == RGUI_ACTION_OK)
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
#ifdef HAVE_SHADER_MANAGER
            menu_type_is_shader_browser(menu_type) ||
#endif
            menu_type_is_directory_browser(menu_type) ||
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY))
   {
      rgui->need_refresh = false;
      if (menu_type == RGUI_SETTINGS_INPUT_OPTIONS)
         rgui_settings_controller_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_PATH_OPTIONS)
         rgui_settings_path_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_OPTIONS)
         rgui_settings_options_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_CORE_OPTIONS)
         rgui_settings_core_options_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_AUDIO_OPTIONS)
         rgui_settings_audio_options_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_DISK_OPTIONS)
         rgui_settings_disc_options_populate_entries(rgui);
      else if (menu_type == RGUI_SETTINGS_VIDEO_OPTIONS)
         rgui_settings_video_options_populate_entries(rgui);
#ifdef HAVE_SHADER_MANAGER
      else if (menu_type == RGUI_SETTINGS_SHADER_OPTIONS)
         rgui_settings_shader_manager_populate_entries(rgui);
#endif
      else
         rgui_settings_populate_entries(rgui);
   }

   render_text(rgui);

   return 0;
}

static void history_parse(rgui_handle_t *rgui)
{
   size_t history_size = rom_history_size(rgui->history);
   for (size_t i = 0; i < history_size; i++)
   {
      const char *path = NULL;
      const char *core_path = NULL;
      const char *core_name = NULL;

      rom_history_get_index(rgui->history, i,
            &path, &core_path, &core_name);

      char fill_buf[PATH_MAX];

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
}

static bool rgui_directory_parse(rgui_handle_t *rgui, const char *directory, unsigned menu_type, void *ctx)
{
   if (!*directory)
   {
#if defined(GEKKO)
#ifdef HW_RVL
      rgui_list_push(ctx, "sd:/", menu_type, 0);
      rgui_list_push(ctx, "usb:/", menu_type, 0);
#endif
#if !(defined(HAVE_MINIOGC) && defined(HW_RVL))
      rgui_list_push(ctx, "carda:/", menu_type, 0);
      rgui_list_push(ctx, "cardb:/", menu_type, 0);
#endif
#elif defined(_XBOX1)
      rgui_list_push(ctx, "C:\\", menu_type, 0);
      rgui_list_push(ctx, "D:\\", menu_type, 0);
      rgui_list_push(ctx, "E:\\", menu_type, 0);
      rgui_list_push(ctx, "F:\\", menu_type, 0);
      rgui_list_push(ctx, "G:\\", menu_type, 0);
#elif defined(_WIN32)
      unsigned drives = GetLogicalDrives();
      char drive[] = " :\\";
      for (unsigned i = 0; i < 32; i++)
      {
         drive[0] = 'A' + i;
         if (drives & (1 << i))
            rgui_list_push(ctx, drive, menu_type, 0);
      }
#elif defined(__CELLOS_LV2__)
      rgui_list_push(ctx, "app_home:/", menu_type, 0);
      rgui_list_push(ctx, "dev_hdd0:/", menu_type, 0);
      rgui_list_push(ctx, "dev_hdd1:/", menu_type, 0);
      rgui_list_push(ctx, "host_root:/", menu_type, 0);
#else
      rgui_list_push(ctx, "/", menu_type, 0);
#endif
      return true;
   }

#if defined(GEKKO) && defined(HW_RVL)
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(directory);

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
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == RGUI_SETTINGS_SHADER_PRESET)
      exts = "cgp|glslp";
   else if (menu_type_is_shader_browser(menu_type))
      exts = "cg|glsl";
#endif
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
      exts = "cfg";
#endif
   else if (menu_type_is_directory_browser(menu_type))
      exts = ""; // we ignore files anyway
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

   struct string_list *list = dir_list_new(directory, exts, true);
   if (!list)
      return false;

   dir_list_sort(list, true);

   if (menu_type_is_directory_browser(menu_type))
      rgui_list_push(ctx, "<Use this directory>", RGUI_FILE_USE_DIRECTORY, 0);

   for (size_t i = 0; i < list->size; i++)
   {
      bool is_dir = list->elems[i].attr.b;

      if (menu_type_is_directory_browser(menu_type) && !is_dir)
         continue;

#ifdef HAVE_LIBRETRO_MANAGEMENT
      if (menu_type == RGUI_SETTINGS_CORE && (is_dir || strcasecmp(list->elems[i].data, SALAMANDER_FILE) == 0))
         continue;
#endif

      // Need to preserve slash first time.
      const char *path = list->elems[i].data;
      if (*directory)
         path = path_basename(path);

      // Push menu_type further down in the chain.
      // Needed for shader manager currently.
      rgui_list_push(ctx, path,
            is_dir ? menu_type : RGUI_FILE_PLAIN, 0);
   }

   string_list_free(list);
   return true;
}

static int rgui_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const char *dir = 0;
   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);
   int ret = 0;

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_frame(driver.video_data, menu_framebuf,
            false, RGUI_WIDTH, RGUI_HEIGHT, 1.0f);

   if (menu_type_is_settings(menu_type))
      return rgui_settings_iterate(rgui, action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
      return rgui_viewport_iterate(rgui, action);

   if (rgui->need_refresh && action != RGUI_ACTION_MESSAGE)
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

      case RGUI_ACTION_LEFT:
         if (rgui->selection_ptr > 8)
            rgui->selection_ptr -= 8;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_RIGHT:
         if (rgui->selection_ptr + 8 < rgui->selection_buf->size)
            rgui->selection_ptr += 8;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;
      case RGUI_ACTION_SCROLL_UP:
         if (rgui->selection_ptr > 16)
            rgui->selection_ptr -= 16;
         else
            rgui->selection_ptr = 0;
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         if (rgui->selection_ptr + 16 < rgui->selection_buf->size)
            rgui->selection_ptr += 16;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
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
#ifdef HAVE_SHADER_MANAGER
               menu_type_is_shader_browser(type) ||
#endif
               menu_type_is_directory_browser(type) ||
#ifdef HAVE_OVERLAY
               type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
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
            if (menu_type_is_shader_browser(menu_type))
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
            if (menu_type == RGUI_SETTINGS_CORE)
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
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_CONFIG_DIR_PATH)
            {
               strlcpy(g_settings.rgui_config_directory, dir, sizeof(g_settings.rgui_config_directory));
               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
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
               fill_pathname_join(g_extern.fullpath, dir, path, sizeof(g_extern.fullpath));
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);

               rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
               rgui->msg_force = true;
               ret = -1;
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
#ifdef HAVE_SHADER_MANAGER
            menu_type_is_shader_browser(menu_type) ||
#endif
            menu_type_is_directory_browser(menu_type) || 
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
            menu_type == RGUI_SETTINGS_OPEN_HISTORY ||
            menu_type == RGUI_SETTINGS_DISK_APPEND))
   {
      rgui->need_refresh = false;
      rgui_list_clear(rgui->selection_buf);

      if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
         history_parse(rgui);
      else
         rgui_directory_parse(rgui, dir, menu_type, rgui->selection_buf);

      // Before a refresh, we could have deleted a file on disk, causing
      // selection_ptr to suddendly be out of range. Ensure it doesn't overflow.
      if (rgui->selection_ptr >= rgui->selection_buf->size && rgui->selection_buf->size)
         rgui->selection_ptr = rgui->selection_buf->size - 1;
      else if (!rgui->selection_buf->size)
         rgui->selection_ptr = 0;
   }

   render_text(rgui);

   return ret;
}

int rgui_input_postprocess(void *data, uint64_t old_state)
{
   (void)data;

   int ret = 0;

   if ((rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   if (ret < 0)
   {
      unsigned type = 0;
      rgui_list_get_last(rgui->menu_stack, NULL, &type);
      while (type != RGUI_SETTINGS)
      {
         rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         rgui_list_get_last(rgui->menu_stack, NULL, &type);
      }
   }

   return ret;
}

const menu_ctx_driver_t menu_ctx_rgui = {
   rgui_iterate,
   rgui_init,
   rgui_free,
   "rgui",
};
