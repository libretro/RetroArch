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

#include "rgui.h"
#include "list.h"
#include "../rarch_console_video.h"
#include "../../gfx/fonts/bitmap.h"
#include "../../screenshot.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((RGUI_WIDTH - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((RGUI_HEIGHT - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

#ifdef HAVE_HDD_CACHE_PARTITION
#define LAST_ZIP_EXTRACT ZIP_EXTRACT_TO_CACHE_DIR
#else
#define LAST_ZIP_EXTRACT ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE
#endif

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
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_640_256,
   GX_RESOLUTIONS_640_288,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_DEFAULT,
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
   { 640, 224 },
   { 640, 240 },
   { 640, 256 },
   { 640, 288 },
   { 640, 448 },
   { 640, 480 },
   { 0, 0 }
};

unsigned rgui_current_gx_resolution = GX_RESOLUTIONS_DEFAULT;
#endif

unsigned RGUI_WIDTH = 320;
unsigned RGUI_HEIGHT = 240;

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

   const uint8_t *font;
   bool alloc_font;
};

static const char *rgui_device_labels[] = {
   "GameCube Controller",
   "Wiimote",
   "Wiimote + Nunchuk",
   "Classic Controller",
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

static inline bool rgui_is_controller_menu(rgui_file_type_t menu_type)
{
   return (menu_type >= RGUI_SETTINGS_CONTROLLER_1 && menu_type <= RGUI_SETTINGS_CONTROLLER_4);
}

static inline bool rgui_is_viewport_menu(rgui_file_type_t menu_type)
{
   return (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2);
}

static void copy_glyph(uint8_t *glyph, const uint8_t *buf)
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
      copy_glyph(&font[FONT_OFFSET(i)],
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
      rarch_settings_change(S_QUIT);
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
   return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = ((x + y) & 1) + 1;
   return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
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
               (3 << 0) | (10 << 4) | (3 << 8) | (7 << 12) : 0x7FFF;
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
   if (rgui->need_refresh && g_extern.console.rmenu.mode == MODE_MENU && !rgui->msg_force)
      return;

   size_t begin = rgui->directory_ptr >= TERM_HEIGHT / 2 ?
      rgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->directory_ptr + TERM_HEIGHT <= rgui_list_size(rgui->folder_buf) ?
      rgui->directory_ptr + TERM_HEIGHT : rgui_list_size(rgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_background(rgui);

   char title[TERM_WIDTH];
   const char *dir = 0;
   rgui_file_type_t menu_type = 0;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, NULL);

   if (menu_type == RGUI_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION");
   else if (rgui_is_controller_menu(menu_type) || rgui_is_viewport_menu(menu_type) || menu_type == RGUI_SETTINGS)
      snprintf(title, sizeof(title), "SETTINGS: %s", dir);
   else
      snprintf(title, sizeof(title), "FILE BROWSER: %s", dir);

   blit_line(rgui, TERM_START_X + 15, 15, title, true);

   struct retro_system_info info;
   retro_get_system_info(&info);
   snprintf(title, sizeof(title), "CORE: %s %s", info.library_name, info.library_version);
   blit_line(rgui, TERM_START_X + 15, (TERM_HEIGHT * FONT_HEIGHT_STRIDE) + TERM_START_Y + 2, title, true);

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path = 0;
      rgui_file_type_t type = 0;
      rgui_list_at(rgui->folder_buf, i, &path, &type, NULL);
      char message[TERM_WIDTH + 1];
      char type_str[TERM_WIDTH + 1];
      int w = rgui_is_controller_menu(menu_type) ? 26 : 19;
      unsigned port = menu_type - RGUI_SETTINGS_CONTROLLER_1;
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
         case RGUI_SETTINGS_SAVESTATE_SAVE:
         case RGUI_SETTINGS_SAVESTATE_LOAD:
            snprintf(type_str, sizeof(type_str), "%d", g_extern.state_slot);
            break;
         case RGUI_SETTINGS_VIDEO_FILTER:
            snprintf(type_str, sizeof(type_str), g_settings.video.smooth ? "Bilinear filtering" : "Point filtering");
            break;
#ifdef HW_RVL
         case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
            snprintf(type_str, sizeof(type_str), g_extern.console.screen.state.soft_filter.enable ? "ON" : "OFF");
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
               rarch_settings_create_menu_item_label(rotate_msg, S_LBL_ROTATION, sizeof(rotate_msg));
               snprintf(type_str, sizeof(type_str), rotate_msg);
            }
            break;
         case RGUI_SETTINGS_AUDIO_MUTE:
            snprintf(type_str, sizeof(type_str), g_extern.audio_data.mute ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_AUDIO_CONTROL_RATE:
            snprintf(type_str, sizeof(type_str), "%.3f", g_settings.audio.rate_control_delta);
            break;
         case RGUI_SETTINGS_ZIP_EXTRACT:
            switch(g_extern.file_state.zip_extract_mode)
            {
               case ZIP_EXTRACT_TO_CURRENT_DIR:
                  snprintf(type_str, sizeof(type_str), "Current");
                  break;
               case ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE:
                  snprintf(type_str, sizeof(type_str), "Current + Load");
                  break;
               case ZIP_EXTRACT_TO_CACHE_DIR:
                  snprintf(type_str, sizeof(type_str), "Cache");
                  break;
            }
            break;
         case RGUI_SETTINGS_SRAM_DIR:
            snprintf(type_str, sizeof(type_str), g_extern.console.main_wrap.state.default_sram_dir.enable ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_STATE_DIR:
            snprintf(type_str, sizeof(type_str), g_extern.console.main_wrap.state.default_savestate_dir.enable ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_DEBUG_TEXT:
            snprintf(type_str, sizeof(type_str), g_extern.console.rmenu.state.msg_fps.enable ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_CUSTOM_VIEWPORT:
         case RGUI_SETTINGS_CORE:
         case RGUI_SETTINGS_CONTROLLER_1:
         case RGUI_SETTINGS_CONTROLLER_2:
         case RGUI_SETTINGS_CONTROLLER_3:
         case RGUI_SETTINGS_CONTROLLER_4:
            snprintf(type_str, sizeof(type_str), "...");
            break;
         case RGUI_SETTINGS_BIND_DEVICE:
            snprintf(type_str, sizeof(type_str), "%s", rgui_device_labels[g_settings.input.device[port]]);
            break;
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
         default:
            type_str[0] = 0;
            w = 0;
            break;
      }

      const char *entry_title;
      char tmp[TERM_WIDTH];
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

   const char *message_queue;
#ifdef GEKKO
   gx_video_t *gx = (gx_video_t*)driver.video_data;
   if (rgui->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      rgui->msg_force = false;
   }
   else
   {
      message_queue = gx->msg;
   }
#else
   message_queue = msg_queue_pull(g_extern.msg_queue);
#endif
   render_messagebox(rgui, message_queue);
}

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

static void rgui_settings_toggle_setting(rgui_file_type_t setting, rgui_action_t action, rgui_file_type_t menu_type)
{
   unsigned port = menu_type - RGUI_SETTINGS_CONTROLLER_1;

   switch (setting)
   {
      case RGUI_SETTINGS_SAVESTATE_SAVE:
      case RGUI_SETTINGS_SAVESTATE_LOAD:
         if (action == RGUI_ACTION_OK)
         {
            if (setting == RGUI_SETTINGS_SAVESTATE_SAVE)
               rarch_save_state();
            else
               rarch_load_state();
            rarch_settings_change(S_RETURN_TO_GAME);
         }
         else if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_SAVE_STATE);
         else if (action == RGUI_ACTION_LEFT)
            rarch_settings_change(S_SAVESTATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            rarch_settings_change(S_SAVESTATE_INCREMENT);
         break;
      case RGUI_SETTINGS_SCREENSHOT:
         if (action == RGUI_ACTION_OK)
         {
            const uint16_t *data = (const uint16_t*)g_extern.frame_cache.data;
            unsigned width       = g_extern.frame_cache.width;
            unsigned height      = g_extern.frame_cache.height;
            int pitch            = g_extern.frame_cache.pitch;

            // Negative pitch is needed as screenshot takes bottom-up,
            // but we use top-down.
            bool r = screenshot_dump(default_paths.port_dir,
                  data + (height - 1) * (pitch >> 1), 
                  width, height, -pitch, false);

            msg_queue_push(g_extern.msg_queue, r ? "Screenshot saved" : "Screenshot failed to save", 1, S_DELAY_90);
         }
         break;
      case RGUI_SETTINGS_RESTART_GAME:
         if (action == RGUI_ACTION_OK)
         {
            rarch_settings_change(S_RETURN_TO_GAME);
            rarch_game_reset();
         }
         break;
      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_HW_TEXTURE_FILTER);
         else
            rarch_settings_change(S_HW_TEXTURE_FILTER);
         break;
#ifdef HW_RVL
      case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
         {
            g_extern.console.screen.state.soft_filter.enable = !g_extern.console.screen.state.soft_filter.enable;
            driver.video->apply_state_changes();
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
            driver.video->apply_state_changes();
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            if(g_extern.console.screen.gamma_correction > 0)
            {
               g_extern.console.screen.gamma_correction--;
               driver.video->apply_state_changes();
            }
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            if(g_extern.console.screen.gamma_correction < MAX_GAMMA_SETTING)
            {
               g_extern.console.screen.gamma_correction++;
               driver.video->apply_state_changes();
            }
         }
         break;
      case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
         if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_ASPECT_RATIO);
         else if (action == RGUI_ACTION_LEFT)
            rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
         video_set_aspect_ratio_func(g_settings.video.aspect_ratio_idx);
         break;
      case RGUI_SETTINGS_VIDEO_ROTATION:
         if (action == RGUI_ACTION_START)
         {
            rarch_settings_default(S_DEF_AUDIO_CONTROL_RATE);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            rarch_settings_change(S_ROTATION_DECREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            rarch_settings_change(S_ROTATION_INCREMENT);
            video_set_rotation_func(g_extern.console.screen.orientation);
         }
         break;
      case RGUI_SETTINGS_VIDEO_OVERSCAN:
         if (action == RGUI_ACTION_START)
         {
            rarch_settings_default(S_DEF_OVERSCAN);
            driver.video->apply_state_changes();
         }
         else if (action == RGUI_ACTION_LEFT)
         {
            rarch_settings_change(S_OVERSCAN_DECREMENT);
            driver.video->apply_state_changes();
         }
         else if (action == RGUI_ACTION_RIGHT)
         {
            rarch_settings_change(S_OVERSCAN_INCREMENT);
            driver.video->apply_state_changes();
         }
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
         if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_AUDIO_MUTE);
         else
            rarch_settings_change(S_AUDIO_MUTE);
         break;
      case RGUI_SETTINGS_AUDIO_CONTROL_RATE:
         if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_AUDIO_CONTROL_RATE);
         else if (action == RGUI_ACTION_LEFT)
            rarch_settings_change(S_AUDIO_CONTROL_RATE_DECREMENT);
         else if (action == RGUI_ACTION_RIGHT)
            rarch_settings_change(S_AUDIO_CONTROL_RATE_INCREMENT);
         break;
      case RGUI_SETTINGS_ZIP_EXTRACT:
         if (action == RGUI_ACTION_START)
            g_extern.file_state.zip_extract_mode = ZIP_EXTRACT_TO_CURRENT_DIR;
         else if (action == RGUI_ACTION_LEFT && g_extern.file_state.zip_extract_mode > 0)
            g_extern.file_state.zip_extract_mode--;
         else if (action == RGUI_ACTION_RIGHT && g_extern.file_state.zip_extract_mode < LAST_ZIP_EXTRACT)
            g_extern.file_state.zip_extract_mode++;
         break;
      case RGUI_SETTINGS_SRAM_DIR:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.console.main_wrap.state.default_sram_dir.enable = false;
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.console.main_wrap.state.default_sram_dir.enable = true;
         break;
      case RGUI_SETTINGS_STATE_DIR:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.console.main_wrap.state.default_savestate_dir.enable = false;
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.console.main_wrap.state.default_savestate_dir.enable = true;
         break;
      case RGUI_SETTINGS_DEBUG_TEXT:
         if (action == RGUI_ACTION_START || action == RGUI_ACTION_LEFT)
            g_extern.console.rmenu.state.msg_fps.enable = false;
         else if (action == RGUI_ACTION_RIGHT)
            g_extern.console.rmenu.state.msg_fps.enable = true;
         break;
      case RGUI_SETTINGS_RESTART_EMULATOR:
         if (action == RGUI_ACTION_OK)
         {
#ifdef GEKKO
            snprintf(g_extern.console.external_launch.launch_app, sizeof(g_extern.console.external_launch.launch_app), "%s/boot.dol", default_paths.core_dir);
#endif
            rarch_settings_change(S_RETURN_TO_LAUNCHER);
         }
         break;
      case RGUI_SETTINGS_QUIT_EMULATOR:
         if (action == RGUI_ACTION_OK)
            rarch_settings_change(S_QUIT);
         break;
      // controllers
      case RGUI_SETTINGS_BIND_DEVICE:
         g_settings.input.device[port] += RARCH_DEVICE_LAST;
         if (action == RGUI_ACTION_START)
            g_settings.input.device[port] = 0;
         else if (action == RGUI_ACTION_LEFT)
            g_settings.input.device[port]--;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.input.device[port]++;
         g_settings.input.device[port] %= RARCH_DEVICE_LAST;
         input_gx.set_default_keybind_lut(g_settings.input.device[port], port);
         rarch_input_set_default_keybinds(port);
         input_gx.set_analog_dpad_mapping(g_settings.input.device[port], g_settings.input.dpad_emulation[port], port);
         break;
      case RGUI_SETTINGS_BIND_DPAD_EMULATION:
         g_settings.input.dpad_emulation[port] += DPAD_EMULATION_LAST;
         if (action == RGUI_ACTION_START)
            g_settings.input.dpad_emulation[port] = DPAD_EMULATION_LSTICK;
         else if (action == RGUI_ACTION_LEFT)
            g_settings.input.dpad_emulation[port]--;
         else if (action == RGUI_ACTION_RIGHT)
            g_settings.input.dpad_emulation[port]++;
         g_settings.input.dpad_emulation[port] %= DPAD_EMULATION_LAST;
         input_gx.set_analog_dpad_mapping(g_settings.input.device[port], g_settings.input.dpad_emulation[port], port);
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
      default:
         break;
   }
}

#define RGUI_MENU_ITEM(x, y) rgui_list_push(rgui->folder_buf, x, y, 0)

static void rgui_settings_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->folder_buf);

   if (g_extern.main_is_init)
   {
      RGUI_MENU_ITEM("Save State", RGUI_SETTINGS_SAVESTATE_SAVE);
      RGUI_MENU_ITEM("Load State", RGUI_SETTINGS_SAVESTATE_LOAD);
      RGUI_MENU_ITEM("Take Screenshot", RGUI_SETTINGS_SCREENSHOT);
      RGUI_MENU_ITEM("Restart Game", RGUI_SETTINGS_RESTART_GAME);
   }
   RGUI_MENU_ITEM("Hardware filtering", RGUI_SETTINGS_VIDEO_FILTER);
#ifdef HW_RVL
   RGUI_MENU_ITEM("VI Trap filtering", RGUI_SETTINGS_VIDEO_SOFT_FILTER);
#endif
#ifdef GEKKO
   RGUI_MENU_ITEM("Screen Resolution", RGUI_SETTINGS_VIDEO_RESOLUTION);
#endif
   RGUI_MENU_ITEM("Gamma", RGUI_SETTINGS_VIDEO_GAMMA);
   RGUI_MENU_ITEM("Aspect Ratio", RGUI_SETTINGS_VIDEO_ASPECT_RATIO);
   RGUI_MENU_ITEM("Custom Ratio", RGUI_SETTINGS_CUSTOM_VIEWPORT);
   RGUI_MENU_ITEM("Overscan", RGUI_SETTINGS_VIDEO_OVERSCAN);
   RGUI_MENU_ITEM("Rotation", RGUI_SETTINGS_VIDEO_ROTATION);
   RGUI_MENU_ITEM("Mute Audio", RGUI_SETTINGS_AUDIO_MUTE);
   RGUI_MENU_ITEM("Audio Control Rate", RGUI_SETTINGS_AUDIO_CONTROL_RATE);
   RGUI_MENU_ITEM("Zip Extract Directory", RGUI_SETTINGS_ZIP_EXTRACT);
   RGUI_MENU_ITEM("SRAM Saves in \"sram\" Dir", RGUI_SETTINGS_SRAM_DIR);
   RGUI_MENU_ITEM("State Saves in \"state\" Dir", RGUI_SETTINGS_STATE_DIR);
   RGUI_MENU_ITEM("Core", RGUI_SETTINGS_CORE);
   RGUI_MENU_ITEM("Controller #1 Config", RGUI_SETTINGS_CONTROLLER_1);
   RGUI_MENU_ITEM("Controller #2 Config", RGUI_SETTINGS_CONTROLLER_2);
   RGUI_MENU_ITEM("Controller #3 Config", RGUI_SETTINGS_CONTROLLER_3);
   RGUI_MENU_ITEM("Controller #4 Config", RGUI_SETTINGS_CONTROLLER_4);
   RGUI_MENU_ITEM("Debug Text", RGUI_SETTINGS_DEBUG_TEXT);
   RGUI_MENU_ITEM("Restart RetroArch", RGUI_SETTINGS_RESTART_EMULATOR);
   RGUI_MENU_ITEM("Exit RetroArch", RGUI_SETTINGS_QUIT_EMULATOR);
}

static void rgui_settings_controller_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->folder_buf);

   RGUI_MENU_ITEM("Device", RGUI_SETTINGS_BIND_DEVICE);
   RGUI_MENU_ITEM("DPad Emulation", RGUI_SETTINGS_BIND_DPAD_EMULATION);
   RGUI_MENU_ITEM("Up", RGUI_SETTINGS_BIND_UP);
   RGUI_MENU_ITEM("Down", RGUI_SETTINGS_BIND_DOWN);
   RGUI_MENU_ITEM("Left", RGUI_SETTINGS_BIND_LEFT);
   RGUI_MENU_ITEM("Right", RGUI_SETTINGS_BIND_RIGHT);
   RGUI_MENU_ITEM("A", RGUI_SETTINGS_BIND_A);
   RGUI_MENU_ITEM("B", RGUI_SETTINGS_BIND_B);
   RGUI_MENU_ITEM("X", RGUI_SETTINGS_BIND_X);
   RGUI_MENU_ITEM("Y", RGUI_SETTINGS_BIND_Y);
   RGUI_MENU_ITEM("Start", RGUI_SETTINGS_BIND_START);
   RGUI_MENU_ITEM("Select", RGUI_SETTINGS_BIND_SELECT);
   RGUI_MENU_ITEM("L", RGUI_SETTINGS_BIND_L);
   RGUI_MENU_ITEM("R", RGUI_SETTINGS_BIND_R);
   RGUI_MENU_ITEM("L2", RGUI_SETTINGS_BIND_L2);
   RGUI_MENU_ITEM("R2", RGUI_SETTINGS_BIND_R2);
   RGUI_MENU_ITEM("L3", RGUI_SETTINGS_BIND_L3);
   RGUI_MENU_ITEM("R3", RGUI_SETTINGS_BIND_R3);
}


void rgui_viewport_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
#ifdef GEKKO
   gx_video_t *gx = (gx_video_t*)driver.video_data;
#endif
   rgui_file_type_t menu_type = 0;
   rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);

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
         driver.video->apply_state_changes();
         break;

      case RGUI_ACTION_DOWN:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.y += 1;
            g_extern.console.screen.viewports.custom_vp.height -= 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.height += 1;
         driver.video->apply_state_changes();
         break;

      case RGUI_ACTION_LEFT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.x -= 1;
            g_extern.console.screen.viewports.custom_vp.width += 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.width -= 1;
         driver.video->apply_state_changes();
         break;

      case RGUI_ACTION_RIGHT:
         if (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT)
         {
            g_extern.console.screen.viewports.custom_vp.x += 1;
            g_extern.console.screen.viewports.custom_vp.width -= 1;
         }
         else
            g_extern.console.screen.viewports.custom_vp.width += 1;
         driver.video->apply_state_changes();
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
            g_extern.console.screen.viewports.custom_vp.width = gx->win_width - g_extern.console.screen.viewports.custom_vp.x;
            g_extern.console.screen.viewports.custom_vp.height = gx->win_height - g_extern.console.screen.viewports.custom_vp.y;
         }
#endif
         driver.video->apply_state_changes();
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
}

void rgui_settings_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rgui->frame_buf_pitch = RGUI_WIDTH * 2;
   rgui_file_type_t type = 0;
   const char *label = 0;
   rgui_list_at(rgui->folder_buf, rgui->directory_ptr, &label, &type, NULL);
   if (type == RGUI_SETTINGS_CORE)
      label = default_paths.core_dir;
   const char *dir = 0;
   rgui_file_type_t menu_type = 0;
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
         rgui_list_pop(rgui->path_stack);
         rgui->directory_ptr = directory_ptr;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if ((rgui_is_controller_menu(type) || type == RGUI_SETTINGS_CORE) && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, label, type, rgui->directory_ptr);
            rgui->directory_ptr = 0;
            rgui->need_refresh = true;
         }
         else if (type == RGUI_SETTINGS_CUSTOM_VIEWPORT && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, "", type, rgui->directory_ptr);
            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
            video_set_aspect_ratio_func(g_settings.video.aspect_ratio_idx);
         }
         else
            rgui_settings_toggle_setting(type, action, menu_type);
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

   if (rgui->need_refresh && !(menu_type == RGUI_FILE_DIRECTORY || menu_type == RGUI_FILE_DEVICE || menu_type == RGUI_SETTINGS_CORE))
   {
      rgui->need_refresh = false;
      if (rgui_is_controller_menu(menu_type))
         rgui_settings_controller_populate_entries(rgui);
      else
         rgui_settings_populate_entries(rgui);
   }

   render_text(rgui);

   return;
}

void rgui_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   const char *dir = 0;
   rgui_file_type_t menu_type = 0;
   size_t directory_ptr = 0;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, &directory_ptr);

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
            return;

         const char *path = 0;
         rgui_file_type_t type = 0;
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
            if (menu_type == RGUI_SETTINGS_CORE)
            {
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               rgui->directory_ptr = directory_ptr;
               rgui->need_refresh = true;
               rgui_list_pop(rgui->path_stack);
               msg_queue_push(g_extern.msg_queue, "Change requires restart to take effect", 1, S_DELAY_90);
            }
            else
            {
               snprintf(rgui->path_buf, sizeof(rgui->path_buf), "%s/%s", dir, path);
               rarch_console_load_game_wrap(rgui->path_buf, g_extern.file_state.zip_extract_mode, S_DELAY_1);
               rgui->need_refresh = true; // in case of zip extract
               rgui->msg_force = true;
            }
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_SETTINGS:
         if (menu_type == RGUI_SETTINGS_CORE)
         {
            rgui->directory_ptr = directory_ptr;
            rgui->need_refresh = true;
            rgui_list_pop(rgui->path_stack);
         }
         else
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

   if (rgui->need_refresh && (menu_type == RGUI_FILE_DIRECTORY || menu_type == RGUI_FILE_DEVICE || menu_type == RGUI_SETTINGS_CORE) && g_extern.console.rmenu.mode == MODE_MENU)
   {
      rgui->need_refresh = false;
      rgui_list_clear(rgui->folder_buf);

      rgui->folder_cb(dir, (rgui_file_enum_cb_t)rgui_list_push,
         &menu_type, rgui->folder_buf);

      if (*dir)
         rgui_list_sort(rgui->folder_buf);
   }

   render_text(rgui);
}
