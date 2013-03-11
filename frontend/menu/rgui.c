/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "utils/file_list.h"
#include "menu_settings.h"
#include "../../general.h"
#include "../../gfx/gfx_common.h"

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
   GX_RESOLUTIONS_160_144 = 0,
   GX_RESOLUTIONS_240_160,
   GX_RESOLUTIONS_256_192,
   GX_RESOLUTIONS_256_224,
   GX_RESOLUTIONS_256_239,
   GX_RESOLUTIONS_256_240,
   GX_RESOLUTIONS_256_256,
   GX_RESOLUTIONS_256_480,
   GX_RESOLUTIONS_288_224,
   GX_RESOLUTIONS_304_224,
   GX_RESOLUTIONS_320_200,
   GX_RESOLUTIONS_320_224,
   GX_RESOLUTIONS_320_240,
   GX_RESOLUTIONS_320_256,
   GX_RESOLUTIONS_320_480,
   GX_RESOLUTIONS_352_224,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_352_256,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_400_254,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_512_239,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_512_478,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_512_512,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_640_256,
   GX_RESOLUTIONS_640_288,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

unsigned rgui_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 160, 144 },
   { 240, 160 },
   { 256, 192 },
   { 256, 224 },
   { 256, 239 },
   { 256, 240 },
   { 256, 256 },
   { 256, 480 },
   { 288, 224 },
   { 304, 224 },
   { 320, 200 },
   { 320, 224 },
   { 320, 240 },
   { 320, 256 },
   { 320, 480 },
   { 352, 224 },
   { 352, 240 },
   { 352, 256 },
   { 352, 480 },
   { 384, 224 },
   { 384, 448 },
   { 400, 254 },
   { 512, 224 },
   { 512, 239 },
   { 512, 240 },
   { 512, 384 },
   { 512, 448 },
   { 512, 478 },
   { 512, 480 },
   { 512, 512 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 640, 240 },
   { 640, 256 },
   { 640, 288 },
   { 640, 448 },
   { 640, 480 },
};

unsigned rgui_current_gx_resolution = GX_RESOLUTIONS_640_480;

static const char *rgui_device_labels[] = {
   "GameCube Controller",
   "Wiimote",
   "Wiimote + Nunchuk",
   "Classic Controller",
};
#endif

unsigned RGUI_WIDTH = 320;
unsigned RGUI_HEIGHT = 240;
uint16_t menu_framebuf[400 * 240];
rgui_handle_t *rgui;

struct rgui_handle
{
   uint16_t *frame_buf;
   size_t frame_buf_pitch;

   rgui_folder_enum_cb_t folder_cb;
   void *userdata;

   rgui_list_t *path_stack;
   rgui_list_t *folder_buf;
   int directory_ptr;
   bool need_refresh;
   bool msg_force;

   char path_buf[PATH_MAX];
   char base_path[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;
};

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

static inline bool rgui_is_controller_menu(unsigned menu_type)
{
   return (menu_type >= RGUI_SETTINGS_CONTROLLER_1 && menu_type <= RGUI_SETTINGS_CONTROLLER_4);
}

static inline bool rgui_is_viewport_menu(unsigned menu_type)
{
   return (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2);
}

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

rgui_handle_t *rgui_init(const char *base_path,
      uint16_t *framebuf, size_t framebuf_pitch,
      const uint8_t *font_bmp_buf, const uint8_t *font_bin_buf,
      rgui_folder_enum_cb_t folder_cb, void *userdata)
{
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   rgui->frame_buf = framebuf;
   rgui->frame_buf_pitch = framebuf_pitch;
   rgui->folder_cb = folder_cb;
   rgui->userdata = userdata;
   strlcpy(rgui->base_path, base_path, sizeof(rgui->base_path));

   rgui->path_stack = rgui_list_new();
   rgui->folder_buf = rgui_list_new();
   rgui_list_push(rgui->path_stack, base_path, RGUI_FILE_DIRECTORY, 0);

   if (font_bmp_buf)
      init_font(rgui, font_bmp_buf);
   else if (font_bin_buf)
      rgui->font = font_bin_buf;
   else
   {
      RARCH_ERR("no font bmp or bin, abort");
      g_extern.lifecycle_mode_state &= ~((1ULL << MODE_MENU) | (1ULL << MODE_MENU_INGAME) | (1ULL << MODE_GAME));
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
   }

   return rgui;
}

void rgui_free(rgui_handle_t *rgui)
{
   rgui_list_free(rgui->path_stack);
   rgui_list_free(rgui->folder_buf);
   if (rgui->alloc_font)
      free((uint8_t *) rgui->font);
   free(rgui);
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
      unsigned x, unsigned y, const char *message, bool green)
{
   while (*message)
   {
      for (unsigned j = 0; j < FONT_HEIGHT; j++)
      {
         for (unsigned i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            unsigned offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (rgui->font[FONT_OFFSET((unsigned char)*message) + offset] & rem);

            if (col)
               rgui->frame_buf[(y + j) * (rgui->frame_buf_pitch >> 1) + (x + i)] = green ?
#ifdef GEKKO
               (3 << 0) | (10 << 4) | (3 << 8) | (7 << 12) : 0x7FFF;
#else
               (15 << 0) | (7 << 4) | (15 << 8) | (7 << 12) : 0xFFFF;
#endif
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
   unsigned x = (RGUI_WIDTH - width) / 2;
   unsigned y = (RGUI_HEIGHT - height) / 2;
   
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

   size_t begin = rgui->directory_ptr >= TERM_HEIGHT / 2 ?
      rgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->directory_ptr + TERM_HEIGHT <= rgui_list_size(rgui->folder_buf) ?
      rgui->directory_ptr + TERM_HEIGHT : rgui_list_size(rgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_background(rgui);

   char title[256];
   const char *dir = 0;
   unsigned menu_type = 0;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, NULL);

#ifdef HAVE_LIBRETRO_MANAGEMENT
   if (menu_type == RGUI_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION");
   else
#endif
   if (rgui_is_controller_menu(menu_type) || rgui_is_viewport_menu(menu_type) || menu_type == RGUI_SETTINGS)
      snprintf(title, sizeof(title), "SETTINGS: %s", dir);
   else
      snprintf(title, sizeof(title), "FILE BROWSER: %s", dir);

   blit_line(rgui, TERM_START_X + 15, 15, title, true);

   blit_line(rgui, TERM_START_X + 15, (TERM_HEIGHT * FONT_HEIGHT_STRIDE) + TERM_START_Y + 2, g_extern.title_buf, true);
   blit_line(rgui, TERM_HEIGHT - 80, (TERM_HEIGHT * FONT_HEIGHT_STRIDE) + TERM_START_Y + 2, PACKAGE_VERSION, true);

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path = 0;
      unsigned type = 0;
      rgui_list_at(rgui->folder_buf, i, &path, &type, NULL);
      char message[256];
      char type_str[256];
      int w = rgui_is_controller_menu(menu_type) ? 26 : 19;
#ifdef RARCH_CONSOLE
      unsigned port = menu_type - RGUI_SETTINGS_CONTROLLER_1;
#endif
      switch (type)
      {
         case RGUI_FILE_PLAIN:
            snprintf(type_str, sizeof(type_str), "(FILE)");
            w = 6;
            break;
         case RGUI_FILE_DIRECTORY:
            snprintf(type_str, sizeof(type_str), "(DIR)");
            w = 5;
            break;
         case RGUI_FILE_DEVICE:
            snprintf(type_str, sizeof(type_str), "(DEV)");
            w = 5;
            break;
         case RGUI_SETTINGS_REWIND_ENABLE:
            snprintf(type_str, sizeof(type_str), g_settings.rewind_enable ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_REWIND_GRANULARITY:
            snprintf(type_str, sizeof(type_str), "%d", g_settings.rewind_granularity);
            break;
         case RGUI_SETTINGS_SAVESTATE_SAVE:
         case RGUI_SETTINGS_SAVESTATE_LOAD:
            snprintf(type_str, sizeof(type_str), "%d", g_extern.state_slot);
            break;
         case RGUI_SETTINGS_VIDEO_FILTER:
            snprintf(type_str, sizeof(type_str), g_settings.video.smooth ? "Bilinear filtering" : "Point filtering");
            break;
#ifdef HW_RVL
         case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
            snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
            break;
#endif
#ifdef GEKKO
         case RGUI_SETTINGS_VIDEO_RESOLUTION:
            snprintf(type_str, sizeof(type_str), "%s", gx_get_video_mode());
            break;
#endif
         case RGUI_SETTINGS_VIDEO_GAMMA:
            snprintf(type_str, sizeof(type_str), "%d", g_extern.console.screen.gamma_correction);
            break;
         case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
            snprintf(type_str, sizeof(type_str), "%s", aspectratio_lut[g_settings.video.aspect_ratio_idx].name);
            break;
         case RGUI_SETTINGS_VIDEO_OVERSCAN:
            snprintf(type_str, sizeof(type_str), "%.2f", g_extern.console.screen.overscan_amount);
            break;
         case RGUI_SETTINGS_VIDEO_ROTATION:
            {
               char rotate_msg[64];
               menu_settings_create_menu_item_label(rotate_msg, S_LBL_ROTATION, sizeof(rotate_msg));
               snprintf(type_str, sizeof(type_str), rotate_msg);
            }
            break;
         case RGUI_SETTINGS_AUDIO_MUTE:
            snprintf(type_str, sizeof(type_str), g_extern.audio_data.mute ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_AUDIO_CONTROL_RATE:
            snprintf(type_str, sizeof(type_str), "%.3f", g_settings.audio.rate_control_delta);
            break;
         case RGUI_SETTINGS_RESAMPLER_TYPE:
#ifdef HAVE_SINC
            if (strstr(g_settings.audio.resampler, "sinc"))
               snprintf(type_str, sizeof(type_str), "Sinc");
            else
#endif
               snprintf(type_str, sizeof(type_str), "Hermite");
            break;
         case RGUI_SETTINGS_SRAM_DIR:
            snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_STATE_DIR:
            snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_DEBUG_TEXT:
            snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_OPEN_FILEBROWSER:
         case RGUI_SETTINGS_CUSTOM_VIEWPORT:
#ifdef HAVE_LIBRETRO_MANAGEMENT
         case RGUI_SETTINGS_CORE:
#endif
         case RGUI_SETTINGS_CONTROLLER_1:
         case RGUI_SETTINGS_CONTROLLER_2:
         case RGUI_SETTINGS_CONTROLLER_3:
         case RGUI_SETTINGS_CONTROLLER_4:
            snprintf(type_str, sizeof(type_str), "...");
            break;
#ifdef GEKKO
         case RGUI_SETTINGS_BIND_DEVICE:
            snprintf(type_str, sizeof(type_str), "%s", rgui_device_labels[g_settings.input.device[port]]);
            break;
#endif
#ifdef RARCH_CONSOLE
         case RGUI_SETTINGS_BIND_DPAD_EMULATION:
            snprintf(type_str, sizeof(type_str), "%s", rarch_dpad_emulation_name_lut[g_settings.input.dpad_emulation[port]]);
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
            snprintf(type_str, sizeof(type_str), "%s", rarch_input_find_platform_key_label(g_settings.input.binds[port][rgui_controller_lut[type - RGUI_SETTINGS_BIND_UP]].joykey));
            break;
#endif
         default:
            type_str[0] = 0;
            w = 0;
            break;
      }

      const char *entry_title;
      char tmp[256];
      size_t path_len = strlen(path);
      // trim long filenames
      if ((type == RGUI_FILE_PLAIN || type == RGUI_FILE_DIRECTORY) && path_len > TERM_WIDTH - (w + 1 + 2))
      {
         snprintf(tmp, sizeof(tmp), "%.*s...%s", TERM_WIDTH - (w + 1 + 2) - 8, path, &path[path_len - 5]);
         entry_title = tmp;
      }
      else
      {
         entry_title = path;
      }

      snprintf(message, sizeof(message), "%c %-*.*s %-*s\n",
            i == rgui->directory_ptr ? '>' : ' ',
            TERM_WIDTH - (w + 1 + 2), TERM_WIDTH - (w + 1 + 2),
            entry_title,
            w,
            type_str);

      blit_line(rgui, x, y, message, i == rgui->directory_ptr);
   }

#ifdef GEKKO
   const char *message_queue;

   if (rgui->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      rgui->msg_force = false;
   }
   else
   {
      message_queue = driver.current_msg;
   }
   render_messagebox(rgui, message_queue);
#endif
}

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

static int rgui_settings_toggle_setting(unsigned setting, rgui_action_t action, unsigned menu_type)
{
#ifdef RARCH_CONSOLE
      unsigned port = menu_type - RGUI_SETTINGS_CONTROLLER_1;
#endif

   switch (setting)
   {
      case RGUI_SETTINGS_REWIND_ENABLE:
         if (action == RGUI_ACTION_OK || action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
         {
            menu_settings_set(S_REWIND);

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               menu_settings_msg(S_MSG_RESTART_RARCH, 180);
         }
         else if (action == RGUI_ACTION_START)
            g_settings.rewind_enable = false;
         break;
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
            menu_settings_set_default(S_DEF_SAVE_STATE);
         else if (action == RGUI_ACTION_LEFT)
            menu_settings_set(S_SAVESTATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            menu_settings_set(S_SAVESTATE_INCREMENT);
         break;
#ifdef HAVE_SCREENSHOTS
      case RGUI_SETTINGS_SCREENSHOT:
         if (action == RGUI_ACTION_OK)
         {
            const uint16_t *data = (const uint16_t*)g_extern.frame_cache.data;
            unsigned width       = g_extern.frame_cache.width;
            unsigned height      = g_extern.frame_cache.height;
            int pitch            = g_extern.frame_cache.pitch;

#ifdef RARCH_CONSOLE
            const char *screenshot_dir = default_paths.port_dir;
#else
            const char *screenshot_dir = g_settings.screenshot_directory;
#endif

            // Negative pitch is needed as screenshot takes bottom-up,
            // but we use top-down.
            bool r = screenshot_dump(screenshot_dir,
                  data + (height - 1) * (pitch >> 1), 
                  width, height, -pitch, false);

            msg_queue_push(g_extern.msg_queue, r ? "Screenshot saved" : "Screenshot failed to save", 1, 90);
         }
         break;
#endif
#ifndef HAVE_DYNAMIC
      case RGUI_SETTINGS_RESTART_GAME:
         if (action == RGUI_ACTION_OK)
         {
            rarch_game_reset();
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            return -1;
         }
         break;
#endif
      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            menu_settings_set_default(S_DEF_HW_TEXTURE_FILTER);
         else
            menu_settings_set(S_HW_TEXTURE_FILTER);

         if (driver.video_poke->set_filtering)
            driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
         break;
#ifdef HW_RVL
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE);

            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         break;
#endif
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
            if(rgui_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
            {
#ifdef HW_RVL
               if ((rgui_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
                  if (CONF_GetVideo() != CONF_VIDEO_PAL)
                     return 0;
#endif

               rgui_current_gx_resolution++;
               gx_set_video_mode(rgui_gx_resolutions[rgui_current_gx_resolution][0], rgui_gx_resolutions[rgui_current_gx_resolution][1]);
            }
         }
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
      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         if (action == RGUI_ACTION_START)
            menu_settings_set_default(S_DEF_ASPECT_RATIO);
         else if (action == RGUI_ACTION_LEFT)
            menu_settings_set(S_ASPECT_RATIO_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            menu_settings_set(S_ASPECT_RATIO_INCREMENT);

         if (driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         break;
      case RGUI_SETTINGS_VIDEO_ROTATION:
         if (action == RGUI_ACTION_START)
         {
            menu_settings_set_default(S_DEF_AUDIO_CONTROL_RATE);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            menu_settings_set(S_ROTATION_DECREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            menu_settings_set(S_ROTATION_INCREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         break;
      case RGUI_SETTINGS_VIDEO_OVERSCAN:
         if (action == RGUI_ACTION_START)
         {
            menu_settings_set_default(S_DEF_OVERSCAN);
            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            menu_settings_set(S_OVERSCAN_DECREMENT);
            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            menu_settings_set(S_OVERSCAN_INCREMENT);
            if (driver.video_poke->apply_state_changes)
               driver.video_poke->apply_state_changes(driver.video_data);
         }
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         if (action == RGUI_ACTION_START)
            menu_settings_set_default(S_DEF_AUDIO_MUTE);
         else
            menu_settings_set(S_AUDIO_MUTE);
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE:
         if (action == RGUI_ACTION_START)
            menu_settings_set_default(S_DEF_AUDIO_CONTROL_RATE);
         else if (action == RGUI_ACTION_LEFT)
            menu_settings_set(S_AUDIO_CONTROL_RATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            menu_settings_set(S_AUDIO_CONTROL_RATE_INCREMENT);
         break;
      case RGUI_SETTINGS_RESAMPLER_TYPE:
         {
            bool changed = false;
            if (action == RGUI_ACTION_START)
            {
#ifdef HAVE_SINC
               snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "sinc");
#else
               snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "hermite");
#endif
               changed = true;
            }
            else if (action == RGUI_ACTION_LEFT || action == RGUI_ACTION_RIGHT)
            {
#ifdef HAVE_SINC
               if( strstr(g_settings.audio.resampler, "hermite"))
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "sinc");
               else
#endif
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "hermite");
               changed = true;
            }

            if (g_extern.main_is_init && changed)
            {
               if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                        g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
               {
                  RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                  g_extern.audio_active = false;
               }
            }
         }
         break;
      case RGUI_SETTINGS_SRAM_DIR:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
         break;
      case RGUI_SETTINGS_STATE_DIR:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
         break;
#ifndef HAVE_DYNAMIC
      case RGUI_SETTINGS_RESTART_EMULATOR:
         if (action == RGUI_ACTION_OK)
         {
#ifdef GEKKO
            snprintf(g_extern.fullpath, sizeof(g_extern.fullpath), "%s/boot.dol", default_paths.core_dir);
#endif
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            return -1;
         }
         break;
#endif
      case RGUI_SETTINGS_QUIT_EMULATOR:
         if (action == RGUI_ACTION_OK)
         {
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            return -1;
         }
         break;
      // controllers
#ifdef GEKKO
      case RGUI_SETTINGS_BIND_DEVICE:
         g_settings.input.device[port] += RARCH_DEVICE_LAST;
         if (action == RGUI_ACTION_START)
            g_settings.input.device[port] = 0;
         else if (action == RGUI_ACTION_LEFT)
            g_settings.input.device[port]--;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.input.device[port]++;
         g_settings.input.device[port] %= RARCH_DEVICE_LAST;
         driver.input->set_default_keybind_lut(g_settings.input.device[port], port);
         rarch_input_set_default_keybinds(port);
         driver.input->set_analog_dpad_mapping(g_settings.input.device[port], g_settings.input.dpad_emulation[port], port);
         break;
#endif
#ifdef RARCH_CONSOLE
      case RGUI_SETTINGS_BIND_DPAD_EMULATION:
         g_settings.input.dpad_emulation[port] += DPAD_EMULATION_LAST;
         if (action == RGUI_ACTION_START)
            g_settings.input.dpad_emulation[port] = DPAD_EMULATION_LSTICK;
         else if (action == RGUI_ACTION_LEFT)
            g_settings.input.dpad_emulation[port]--;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.input.dpad_emulation[port]++;
         g_settings.input.dpad_emulation[port] %= DPAD_EMULATION_LAST;
         driver.input->set_analog_dpad_mapping(g_settings.input.device[port], g_settings.input.dpad_emulation[port], port);
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
         unsigned keybind_action;

         if (action == RGUI_ACTION_START)
            keybind_action = KEYBIND_DEFAULT;
         else if (action == RGUI_ACTION_LEFT)
            keybind_action = KEYBIND_DECREMENT;
         else if (action == RGUI_ACTION_RIGHT)
            keybind_action = KEYBIND_INCREMENT;
         else
            break;

         rarch_input_set_keybind(port, keybind_action, rgui_controller_lut[setting - RGUI_SETTINGS_BIND_UP]);
      }
#endif
      default:
         break;
   }

   return 0;
}

static void rgui_settings_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->folder_buf);

   rgui_list_push(rgui->folder_buf, "Rewind", RGUI_SETTINGS_REWIND_ENABLE, 0);
   rgui_list_push(rgui->folder_buf, "Rewind granularity", RGUI_SETTINGS_REWIND_GRANULARITY, 0);
   if (g_extern.main_is_init)
   {
      rgui_list_push(rgui->folder_buf, "Save State", RGUI_SETTINGS_SAVESTATE_SAVE, 0);
      rgui_list_push(rgui->folder_buf, "Load State", RGUI_SETTINGS_SAVESTATE_LOAD, 0);
#ifdef HAVE_SCREENSHOTS
      rgui_list_push(rgui->folder_buf, "Take Screenshot", RGUI_SETTINGS_SCREENSHOT, 0);
#endif
      rgui_list_push(rgui->folder_buf, "Restart Game", RGUI_SETTINGS_RESTART_GAME, 0);
   }
   rgui_list_push(rgui->folder_buf, "Hardware filtering", RGUI_SETTINGS_VIDEO_FILTER, 0);
#ifdef HW_RVL
   rgui_list_push(rgui->folder_buf, "VI Trap filtering", RGUI_SETTINGS_VIDEO_SOFT_FILTER, 0);
#endif
#ifdef GEKKO
   rgui_list_push(rgui->folder_buf, "Screen Resolution", RGUI_SETTINGS_VIDEO_RESOLUTION, 0);
   rgui_list_push(rgui->folder_buf, "Gamma", RGUI_SETTINGS_VIDEO_GAMMA, 0);
#endif
   rgui_list_push(rgui->folder_buf, "Aspect Ratio", RGUI_SETTINGS_VIDEO_ASPECT_RATIO, 0);
   rgui_list_push(rgui->folder_buf, "Custom Ratio", RGUI_SETTINGS_CUSTOM_VIEWPORT, 0);
   rgui_list_push(rgui->folder_buf, "Overscan", RGUI_SETTINGS_VIDEO_OVERSCAN, 0);
   rgui_list_push(rgui->folder_buf, "Rotation", RGUI_SETTINGS_VIDEO_ROTATION, 0);
   rgui_list_push(rgui->folder_buf, "Mute Audio", RGUI_SETTINGS_AUDIO_MUTE, 0);
   rgui_list_push(rgui->folder_buf, "Audio Control Rate", RGUI_SETTINGS_AUDIO_CONTROL_RATE, 0);
   rgui_list_push(rgui->folder_buf, "Audio Resampler", RGUI_SETTINGS_RESAMPLER_TYPE, 0);
#ifdef GEKKO
   rgui_list_push(rgui->folder_buf, "SRAM Saves in \"sram\" Dir", RGUI_SETTINGS_SRAM_DIR, 0);
   rgui_list_push(rgui->folder_buf, "State Saves in \"state\" Dir", RGUI_SETTINGS_STATE_DIR, 0);
#endif
#ifdef HAVE_LIBRETRO_MANAGEMENT
   rgui_list_push(rgui->folder_buf, "Core", RGUI_SETTINGS_CORE, 0);
#endif
#ifdef GEKKO
   rgui_list_push(rgui->folder_buf, "Controller #1 Config", RGUI_SETTINGS_CONTROLLER_1, 0);
   rgui_list_push(rgui->folder_buf, "Controller #2 Config", RGUI_SETTINGS_CONTROLLER_2, 0);
   rgui_list_push(rgui->folder_buf, "Controller #3 Config", RGUI_SETTINGS_CONTROLLER_3, 0);
   rgui_list_push(rgui->folder_buf, "Controller #4 Config", RGUI_SETTINGS_CONTROLLER_4, 0);
#endif
   rgui_list_push(rgui->folder_buf, "Debug Text", RGUI_SETTINGS_DEBUG_TEXT, 0);
#ifndef HAVE_DYNAMIC
   rgui_list_push(rgui->folder_buf, "Restart RetroArch", RGUI_SETTINGS_RESTART_EMULATOR, 0);
#endif
   rgui_list_push(rgui->folder_buf, "Exit RetroArch", RGUI_SETTINGS_QUIT_EMULATOR, 0);
}

static void rgui_settings_controller_populate_entries(rgui_handle_t *rgui)
{
#ifdef RARCH_CONSOLE
   rgui_list_clear(rgui->folder_buf);

   rgui_list_push(rgui->folder_buf, "Device", RGUI_SETTINGS_BIND_DEVICE, 0);
   rgui_list_push(rgui->folder_buf, "DPad Emulation", RGUI_SETTINGS_BIND_DPAD_EMULATION, 0);
   rgui_list_push(rgui->folder_buf, "Up", RGUI_SETTINGS_BIND_UP, 0);
   rgui_list_push(rgui->folder_buf, "Down", RGUI_SETTINGS_BIND_DOWN, 0);
   rgui_list_push(rgui->folder_buf, "Left", RGUI_SETTINGS_BIND_LEFT, 0);
   rgui_list_push(rgui->folder_buf, "Right", RGUI_SETTINGS_BIND_RIGHT, 0);
   rgui_list_push(rgui->folder_buf, "A", RGUI_SETTINGS_BIND_A, 0);
   rgui_list_push(rgui->folder_buf, "B", RGUI_SETTINGS_BIND_B, 0);
   rgui_list_push(rgui->folder_buf, "X", RGUI_SETTINGS_BIND_X, 0);
   rgui_list_push(rgui->folder_buf, "Y", RGUI_SETTINGS_BIND_Y, 0);
   rgui_list_push(rgui->folder_buf, "Start", RGUI_SETTINGS_BIND_START, 0);
   rgui_list_push(rgui->folder_buf, "Select", RGUI_SETTINGS_BIND_SELECT, 0);
   rgui_list_push(rgui->folder_buf, "L", RGUI_SETTINGS_BIND_L, 0);
   rgui_list_push(rgui->folder_buf, "R", RGUI_SETTINGS_BIND_R, 0);
   rgui_list_push(rgui->folder_buf, "L2", RGUI_SETTINGS_BIND_L2, 0);
   rgui_list_push(rgui->folder_buf, "R2", RGUI_SETTINGS_BIND_R2, 0);
   rgui_list_push(rgui->folder_buf, "L3", RGUI_SETTINGS_BIND_L3, 0);
   rgui_list_push(rgui->folder_buf, "R3", RGUI_SETTINGS_BIND_R3, 0);
#endif
}

static int rgui_viewport_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rarch_viewport_t vp;
   driver.video->viewport_info(driver.video_data, &vp);
   unsigned win_width = vp.full_width;
   unsigned win_height = vp.full_height;
   unsigned menu_type = 0;
   rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);

   (void)win_width;
   (void)win_height;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.y -= 1;
            g_extern.console.screen.viewports.custom_vp.height += 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.height -= 1;
         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_DOWN:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.y += 1;
            g_extern.console.screen.viewports.custom_vp.height -= 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.height += 1;
         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_LEFT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.x -= 1;
            g_extern.console.screen.viewports.custom_vp.width += 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.width -= 1;
         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_RIGHT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.x += 1;
            g_extern.console.screen.viewports.custom_vp.width -= 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.width += 1;
         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_CANCEL:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
         {
            rgui_list_pop(rgui->path_stack);
            rgui_list_push(rgui->path_stack, "", RGUI_SETTINGS_CUSTOM_VIEWPORT, 0);
         }
         else
         {
            rgui_list_pop(rgui->path_stack);
         }
         break;

      case RGUI_ACTION_OK:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            rgui_list_pop(rgui->path_stack);
            rgui_list_push(rgui->path_stack, "", RGUI_SETTINGS_CUSTOM_VIEWPORT_2, 0);
         }
         else
         {
            rgui_list_pop(rgui->path_stack);
         }
         break;

      case RGUI_ACTION_START:
#ifdef GEKKO
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.width += g_extern.console.screen.viewports.custom_vp.x;
            g_extern.console.screen.viewports.custom_vp.height += g_extern.console.screen.viewports.custom_vp.y;
            g_extern.console.screen.viewports.custom_vp.x = 0;
            g_extern.console.screen.viewports.custom_vp.y = 0;
         }
         else
         {
            g_extern.console.screen.viewports.custom_vp.width = win_width - g_extern.console.screen.viewports.custom_vp.x;
            g_extern.console.screen.viewports.custom_vp.height = win_height - g_extern.console.screen.viewports.custom_vp.y;
         }
#endif
         if (driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;

      case RGUI_ACTION_SETTINGS:
         rgui_list_pop(rgui->path_stack);
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);

   render_text(rgui);

   if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
      render_messagebox(rgui, "Set Upper-Left Corner");
   else if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2)
      render_messagebox(rgui, "Set Bottom-Right Corner");

   return 0;
}

static int rgui_settings_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rgui->frame_buf_pitch = RGUI_WIDTH * 2;
   unsigned type = 0;
   const char *label = 0;
   if (action != RGUI_ACTION_REFRESH)
      rgui_list_at(rgui->folder_buf, rgui->directory_ptr, &label, &type, NULL);
#ifdef HAVE_LIBRETRO_MANAGEMENT
   if (type == RGUI_SETTINGS_CORE)
      label = default_paths.core_dir;
#endif
   const char *dir = 0;
   unsigned menu_type = 0;
   size_t directory_ptr = 0;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, &directory_ptr);

   if (rgui->need_refresh)
      action = RGUI_ACTION_NOOP;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->directory_ptr > 0)
            rgui->directory_ptr--;
         else
            rgui->directory_ptr = rgui_list_size(rgui->folder_buf) - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->directory_ptr + 1 < rgui_list_size(rgui->folder_buf))
            rgui->directory_ptr++;
         else
            rgui->directory_ptr = 0;
         break;

      case RGUI_ACTION_CANCEL:
      case RGUI_ACTION_SETTINGS:
         if (rgui_list_size(rgui->path_stack) > 1)
         {
            rgui_list_pop(rgui->path_stack);
            rgui->directory_ptr = directory_ptr;
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if ((rgui_is_controller_menu(type)
#ifdef HAVE_LIBRETRO_MANAGEMENT
                  || type == RGUI_SETTINGS_CORE
#endif
            )
               && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, label, type, rgui->directory_ptr);
            rgui->directory_ptr = 0;
            rgui->need_refresh = true;
         }
         else if (type == RGUI_SETTINGS_CUSTOM_VIEWPORT && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, "", type, rgui->directory_ptr);
            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

            if (driver.video_poke->set_aspect_ratio)
               driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
         }
         else if (type == RGUI_SETTINGS_OPEN_FILEBROWSER && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, rgui->base_path, RGUI_FILE_DIRECTORY, rgui->directory_ptr);
            rgui->need_refresh = true;
         }
         else
         {
            int ret = rgui_settings_toggle_setting(type, action, menu_type);

            if (ret != 0)
               return ret;
         }
         break;

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   rgui_list_back(rgui->path_stack, &dir, &menu_type, &directory_ptr);

   if (rgui->need_refresh && !(menu_type == RGUI_FILE_DIRECTORY || menu_type == RGUI_FILE_DEVICE
#ifdef HAVE_LIBRETRO_MANAGEMENT
            || menu_type == RGUI_SETTINGS_CORE
#endif
            ))
   {
      rgui->need_refresh = false;
      if (rgui_is_controller_menu(menu_type))
         rgui_settings_controller_populate_entries(rgui);
      else
         rgui_settings_populate_entries(rgui);
   }

   render_text(rgui);

   return 0;
}

int rgui_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   const char *dir = 0;
   unsigned menu_type = 0;
   size_t directory_ptr = 0;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, &directory_ptr);
   int ret = 0;

   if (menu_type == RGUI_SETTINGS || rgui_is_controller_menu(menu_type))
      return rgui_settings_iterate(rgui, action);
   else if (rgui_is_viewport_menu(menu_type))
      return rgui_viewport_iterate(rgui, action);
   if (rgui->need_refresh && action != RGUI_ACTION_MESSAGE)
      action = RGUI_ACTION_NOOP;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->directory_ptr > 0)
            rgui->directory_ptr--;
         else
            rgui->directory_ptr = rgui_list_size(rgui->folder_buf) - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->directory_ptr + 1 < rgui_list_size(rgui->folder_buf))
            rgui->directory_ptr++;
         else
            rgui->directory_ptr = 0;
         break;

      case RGUI_ACTION_LEFT:
         if (rgui->directory_ptr - 8 > 0)
            rgui->directory_ptr -= 8;
         else
            rgui->directory_ptr = 0;
         break;

      case RGUI_ACTION_RIGHT:
         if (rgui->directory_ptr + 8 < rgui_list_size(rgui->folder_buf))
            rgui->directory_ptr += 8;
         else
            rgui->directory_ptr = rgui_list_size(rgui->folder_buf) - 1;
         break;
      
      case RGUI_ACTION_CANCEL:
         if (rgui_list_size(rgui->path_stack) > 1)
         {
            rgui->need_refresh = true;
            rgui->directory_ptr = directory_ptr;
            rgui_list_pop(rgui->path_stack);
         }
         break;

      case RGUI_ACTION_OK:
      {
         if (rgui_list_size(rgui->folder_buf) == 0)
            return 0;

         const char *path = 0;
         unsigned type = 0;
         rgui_list_at(rgui->folder_buf, rgui->directory_ptr, &path, &type, NULL);

         if (type == RGUI_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            snprintf(cat_path, sizeof(cat_path), "%s/%s", dir, path);

            if (strcmp(path, "..") == 0)
            {
               rgui->directory_ptr = directory_ptr;
               rgui_list_pop(rgui->path_stack);
            }
            else if (strcmp(path, ".") != 0)
            {
               rgui_list_push(rgui->path_stack, cat_path, RGUI_FILE_DIRECTORY, rgui->directory_ptr);
               rgui->directory_ptr = 0;
            }
            rgui->need_refresh = true;
         }
         else if (type == RGUI_FILE_DEVICE)
         {
            rgui_list_push(rgui->path_stack, path, RGUI_FILE_DEVICE, rgui->directory_ptr);
            rgui->directory_ptr = 0;
            rgui->need_refresh = true;
         }
         else
         {
#ifdef HAVE_LIBRETRO_MANAGEMENT
            if (menu_type == RGUI_SETTINGS_CORE)
            {
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               rgui->directory_ptr = directory_ptr;
               rgui->need_refresh = true;
               rgui_list_pop(rgui->path_stack);

#ifdef GEKKO
               snprintf(g_extern.fullpath, sizeof(g_extern.fullpath), "%s/boot.dol", default_paths.core_dir);
#endif
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
               g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
               g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
            }
            else
#endif
            {
               snprintf(rgui->path_buf, sizeof(rgui->path_buf), "%s/%s", dir, path);

               strlcpy(g_extern.fullpath, rgui->path_buf, sizeof(g_extern.fullpath));
               g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);

               menu_settings_msg(S_MSG_LOADING_ROM, 1);
               rgui->need_refresh = true; // in case of zip extract
               rgui->msg_force = true;
            }

            ret = -1;
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_SETTINGS:
#ifdef HAVE_LIBRETRO_MANAGEMENT
         if (menu_type == RGUI_SETTINGS_CORE)
         {
            rgui->directory_ptr = directory_ptr;
            rgui->need_refresh = true;
            rgui_list_pop(rgui->path_stack);
         }
         else
#endif
         {
            rgui_list_push(rgui->path_stack, "", RGUI_SETTINGS, rgui->directory_ptr);
            rgui->directory_ptr = 0;
         }
         return rgui_settings_iterate(rgui, RGUI_ACTION_REFRESH);

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   // refresh values in case the stack changed
   rgui_list_back(rgui->path_stack, &dir, &menu_type, &directory_ptr);

   if (rgui->need_refresh && (menu_type == RGUI_FILE_DIRECTORY || menu_type == RGUI_FILE_DEVICE
#ifdef HAVE_LIBRETRO_MANAGEMENT
            || menu_type == RGUI_SETTINGS_CORE
#endif
            ))
   {
      rgui->need_refresh = false;
      rgui_list_clear(rgui->folder_buf);

      rgui->folder_cb(dir, (rgui_file_enum_cb_t)rgui_list_push,
         &menu_type, rgui->folder_buf);

      if (*dir)
         rgui_list_sort(rgui->folder_buf);
   }

   render_text(rgui);

   return ret;
}

static const struct retro_keybind _menu_nav_binds[] = {
#if defined(HW_RVL)
   { 0, 0, NULL, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP | GX_CLASSIC_UP | GX_CLASSIC_LSTICK_UP | GX_CLASSIC_RSTICK_UP | GX_WIIMOTE_UP | GX_NUNCHUK_UP, 0 },
   { 0, 0, NULL, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN | GX_CLASSIC_DOWN | GX_CLASSIC_LSTICK_DOWN | GX_CLASSIC_RSTICK_DOWN | GX_WIIMOTE_DOWN | GX_NUNCHUK_DOWN, 0 },
   { 0, 0, NULL, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT | GX_CLASSIC_LEFT | GX_CLASSIC_LSTICK_LEFT | GX_CLASSIC_RSTICK_LEFT | GX_WIIMOTE_LEFT | GX_NUNCHUK_LEFT, 0 },
   { 0, 0, NULL, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT | GX_CLASSIC_RIGHT | GX_CLASSIC_LSTICK_RIGHT | GX_CLASSIC_RSTICK_RIGHT | GX_WIIMOTE_RIGHT | GX_NUNCHUK_RIGHT, 0 },
   { 0, 0, NULL, 0, GX_GC_A | GX_CLASSIC_A | GX_WIIMOTE_A | GX_WIIMOTE_2, 0 },
   { 0, 0, NULL, 0, GX_GC_B | GX_CLASSIC_B | GX_WIIMOTE_B | GX_WIIMOTE_1, 0 },
   { 0, 0, NULL, 0, GX_GC_START | GX_CLASSIC_PLUS | GX_WIIMOTE_PLUS, 0 },
   { 0, 0, NULL, 0, GX_GC_Z_TRIGGER | GX_CLASSIC_MINUS | GX_WIIMOTE_MINUS, 0 },
   { 0, 0, NULL, 0, GX_WIIMOTE_HOME | GX_CLASSIC_HOME, 0 },
#elif defined(HW_DOL)
   { 0, 0, NULL, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP, 0 },
   { 0, 0, NULL, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN, 0 },
   { 0, 0, NULL, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT, 0 },
   { 0, 0, NULL, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT, 0 },
   { 0, 0, NULL, 0, GX_GC_A, 0 },
   { 0, 0, NULL, 0, GX_GC_B, 0 },
   { 0, 0, NULL, 0, GX_GC_START, 0 },
   { 0, 0, NULL, 0, GX_GC_Z_TRIGGER, 0 },
   { 0, 0, NULL, 0, GX_WIIMOTE_HOME, 0 },
   { 0, 0, NULL, 0, GX_QUIT_KEY, 0 },
#else
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_UP), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_A), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_B), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_START), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_MENU_TOGGLE), 0 },
   { 0, 0, NULL, (enum retro_key)0, (1ULL << RARCH_QUIT_KEY), 0 },
#endif
};

static const struct retro_keybind *menu_nav_binds[] = {
   _menu_nav_binds
};

enum
{
   GX_DEVICE_NAV_UP = 0,
   GX_DEVICE_NAV_DOWN,
   GX_DEVICE_NAV_LEFT,
   GX_DEVICE_NAV_RIGHT,
   GX_DEVICE_NAV_A,
   GX_DEVICE_NAV_B,
   GX_DEVICE_NAV_START,
   GX_DEVICE_NAV_SELECT,
   GX_DEVICE_NAV_MENU,
   GX_DEVICE_NAV_QUIT,
   RMENU_DEVICE_NAV_LAST
};

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
#ifdef HAVE_LIBRETRO_MANAGEMENT
   bool core_chooser = (userdata) ? *(unsigned*)userdata == RGUI_SETTINGS_CORE : false;
#else
   bool core_chooser = false;
#endif

   if (!*directory)
   {
#if defined(GEKKO)
#ifdef HW_RVL
      file_cb(ctx, "sd:/", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "usb:/", RGUI_FILE_DEVICE, 0);
#endif
      file_cb(ctx, "carda:/", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "cardb:/", RGUI_FILE_DEVICE, 0);
      return true;
#elif defined(_XBOX1)
      file_cb(ctx, "C:\\", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "D:\\", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "E:\\", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "F:\\", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "G:\\", RGUI_FILE_DEVICE, 0);
      return true;
#elif defined(__CELLOS_LV2__)
      file_cb(ctx, "app_home", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "dev_hdd0", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "dev_hdd1", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "host_root", RGUI_FILE_DEVICE, 0);
      return true;
#endif
   }

#if defined(GEKKO) && defined(HW_RVL)
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(directory);

   if (dev != -1 && !gx_devices[dev].mounted && gx_devices[dev].interface->isInserted())
      fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   const char *exts = core_chooser ? EXT_EXECUTABLES : g_extern.system.valid_extensions;
   char dir[PATH_MAX];
   if (*directory)
      strlcpy(dir, directory, sizeof(dir));
   else
      strlcpy(dir, "/", sizeof(dir));

   struct string_list *list = dir_list_new(dir, exts, true);
   if (!list)
      return false;

   for (size_t i = 0; i < list->size; i++)
   {
      bool is_dir = list->elems[i].attr.b;
#ifdef HAVE_LIBRETRO_MANAGEMENT
      if (core_chooser && (is_dir ||
               strcasecmp(list->elems[i].data, default_paths.salamander_file) == 0))
         continue;
#endif

      file_cb(ctx,
            path_basename(list->elems[i].data),
            is_dir ? RGUI_FILE_DIRECTORY : RGUI_FILE_PLAIN, 0);
   }

   string_list_free(list);
   return true;
}

/*============================================================
RMENU API
============================================================ */

void menu_init(void)
{
   rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
         NULL, bitmap_bin, folder_cb, NULL);

   rgui_iterate(rgui, RGUI_ACTION_REFRESH);
}

void menu_free(void)
{
   rgui_free(rgui);
}

static uint16_t trigger_state = 0;

static int menu_input_process(void *data, void *state)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         menu_settings_msg(S_MSG_LOADING_ROM, 100);

      if (g_extern.fullpath)
         g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);

      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      return -1;
   }

   if (!(g_extern.frame_count < g_extern.delay_timer[0]))
   {
      bool return_to_game_enable = ((trigger_state & (1ULL << GX_DEVICE_NAV_MENU)) && g_extern.main_is_init);

      if (return_to_game_enable)
      {
         g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         return -1;
      }
   }

   return 0;
}

bool menu_iterate(void)
{
   static uint16_t old_input_state = 0;
   static bool initial_held = true;
   static bool first_held = false;
   bool do_held;
   int input_entry_ret, input_process_ret;
   rgui_action_t action;
   uint16_t input_state;

   g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
   if (driver.video_poke->apply_state_changes)
      driver.video_poke->apply_state_changes(driver.video_data);

   g_extern.frame_count++;

   input_state = 0;

   driver.input->poll(NULL);

#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      driver.overlay_state = 0;

      unsigned device = input_overlay_full_screen(driver.overlay) ?
         RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

      bool polled = false;
      for (unsigned i = 0;
            input_input_state_func(NULL, 0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
            i++)
      {
         int16_t x = input_input_state_func(NULL, 0,
               device, i, RETRO_DEVICE_ID_POINTER_X);
         int16_t y = input_input_state_func(NULL, 0,
               device, i, RETRO_DEVICE_ID_POINTER_Y);

         driver.overlay_state |= input_overlay_poll(driver.overlay, x, y);
         polled = true;
      }

      if (!polled)
         input_overlay_poll_clear(driver.overlay);
   }
#endif

#ifndef GEKKO
   /* TODO - not sure if correct regarding RARCH_QUIT_KEY */
   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      goto deinit;
   }
#endif

   for (unsigned i = 0; i < RMENU_DEVICE_NAV_LAST; i++)
      input_state |= driver.input->input_state(NULL, menu_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;

   input_state |= driver.input->key_pressed(driver.input_data, RARCH_MENU_TOGGLE) ? (1ULL << GX_DEVICE_NAV_MENU) : 0;
   input_state |= driver.input->key_pressed(driver.input_data, RARCH_QUIT_KEY) ? (1ULL << GX_DEVICE_NAV_QUIT) : 0;

#ifdef HAVE_OVERLAY
   for (unsigned i = 0; i < RMENU_DEVICE_NAV_LAST; i++)
      input_state |= driver.overlay_state & menu_nav_binds[0][i].joykey ? (1ULL << i) : 0;
#endif

   trigger_state = input_state & ~old_input_state;
   do_held = (input_state & ((1ULL << GX_DEVICE_NAV_UP) | (1ULL << GX_DEVICE_NAV_DOWN) | (1ULL << GX_DEVICE_NAV_LEFT) | (1ULL << GX_DEVICE_NAV_RIGHT))) && !(input_state & ((1ULL << GX_DEVICE_NAV_MENU) | (1ULL << GX_DEVICE_NAV_QUIT)));

   if(do_held)
   {
      if(!first_held)
      {
         first_held = true;
         g_extern.delay_timer[1] = g_extern.frame_count + (initial_held ? 15 : 7);
      }

      if (!(g_extern.frame_count < g_extern.delay_timer[1]))
      {
         first_held = false;
         trigger_state = input_state; //second input frame set as current frame
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
   }

   old_input_state = input_state;
   action = RGUI_ACTION_NOOP;

   // don't run anything first frame, only capture held inputs for old_input_state
   if (trigger_state & (1ULL << GX_DEVICE_NAV_UP))
      action = RGUI_ACTION_UP;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_B))
      action = RGUI_ACTION_CANCEL;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_A))
      action = RGUI_ACTION_OK;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_START))
      action = RGUI_ACTION_START;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_SELECT))
      action = RGUI_ACTION_SETTINGS;
#ifdef GEKKO
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_QUIT))
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
      goto deinit;
   }
#endif

   input_entry_ret = 0;
   input_process_ret = 0;

   input_entry_ret = rgui_iterate(rgui, action);

   // draw last frame for loading messages
   if (driver.video_poke->set_rgui_texture)
      driver.video_poke->set_rgui_texture(driver.video_data, menu_framebuf);

   rarch_render_cached_frame();

   if (driver.video_poke->set_rgui_texture)
      driver.video_poke->set_rgui_texture(driver.video_data, NULL);

   input_process_ret = menu_input_process(NULL, NULL);

   if (input_entry_ret != 0 || input_process_ret != 0)
      goto deinit;

   return true;

deinit:
   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding QUIT in the emulation loop (lasts for 30 frame ticks)
   if (!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer[0] = g_extern.frame_count + 30;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);
   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_INGAME);

   return false;
}
