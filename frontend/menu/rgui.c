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

#include "../../gfx/fonts/bitmap.h"
#include "../../screenshot.h"

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((RGUI_WIDTH - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((RGUI_HEIGHT - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

#ifdef GEKKO
enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_640_448, 
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

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
static int video_option_toggle_setting(rgui_handle_t *rgui, unsigned setting, rgui_action_t action);

static const unsigned rgui_controller_lut[] = {
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L2,
   RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L3,
   RETRO_DEVICE_ID_JOYPAD_R3,
};

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   for (int y = 0; y < FONT_HEIGHT; y++)
   {
      for (int x = 0; x < FONT_WIDTH; x++)
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

static void init_font(rgui_handle_t *rgui, const uint8_t *font_bmp_buf)
{
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));
   rgui->alloc_font = true;
   for (unsigned i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   rgui->font = font;
}

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

static void rgui_flush_menu_stack(rgui_handle_t *rgui)
{
   rgui_flush_menu_stack_type(rgui, RGUI_SETTINGS);
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

static void *rgui_init(void)
{
   uint16_t *framebuf = menu_framebuf;
   size_t framebuf_pitch = RGUI_WIDTH * sizeof(uint16_t);
   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   rgui->frame_buf = framebuf;
   rgui->frame_buf_pitch = framebuf_pitch;

   if (font_bmp_buf)
      init_font(rgui, font_bmp_buf);
   else if (font_bin_buf)
      rgui->font = font_bin_buf;
   else
   {
      RARCH_ERR("no font bmp or bin, abort");
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
   for (unsigned j = y; j < y + height; j++)
      for (unsigned i = x; i < x + width; i++)
         buf[j * (pitch >> 1) + i] = col(i, j);
}

static void blit_line(rgui_handle_t *rgui,
      int x, int y, const char *message, bool green)
{
   while (*message)
   {
      for (int j = 0; j < FONT_HEIGHT; j++)
      {
         for (int i = 0; i < FONT_WIDTH; i++)
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
   if (!message || !*message)
      return;

   char *msg = strdup(message);
   if (strlen(msg) > TERM_WIDTH)
   {
      msg[TERM_WIDTH - 2] = '.';
      msg[TERM_WIDTH - 1] = '.';
      msg[TERM_WIDTH - 0] = '.';
      msg[TERM_WIDTH + 1] = '\0';
   }

   unsigned width = strlen(msg) * FONT_WIDTH_STRIDE - 1 + 6 + 10;
   unsigned height = FONT_HEIGHT + 6 + 10;
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

   blit_line(rgui, x + 8, y + 8, msg, false);
   free(msg);
}

static void render_text(rgui_handle_t *rgui)
{
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
   else if (menu_type == RGUI_SETTINGS_CONFIG)
      snprintf(title, sizeof(title), "CONFIG %s", dir);
   else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof(title), "DISK APPEND %s", dir);
   else if (menu_type == RGUI_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof(title));
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
   else if (menu_type_is_shader_browser(menu_type))
      snprintf(title, sizeof(title), "SHADER %s", dir);
#endif
   else if ((menu_type == RGUI_SETTINGS_INPUT_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_PATH_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2) ||
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
      const char *core_name = rgui->info.library_name;
      if (!core_name)
         core_name = g_extern.system.info.library_name;
      if (!core_name)
         core_name = "No Core";

      snprintf(title, sizeof(title), "GAME (%s) %s", core_name, dir);
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

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path = 0;
      unsigned type = 0;
      rgui_list_get_at_offset(rgui->selection_buf, i, &path, &type);
      char message[256];
      char type_str[256];
      unsigned w = (menu_type == RGUI_SETTINGS_INPUT_OPTIONS || menu_type == RGUI_SETTINGS_PATH_OPTIONS) ? 24 : 19;
      unsigned port = rgui->current_pad;
      
#ifdef HAVE_SHADER_MANAGER
      if (type >= RGUI_SETTINGS_SHADER_FILTER &&
            type <= RGUI_SETTINGS_SHADER_LAST)
      {
         // HACK. Work around that we're using the menu_type as dir type to propagate state correctly.
         if (menu_type_is_shader_browser(menu_type) && menu_type_is_shader_browser(type))
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
      if (menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type_is_directory_browser(menu_type))
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
         strlcpy(type_str, core_option_get_val(g_extern.system.core_options, type - RGUI_SETTINGS_CORE_OPTION_START), sizeof(type_str));
      else
      {
         switch (type)
         {
            case RGUI_SETTINGS_VIDEO_ROTATION:
               strlcpy(type_str, rotation_lut[g_settings.video.rotation],
                     sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
               snprintf(type_str, sizeof(type_str),
                     (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
               break;
            case RGUI_SETTINGS_VIDEO_FILTER:
               if (g_settings.video.smooth)
                  strlcpy(type_str, "Bilinear filtering", sizeof(type_str));
               else
                  strlcpy(type_str, "Point filtering", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_GAMMA:
               snprintf(type_str, sizeof(type_str), "%d", g_extern.console.screen.gamma_correction);
               break;
            case RGUI_SETTINGS_VIDEO_VSYNC:
               strlcpy(type_str, g_settings.video.vsync ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_HARD_SYNC:
               strlcpy(type_str, g_settings.video.hard_sync ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
               strlcpy(type_str, g_settings.video.black_frame_insertion ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.video.swap_interval);
               break;
            case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
               snprintf(type_str, sizeof(type_str), "%.1fx", g_settings.video.xscale);
               break;
            case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
               snprintf(type_str, sizeof(type_str), "%.1fx", g_settings.video.yscale);
               break;
            case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
               strlcpy(type_str, g_settings.video.crop_overscan ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.video.hard_sync_frames);
               break;
            case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
            {
               double refresh_rate = 0.0;
               double deviation = 0.0;
               unsigned sample_points = 0;
               if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
                  snprintf(type_str, sizeof(type_str), "%.3f Hz (%.1f%% dev, %u samples)", refresh_rate, 100.0 * deviation, sample_points);
               else
                  strlcpy(type_str, "N/A", sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
               strlcpy(type_str, g_settings.video.scale_integer ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
               strlcpy(type_str, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, sizeof(type_str));
               break;
#ifdef GEKKO
            case RGUI_SETTINGS_VIDEO_RESOLUTION:
               strlcpy(type_str, gx_get_video_mode(), sizeof(type_str));
               break;
#endif

            case RGUI_FILE_PLAIN:
               strlcpy(type_str, "(FILE)", sizeof(type_str));
               w = 6;
               break;
            case RGUI_FILE_DIRECTORY:
               strlcpy(type_str, "(DIR)", sizeof(type_str));
               w = 5;
               break;
            case RGUI_SETTINGS_REWIND_ENABLE:
               strlcpy(type_str, g_settings.rewind_enable ? "ON" : "OFF", sizeof(type_str));
               break;
#ifdef HAVE_SCREENSHOTS
            case RGUI_SETTINGS_GPU_SCREENSHOT:
               strlcpy(type_str, g_settings.video.gpu_screenshot ? "ON" : "OFF", sizeof(type_str));
               break;
#endif
            case RGUI_SETTINGS_REWIND_GRANULARITY:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.rewind_granularity);
               break;
            case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
               strlcpy(type_str, g_extern.config_save_on_exit ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_SRAM_AUTOSAVE:
               strlcpy(type_str, g_settings.autosave_interval ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_SAVESTATE_SAVE:
            case RGUI_SETTINGS_SAVESTATE_LOAD:
               snprintf(type_str, sizeof(type_str), "%d", g_extern.state_slot);
               break;
            case RGUI_SETTINGS_AUDIO_MUTE:
               strlcpy(type_str, g_extern.audio_data.mute ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
               snprintf(type_str, sizeof(type_str), "%.3f", g_settings.audio.rate_control_delta);
               break;
            case RGUI_SETTINGS_DEBUG_TEXT:
               snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
               break;
            case RGUI_BROWSER_DIR_PATH:
               strlcpy(type_str, *g_settings.rgui_browser_directory ? g_settings.rgui_browser_directory : "<default>", sizeof(type_str));
               break;
#ifdef HAVE_SCREENSHOTS
            case RGUI_SCREENSHOT_DIR_PATH:
               strlcpy(type_str, *g_settings.screenshot_directory ? g_settings.screenshot_directory : "<ROM dir>", sizeof(type_str));
               break;
#endif
            case RGUI_SAVEFILE_DIR_PATH:
               strlcpy(type_str, *g_extern.savefile_dir ? g_extern.savefile_dir : "<ROM dir>", sizeof(type_str));
               break;
#ifdef HAVE_OVERLAY
            case RGUI_OVERLAY_DIR_PATH:
               strlcpy(type_str, *g_extern.overlay_dir ? g_extern.overlay_dir : "<default>", sizeof(type_str));
               break;
#endif
            case RGUI_SAVESTATE_DIR_PATH:
               strlcpy(type_str, *g_extern.savestate_dir ? g_extern.savestate_dir : "<ROM dir>", sizeof(type_str));
               break;
#ifdef HAVE_DYNAMIC
            case RGUI_LIBRETRO_DIR_PATH:
               strlcpy(type_str, *rgui->libretro_dir ? rgui->libretro_dir : "<None>", sizeof(type_str));
               break;
#endif
            case RGUI_CONFIG_DIR_PATH:
               strlcpy(type_str, *g_settings.rgui_config_directory ? g_settings.rgui_config_directory : "<default>", sizeof(type_str));
               break;
            case RGUI_SHADER_DIR_PATH:
               strlcpy(type_str, *g_settings.video.shader_dir ? g_settings.video.shader_dir : "<default>", sizeof(type_str));
               break;
            case RGUI_SYSTEM_DIR_PATH:
               strlcpy(type_str, *g_settings.system_directory ? g_settings.system_directory : "<ROM dir>", sizeof(type_str));
               break;
            case RGUI_SETTINGS_DISK_INDEX:
            {
               const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
               unsigned images = control->get_num_images();
               unsigned current = control->get_image_index();
               if (current >= images)
                  strlcpy(type_str, "No Disk", sizeof(type_str));
               else
                  snprintf(type_str, sizeof(type_str), "%u", current + 1);
               break;
            }
            case RGUI_SETTINGS_CONFIG:
               if (*g_extern.config_path)
                  fill_pathname_base(type_str, g_extern.config_path, sizeof(type_str));
               else
                  strlcpy(type_str, "<default>", sizeof(type_str));
               break;
            case RGUI_SETTINGS_OPEN_FILEBROWSER:
            case RGUI_SETTINGS_OPEN_HISTORY:
            case RGUI_SETTINGS_CORE_OPTIONS:
            case RGUI_SETTINGS_CUSTOM_VIEWPORT:
            case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
            case RGUI_SETTINGS_VIDEO_OPTIONS:
            case RGUI_SETTINGS_AUDIO_OPTIONS:
            case RGUI_SETTINGS_DISK_OPTIONS:
#ifdef HAVE_SHADER_MANAGER
            case RGUI_SETTINGS_SHADER_OPTIONS:
            case RGUI_SETTINGS_SHADER_PRESET:
#endif
            case RGUI_SETTINGS_CORE:
            case RGUI_SETTINGS_DISK_APPEND:
            case RGUI_SETTINGS_INPUT_OPTIONS:
            case RGUI_SETTINGS_PATH_OPTIONS:
            case RGUI_SETTINGS_OPTIONS:
               strlcpy(type_str, "...", sizeof(type_str));
               break;
#ifdef HAVE_OVERLAY
            case RGUI_SETTINGS_OVERLAY_PRESET:
               strlcpy(type_str, path_basename(g_settings.input.overlay), sizeof(type_str));
               break;

            case RGUI_SETTINGS_OVERLAY_OPACITY:
            {
               snprintf(type_str, sizeof(type_str), "%.2f", g_settings.input.overlay_opacity);
               break;
            }

            case RGUI_SETTINGS_OVERLAY_SCALE:
            {
               snprintf(type_str, sizeof(type_str), "%.2f", g_settings.input.overlay_scale);
               break;
            }
#endif
            case RGUI_SETTINGS_BIND_PLAYER:
            {
               snprintf(type_str, sizeof(type_str), "#%d", port + 1);
               break;
            }
            case RGUI_SETTINGS_BIND_DEVICE:
            {
               int map = g_settings.input.joypad_map[port];
               if (map >= 0 && map < MAX_PLAYERS)
               {
                  const char *device_name = g_settings.input.device_names[map];
                  if (*device_name)
                     strlcpy(type_str, device_name, sizeof(type_str));
                  else
                     snprintf(type_str, sizeof(type_str), "N/A (port #%u)", map);
               }
               else
                  strlcpy(type_str, "Disabled", sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_BIND_DEVICE_TYPE:
            {
               const char *name;
               switch (g_settings.input.libretro_device[port])
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

               strlcpy(type_str, name, sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_BIND_DPAD_EMULATION:
               switch (g_settings.input.dpad_emulation[port])
               {
                  case ANALOG_DPAD_NONE:
                     strlcpy(type_str, "None", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_LSTICK:
                     strlcpy(type_str, "Left Stick", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_DUALANALOG:
                     strlcpy(type_str, "Dual Analog", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_RSTICK:
                     strlcpy(type_str, "Right Stick", sizeof(type_str));
                     break;
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
               {
                  unsigned id = rgui_controller_lut[type - RGUI_SETTINGS_BIND_UP];
                  struct platform_bind key_label;
                  strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
                  key_label.joykey = g_settings.input.binds[port][id].joykey;

                  if (driver.input->set_keybinds)
                     driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

                  strlcpy(type_str, key_label.desc, sizeof(type_str));
               }
               break;
            default:
               type_str[0] = 0;
               w = 0;
               break;
         }
      }

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

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

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
   unsigned port = rgui->current_pad;

   if (setting >= RGUI_SETTINGS_VIDEO_OPTIONS_FIRST && setting <= RGUI_SETTINGS_VIDEO_OPTIONS_LAST)
      return video_option_toggle_setting(rgui, setting, action);
#ifdef HAVE_SHADER_MANAGER
   else if (setting >= RGUI_SETTINGS_SHADER_FILTER && setting <= RGUI_SETTINGS_SHADER_LAST)
      return shader_manager_toggle_setting(rgui, setting, action);
#endif
   if (setting >= RGUI_SETTINGS_CORE_OPTION_START)
      return rgui_core_setting_toggle(setting, action);

   switch (setting)
   {
      case RGUI_SETTINGS_REWIND_ENABLE:
         if (action == RGUI_ACTION_OK ||
               action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT)
         {
            settings_set(1ULL << S_REWIND);
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
            settings_set(1ULL << S_REWIND_GRANULARITY_INCREMENT);
         else if (action == RGUI_ACTION_LEFT)
            settings_set(1ULL << S_REWIND_GRANULARITY_DECREMENT);
         else if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_REWIND_GRANULARITY);
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
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
      case RGUI_SETTINGS_SRAM_AUTOSAVE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_RIGHT || action == RGUI_ACTION_LEFT)
         {
            rarch_deinit_autosave();
            g_settings.autosave_interval = (!g_settings.autosave_interval) * 10;
            if (g_settings.autosave_interval)
               rarch_init_autosave();
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
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }
         else if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_SAVE_STATE);
         else if (action == RGUI_ACTION_LEFT)
            settings_set(1ULL << S_SAVESTATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            settings_set(1ULL << S_SAVESTATE_INCREMENT);
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
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_AUDIO_MUTE);
         else
            settings_set(1ULL << S_AUDIO_MUTE);
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
         if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_AUDIO_CONTROL_RATE);
         else if (action == RGUI_ACTION_LEFT)
            settings_set(1ULL << S_AUDIO_CONTROL_RATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            settings_set(1ULL << S_AUDIO_CONTROL_RATE_INCREMENT);
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
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
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }
         break;
      case RGUI_SETTINGS_RESUME_GAME:
         if (action == RGUI_ACTION_OK && (g_extern.main_is_init))
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
      case RGUI_SETTINGS_QUIT_RARCH:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            return -1;
         }
         break;
#ifdef HAVE_OVERLAY
      case RGUI_SETTINGS_OVERLAY_PRESET:
         switch (action)
         {
            case RGUI_ACTION_OK:
               rgui_list_push(rgui->menu_stack, g_extern.overlay_dir, setting, rgui->selection_ptr);
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
               settings_set(1ULL << S_INPUT_OVERLAY_OPACITY_DECREMENT);
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               settings_set(1ULL << S_INPUT_OVERLAY_OPACITY_INCREMENT);
               break;

            case RGUI_ACTION_START:
               settings_set(1ULL << S_DEF_INPUT_OVERLAY_OPACITY);
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
               settings_set(1ULL << S_INPUT_OVERLAY_SCALE_DECREMENT);
               break;

            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               settings_set(1ULL << S_INPUT_OVERLAY_SCALE_INCREMENT);
               break;

            case RGUI_ACTION_START:
               settings_set(1ULL << S_DEF_INPUT_OVERLAY_SCALE);
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

            switch (g_settings.input.dpad_emulation[port])
            {
               case ANALOG_DPAD_LSTICK:
                  keybind_action |= (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                  break;
               case ANALOG_DPAD_RSTICK:
                  keybind_action |= (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK);
                  break;
               case ANALOG_DPAD_NONE:
                  keybind_action |= (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE);
                  break;
               default:
                  break;
            }

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

         unsigned current_device = g_settings.input.libretro_device[port];
         unsigned current_index = 0;
         for (unsigned i = 0; i < ARRAY_SIZE(device_types); i++)
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
      case RGUI_SETTINGS_BIND_DPAD_EMULATION:
         g_settings.input.dpad_emulation[port] += ANALOG_DPAD_LAST;
         if (action == RGUI_ACTION_START)
            g_settings.input.dpad_emulation[port] = ANALOG_DPAD_LSTICK;
         else if (action == RGUI_ACTION_LEFT)
            g_settings.input.dpad_emulation[port]--;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.input.dpad_emulation[port]++;
         g_settings.input.dpad_emulation[port] %= ANALOG_DPAD_LAST;

         if (driver.input->set_keybinds)
         {
            unsigned keybind_action = 0;

            switch (g_settings.input.dpad_emulation[port])
            {
               case ANALOG_DPAD_LSTICK:
                  keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_LSTICK);
                  break;
               case ANALOG_DPAD_RSTICK:
                  keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_RSTICK);
                  break;
               case ANALOG_DPAD_NONE:
                  keybind_action = (1ULL << KEYBINDS_ACTION_SET_ANALOG_DPAD_NONE);
                  break;
               default:
                  break;
            }

            if (keybind_action)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[port], port, 0,
                     keybind_action);
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
         if (driver.input->set_keybinds)
         {
            unsigned keybind_action = KEYBINDS_ACTION_NONE;

            if (action == RGUI_ACTION_START)
               keybind_action = (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND);
            else if (action == RGUI_ACTION_LEFT)
               keybind_action = (1ULL << KEYBINDS_ACTION_DECREMENT_BIND);
            else if (action == RGUI_ACTION_RIGHT)
               keybind_action = (1ULL << KEYBINDS_ACTION_INCREMENT_BIND);

            if (keybind_action != KEYBINDS_ACTION_NONE)
               driver.input->set_keybinds(driver.input_data, g_settings.input.device[setting - RGUI_SETTINGS_BIND_UP], port,
                     rgui_controller_lut[setting - RGUI_SETTINGS_BIND_UP], keybind_action); 
         }
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
            *rgui->libretro_dir = '\0';
         break;
#endif
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
      default:
         break;
   }

   return 0;
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

static enum rarch_shader_type shader_manager_get_type(const struct gfx_shader *shader)
{
   // All shader types must be the same, or we cannot use it.
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   for (unsigned i = 0; i < shader->passes; i++)
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

static void shader_manager_set_preset(struct gfx_shader *shader, enum rarch_shader_type type, const char *path)
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
   else if (setting == RGUI_SETTINGS_SHADER_APPLY)
   {
      if (!driver.video->set_shader || action != RGUI_ACTION_OK)
         return 0;

      RARCH_LOG("Applying shader ...\n");

      enum rarch_shader_type type = shader_manager_get_type(&rgui->shader);

      if (rgui->shader.passes && type != RARCH_SHADER_NONE)
      {
         const char *conf_path = type == RARCH_SHADER_GLSL ? rgui->default_glslp : rgui->default_cgp;

         char cgp_path[PATH_MAX];
         const char *shader_dir = *g_settings.video.shader_dir ?
            g_settings.video.shader_dir : g_settings.system_directory;
         fill_pathname_join(cgp_path, shader_dir, conf_path, sizeof(cgp_path));
         config_file_t *conf = config_file_new(NULL);
         if (!conf)
            return 0;
         gfx_shader_write_conf_cgp(conf, &rgui->shader);
         config_file_write(conf, cgp_path);
         config_file_free(conf);

         shader_manager_set_preset(NULL, type, cgp_path); 
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
   else if (setting == RGUI_SETTINGS_SHADER_PASSES)
   {
      switch (action)
      {
         case RGUI_ACTION_START:
            rgui->shader.passes = 0;
            break;

         case RGUI_ACTION_LEFT:
            if (rgui->shader.passes)
               rgui->shader.passes--;
            break;

         case RGUI_ACTION_RIGHT:
         case RGUI_ACTION_OK:
            if (rgui->shader.passes < RGUI_MAX_SHADERS)
               rgui->shader.passes++;
            break;

         default:
            break;
      }

      rgui->need_refresh = true;
   }
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

static int video_option_toggle_setting(rgui_handle_t *rgui, unsigned setting, rgui_action_t action)
{
   switch (setting)
   {
      case RGUI_SETTINGS_VIDEO_ROTATION:
         if (action == RGUI_ACTION_START)
         {
            settings_set(1ULL << S_DEF_ROTATION);
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            settings_set(1ULL << S_ROTATION_DECREMENT);
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            settings_set(1ULL << S_ROTATION_INCREMENT);
            video_set_rotation_func((g_settings.video.rotation + g_extern.system.rotation) % 4);
         }
         break;

      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_HW_TEXTURE_FILTER);
         else
            settings_set(1ULL << S_HW_TEXTURE_FILTER);

         if (driver.video_poke->set_filtering)
            driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         break;

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
            settings_set(1ULL << S_DEF_SCALE_INTEGER);
         else if (action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT ||
               action == RGUI_ACTION_OK)
            settings_set(1ULL << S_SCALE_INTEGER_TOGGLE);

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         if (action == RGUI_ACTION_START)
            settings_set(1ULL << S_DEF_ASPECT_RATIO);
         else if (action == RGUI_ACTION_LEFT)
            settings_set(1ULL << S_ASPECT_RATIO_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            settings_set(1ULL << S_ASPECT_RATIO_INCREMENT);

         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         break;

      case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
         if (action == RGUI_ACTION_OK)
            rarch_set_fullscreen(!g_settings.video.fullscreen);
         break;

#ifdef GEKKO
      case RGUI_SETTINGS_VIDEO_RESOLUTION:
         if (action == RGUI_ACTION_LEFT)
         {
            if(rgui_current_gx_resolution > 0)
            {
               rgui_current_gx_resolution--;
               gx_set_video_mode(rgui_gx_resolutions[rgui_current_gx_resolution][0], rgui_gx_resolutions[rgui_current_gx_resolution][1]);
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
               gx_set_video_mode(rgui_gx_resolutions[rgui_current_gx_resolution][0],
                     rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
         break;
#endif
#ifdef HW_RVL
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
         else
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);

         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
#endif

      case RGUI_SETTINGS_VIDEO_VSYNC:
         switch (action)
         {
            case RGUI_ACTION_START:
               settings_set(1ULL << S_DEF_VIDEO_VSYNC);
               break;

            case RGUI_ACTION_LEFT:
            case RGUI_ACTION_RIGHT:
            case RGUI_ACTION_OK:
               settings_set(1ULL << S_VIDEO_VSYNC_TOGGLE);
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

      default:
         break;
   }

   return 0;
}

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

      case RGUI_ACTION_SETTINGS:
         rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
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

static bool directory_parse(rgui_handle_t *rgui, const char *directory, unsigned menu_type, void *ctx)
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

               rgui_flush_menu_stack(rgui);
            }
            else if (menu_type == RGUI_SETTINGS_CONFIG)
            {
               char config[PATH_MAX];
               fill_pathname_join(config, dir, path, sizeof(config));
               rgui_flush_menu_stack(rgui);
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

               rgui_flush_menu_stack(rgui);
               ret = -1;
            }
            else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
            {
               load_menu_game_history(rgui->selection_ptr);
               rgui_flush_menu_stack(rgui);
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

               rgui_flush_menu_stack(rgui);
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

      case RGUI_ACTION_SETTINGS:
         rgui->need_refresh = true;
         while (rgui->menu_stack->size > 1)
            rgui_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         return rgui_settings_iterate(rgui, RGUI_ACTION_REFRESH);

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
         directory_parse(rgui, dir, menu_type, rgui->selection_buf);

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

   if ((rgui->trigger_state & (1ULL << DEVICE_NAV_MENU)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_INGAME_EXIT);
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_INGAME_EXIT))
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_INGAME_EXIT);

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
