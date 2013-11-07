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
#include "file_list.h"
#include "../../general.h"
#include "../../gfx/gfx_common.h"
#include "../../config.def.h"
#include "../../file.h"
#include "../../dynamic.h"
#include "../../compat/posix_string.h"
#include "../../gfx/shader_parse.h"
#include "../../performance.h"
#include "../../input/input_common.h"

#ifdef HAVE_OPENGL
#include "../../gfx/gl_common.h"
#endif

#include "../../screenshot.h"
#include "../../gfx/fonts/bitmap.h"

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#define HAVE_SHADER_MANAGER
#endif

#if defined(_XBOX1)
#elif defined(__CELLOS_LV2__)
#define ENTRIES_HEIGHT 25
#define POSITION_MIDDLE 0.50f
#define POSITION_EDGE_MAX 1.00f
#define POSITION_EDGE_MIN 0.00f
#define POSITION_EDGE_CENTER 0.70f
#define POSITION_RENDER_OFFSET 0.20f
#define POSITION_OFFSET 0.02f
#define FONT_SIZE_NORMAL 0.95f
#define TERM_WIDTH 70
#endif

struct texture_image *menu_texture;
#ifdef HAVE_MENU_PANEL
struct texture_image *menu_panel;
#endif

static void render_background(rgui_handle_t *rgui)
{
}

static void rgui_render_messagebox(void *data, const char *message)
{
}


static void rgui_render(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   font_params_t font_parms;

   if (rgui->need_refresh && 
         (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   size_t begin = rgui->selection_ptr >= (ENTRIES_HEIGHT / 2) ?
      (rgui->selection_ptr - (ENTRIES_HEIGHT / 2)) : 0;
   size_t end = (rgui->selection_ptr + ENTRIES_HEIGHT) <= rgui->selection_buf->size ?
      rgui->selection_ptr + ENTRIES_HEIGHT : rgui->selection_buf->size;

   if (rgui->selection_buf->size <= ENTRIES_HEIGHT)
      begin = 0;

   if (end - begin > ENTRIES_HEIGHT)
      end = begin + ENTRIES_HEIGHT;
   
   render_background(rgui);

   char title[256];
   const char *dir = NULL;
   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (menu_type == RGUI_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION %s", dir);
   else if (menu_type == RGUI_SETTINGS_DEFERRED_CORE)
      snprintf(title, sizeof(title), "DETECTED CORES %s", dir);
   else if (menu_type == RGUI_SETTINGS_CONFIG)
      snprintf(title, sizeof(title), "CONFIG %s", dir);
   else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof(title), "DISK APPEND %s", dir);
   else if (menu_type == RGUI_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_DRIVERS)
      strlcpy(title, "DRIVER OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == RGUI_SETTINGS_SHADER_OPTIONS)
      strlcpy(title, "SHADER OPTIONS", sizeof(title));
#endif
   else if (menu_type == RGUI_SETTINGS_AUDIO_OPTIONS)
      strlcpy(title, "AUDIO OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_DISK_OPTIONS)
      strlcpy(title, "DISK OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_CORE_OPTIONS)
      strlcpy(title, "CORE OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
      snprintf(title, sizeof(title), "SHADER %s", dir);
#endif
   else if ((menu_type == RGUI_SETTINGS_INPUT_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_PATH_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2) ||
         menu_type == RGUI_SETTINGS_CUSTOM_BIND ||
         menu_type == RGUI_START_SCREEN ||
         menu_type == RGUI_SETTINGS)
      snprintf(title, sizeof(title), "MENU %s", dir);
   else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      strlcpy(title, "LOAD HISTORY", sizeof(title));
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
      snprintf(title, sizeof(title), "OVERLAY %s", dir);
#endif
   else if (menu_type == RGUI_BROWSER_DIR_PATH)
      snprintf(title, sizeof(title), "BROWSER DIR %s", dir);
#ifdef HAVE_SCREENSHOTS
   else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
      snprintf(title, sizeof(title), "SCREENSHOT DIR %s", dir);
#endif
   else if (menu_type == RGUI_SHADER_DIR_PATH)
      snprintf(title, sizeof(title), "SHADER DIR %s", dir);
   else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVESTATE DIR %s", dir);
#ifdef HAVE_DYNAMIC
   else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
      snprintf(title, sizeof(title), "LIBRETRO DIR %s", dir);
#endif
   else if (menu_type == RGUI_CONFIG_DIR_PATH)
      snprintf(title, sizeof(title), "CONFIG DIR %s", dir);
   else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVEFILE DIR %s", dir);
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_OVERLAY_DIR_PATH)
      snprintf(title, sizeof(title), "OVERLAY DIR %s", dir);
#endif
   else if (menu_type == RGUI_SYSTEM_DIR_PATH)
      snprintf(title, sizeof(title), "SYSTEM DIR %s", dir);
   else
   {
      if (rgui->defer_core)
         snprintf(title, sizeof(title), "GAME %s", dir);
      else
      {
         const char *core_name = rgui->info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof(title), "GAME (%s) %s", core_name, dir);
      }
   }

   char title_buf[256];
   menu_ticker_line(title_buf, TERM_WIDTH, g_extern.frame_count / 15, title, true);

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, title_buf, &font_parms);

   char title_msg[64];
   const char *core_name = rgui->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = rgui->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
   font_parms.y = POSITION_EDGE_MAX - (POSITION_OFFSET*2);
   font_parms.scale = FONT_SIZE_NORMAL;
   font_parms.color = WHITE;

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION, core_name, core_version);

   if (driver.video_poke->set_osd_msg)
      driver.video_poke->set_osd_msg(driver.video_data, title_msg, &font_parms);

   size_t i, j;

   j = 0;

   for (i = begin; i < end; i++, j++)
   {
      const char *path = 0;
      unsigned type = 0;
      rgui_list_get_at_offset(rgui->selection_buf, i, &path, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;
      if (menu_type == RGUI_SETTINGS_INPUT_OPTIONS || menu_type == RGUI_SETTINGS_CUSTOM_BIND)
         w = 21;
      else if (menu_type == RGUI_SETTINGS_PATH_OPTIONS)
         w = 24;

#ifdef HAVE_SHADER_MANAGER
      if (type >= RGUI_SETTINGS_SHADER_FILTER &&
            type <= RGUI_SETTINGS_SHADER_LAST)
      {
         // HACK. Work around that we're using the menu_type as dir type to propagate state correctly.
         if ((menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
               && (menu_type_is(type) == RGUI_SETTINGS_SHADER_OPTIONS))
         {
            type = RGUI_FILE_DIRECTORY;
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            w = 5;
         }
         else if (type == RGUI_SETTINGS_SHADER_OPTIONS || type == RGUI_SETTINGS_SHADER_PRESET)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == RGUI_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else
            shader_manager_get_str(&rgui->shader, type_str, sizeof(type_str), type);
      }
      else
#endif
      // Pretty-print libretro cores from menu.
      if (menu_type == RGUI_SETTINGS_CORE || menu_type == RGUI_SETTINGS_DEFERRED_CORE)
      {
         if (type == RGUI_FILE_PLAIN)
         {
            strlcpy(type_str, "(CORE)", sizeof(type_str));
            rgui_list_get_alt_at_offset(rgui->selection_buf, i, &path);
            w = 6;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = RGUI_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == RGUI_SETTINGS_CONFIG ||
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
      {
         if (type == RGUI_FILE_PLAIN)
         {
            strlcpy(type_str, "(FILE)", sizeof(type_str));
            w = 6;
         }
         else if (type == RGUI_FILE_USE_DIRECTORY)
         {
            *type_str = '\0';
            w = 0;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = RGUI_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type >= RGUI_SETTINGS_CORE_OPTION_START)
         strlcpy(type_str,
               core_option_get_val(g_extern.system.core_options, type - RGUI_SETTINGS_CORE_OPTION_START),
               sizeof(type_str));
      else
         menu_set_settings_label(type_str, sizeof(type_str), &w, type);

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == rgui->selection_ptr;

      strlcpy(entry_title_buf, path, sizeof(entry_title_buf));
      strlcpy(type_str_buf, type_str, sizeof(type_str_buf));

      if ((type == RGUI_FILE_PLAIN || type == RGUI_FILE_DIRECTORY))
         menu_ticker_line(entry_title_buf, TERM_WIDTH - (w + 1 + 2), g_extern.frame_count / 15, path, selected);
      else
         menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15, type_str, selected);

      snprintf(message, sizeof(message), "%c %s", selected ? '>' : ' ', entry_title_buf);

      //blit_line(rgui, x, y, message, selected);
      font_parms.x = POSITION_EDGE_MIN + POSITION_OFFSET;
      font_parms.y = POSITION_EDGE_MIN + POSITION_RENDER_OFFSET + (POSITION_OFFSET * j);
      font_parms.scale = FONT_SIZE_NORMAL;
      font_parms.color = WHITE;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, message, &font_parms);

      font_parms.x = POSITION_EDGE_CENTER + POSITION_OFFSET;

      if (driver.video_poke->set_osd_msg)
         driver.video_poke->set_osd_msg(driver.video_data, type_str, &font_parms);
   }

#ifdef GEKKO
   const char *message_queue;

   if (rgui->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      rgui->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   rgui_render_messagebox(rgui, message_queue);
#endif
}

void rmenu_set_texture(void *data, bool enable)
{
   static bool inited =false;
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (inited)
      return;

   if (driver.video_poke && driver.video_poke->set_texture_enable)
   {
      driver.video_poke->set_texture_frame(driver.video_data, menu_texture->pixels,
            enable, rgui->width, rgui->height, 1.0f);
      inited = true;
   }
}

void rgui_init_textures(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
#ifdef HAVE_MENU_PANEL
   texture_image_load("D:\\Media\\menuMainRomSelectPanel.png", menu_panel);
#endif
   texture_image_load(g_extern.menu_texture_path, menu_texture);
   rgui->width = menu_texture->width;
   rgui->height = menu_texture->height;

   rmenu_set_texture(rgui, true);
}

static void *rgui_init(void)
{
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   menu_texture = (struct texture_image*)calloc(1, sizeof(*menu_texture));
#ifdef HAVE_MENU_PANEL
   menu_panel = (struct texture_image*)calloc(1, sizeof(*menu_panel));
#endif

   rgui_init_textures(rgui);

   return rgui;
}

static void rgui_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

#ifdef _XBOX1
#ifdef HAVE_MENU_PANEL
   if (menu_panel->vertex_buf)
   {
      menu_panel->vertex_buf->Release();
      menu_panel->vertex_buf = NULL;
   }
   if (menu_panel->pixels)
   {
      menu_panel->pixels->Release();
      menu_panel->pixels = NULL;
   }
#endif
   if (menu_texture->vertex_buf)
   {
      menu_texture->vertex_buf->Release();
      menu_texture->vertex_buf = NULL;
   }
   if (menu_texture->pixels)
   {
      menu_texture->pixels->Release();
      menu_texture->pixels = NULL;
   }
#else
#ifdef HAVE_MENU_PANEL
   if (menu_panel)
   {
      free(menu_panel->pixels);
      menu_panel->pixels = NULL;
   }
#endif

   if (menu_texture)
   {
      free(menu_texture->pixels);
      menu_texture->pixels = NULL;
   }
#endif
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

   return ret;
}


const menu_ctx_driver_t menu_ctx_rmenu = {
   rmenu_set_texture,
   rgui_render_messagebox,
   rgui_render,
   rgui_init,
   rgui_free,
   "rmenu",
};
