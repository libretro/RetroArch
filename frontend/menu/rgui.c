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

static unsigned RGUI_WIDTH = 320;
static unsigned RGUI_HEIGHT = 240;
static uint16_t menu_framebuf[400 * 240];

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((RGUI_WIDTH - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((RGUI_HEIGHT - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

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

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int y, x;
   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static uint16_t gray_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = ((x + y) & 1) + 1;
#ifdef GEKKO
   return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
#else
   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
#endif
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = ((x + y) & 1) + 1;
#ifdef GEKKO
   return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
#else
   return (col << 13) | (col << 10) | (col << 5) | (12 << 0);
#endif
}

static void fill_rect(uint16_t *buf, unsigned pitch,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   unsigned j, i;
   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         buf[j * (pitch >> 1) + i] = col(i, j);
}

static void blit_line(rgui_handle_t *rgui,
      int x, int y, const char *message, bool green)
{
   int j, i;
   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (rgui->font[FONT_OFFSET((unsigned char)*message) + offset] & rem);

            if (col)
            {
               rgui->frame_buf[(y + j) * (rgui->frame_buf_pitch >> 1) + (x + i)] = green ?
#ifdef GEKKO
               (3 << 0) | (10 << 4) | (3 << 8) | (7 << 12) : 0x7FFF;
#else
               (15 << 0) | (7 << 4) | (15 << 8) | (7 << 12) : 0xFFFF;
#endif
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
      message++;
   }
}

static void init_font(rgui_handle_t *rgui, const uint8_t *font_bmp_buf)
{
   unsigned i;
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));
   rgui->alloc_font = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   rgui->font = font;
}

static bool rguidisp_init_font(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;
   bool ret = true;

   if (font_bmp_buf)
      init_font(rgui, font_bmp_buf);
   else if (font_bin_buf)
      rgui->font = font_bin_buf;
   else
      ret = false;

   return ret;
}

static void render_background(rgui_handle_t *rgui)
{
   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, 0, RGUI_WIDTH, RGUI_HEIGHT, gray_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, 5, RGUI_WIDTH - 10, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, RGUI_HEIGHT - 10, RGUI_WIDTH - 10, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, 5, 5, RGUI_HEIGHT - 10, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         RGUI_WIDTH - 10, 5, 5, RGUI_HEIGHT - 10, green_filler);
}

static void render_messagebox(rgui_handle_t *rgui, const char *message)
{
   size_t i;

   if (!message || !*message)
      return;

   struct string_list *list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   unsigned width = 0;
   unsigned glyphs_width = 0;
   for (i = 0; i < list->size; i++)
   {
      char *msg = list->elems[i].data;
      unsigned msglen = strlen(msg);
      if (msglen > TERM_WIDTH)
      {
         msg[TERM_WIDTH - 2] = '.';
         msg[TERM_WIDTH - 1] = '.';
         msg[TERM_WIDTH - 0] = '.';
         msg[TERM_WIDTH + 1] = '\0';
         msglen = TERM_WIDTH;
      }

      unsigned line_width = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width = max(width, line_width);
      glyphs_width = max(glyphs_width, msglen);
   }

   unsigned height = FONT_HEIGHT_STRIDE * list->size + 6 + 10;
   int x = (RGUI_WIDTH - width) / 2;
   int y = (RGUI_HEIGHT - height) / 2;
   
   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + 5, y + 5, width - 10, height - 10, gray_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x, y, width - 5, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + width - 5, y, 5, height - 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + 5, y + height - 5, width - 5, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x, y + 5, 5, height - 5, green_filler);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x = FONT_WIDTH_STRIDE * (glyphs_width - strlen(msg)) / 2;
      int offset_y = FONT_HEIGHT_STRIDE * i;
      blit_line(rgui, x + 8 + offset_x, y + 8 + offset_y, msg, false);
   }

   string_list_free(list);
}

static void render_text(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (rgui->need_refresh && 
         (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   size_t begin = rgui->selection_ptr >= TERM_HEIGHT / 2 ?
      rgui->selection_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->selection_ptr + TERM_HEIGHT <= rgui->selection_buf->size ?
      rgui->selection_ptr + TERM_HEIGHT : rgui->selection_buf->size;
   
   // Do not scroll if all items are visible.
   if (rgui->selection_buf->size <= TERM_HEIGHT)
      begin = 0;

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

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
   menu_ticker_line(title_buf, TERM_WIDTH - 3, g_extern.frame_count / 15, title, true);
   blit_line(rgui, TERM_START_X + 15, 15, title_buf, true);

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

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION, core_name, core_version);
   blit_line(rgui, TERM_START_X + 15, (TERM_HEIGHT * FONT_HEIGHT_STRIDE) + TERM_START_Y + 2, title_msg, true);

   unsigned x, y;
   size_t i;

   x = TERM_START_X;
   y = TERM_START_Y;

   for (i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
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

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            TERM_WIDTH - (w + 1 + 2), TERM_WIDTH - (w + 1 + 2),
            entry_title_buf,
            w,
            type_str_buf);

      blit_line(rgui, x, y, message, selected);
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

   render_messagebox(rgui, message_queue);
#endif
}

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

   return rgui;
}

static void rgui_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (rgui->alloc_font)
      free((uint8_t*)rgui->font);
}

// This only makes sense for PC so far.
// Consoles use set_keybind callbacks instead.
static int rgui_custom_bind_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   (void)action; // Have to ignore action here. Only bind that should work here is Quit RetroArch or something like that.

   render_text(rgui);

   char msg[256];
   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)", input_config_bind_map[rgui->binds.begin - RGUI_SETTINGS_BIND_BEGIN].desc);
   render_messagebox(rgui, msg);

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

static int rgui_start_screen_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   render_text(rgui);
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

   render_messagebox(rgui, msg);

   if (action == RGUI_ACTION_OK)
      rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
   return 0;
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

   render_text(rgui);

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
