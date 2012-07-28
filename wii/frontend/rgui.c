/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define FONT_WIDTH 5
#define FONT_HEIGHT 10
#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)
#define FONT_HEIGHT_STRIDE (FONT_HEIGHT + 1)

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((RGUI_WIDTH - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((RGUI_HEIGHT - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)))

struct rgui_handle
{
   uint16_t *frame_buf;
   size_t frame_buf_pitch;
   const uint8_t *font_buf;

   rgui_folder_enum_cb_t folder_cb;
   void *userdata;

   rgui_list_t *path_stack;
   rgui_list_t *folder_buf;
   int directory_ptr;
   bool need_refresh;

   char path_buf[PATH_MAX];

   uint16_t font_white[256][FONT_HEIGHT][FONT_WIDTH];
   uint16_t font_green[256][FONT_HEIGHT][FONT_WIDTH];
};

static const char *rgui_device_lables[] = {
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

static void copy_glyph(uint16_t glyph_white[FONT_HEIGHT][FONT_WIDTH],
      uint16_t glyph_green[FONT_HEIGHT][FONT_WIDTH],
      const uint8_t *buf)
{
   for (int y = 0; y < FONT_HEIGHT; y++)
   {
      for (int x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         glyph_white[y][x] = col == 0xff ? 0 : 0x7fff;
         glyph_green[y][x] = col == 0xff ? 0 : (5 << 10) | (20 << 5) | (5 << 0);
      }
   }
}

static void init_font(rgui_handle_t *rgui, const char *path)
{
   for (unsigned i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      copy_glyph(rgui->font_white[i],
            rgui->font_green[i],
            rgui->font_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }
}

rgui_handle_t *rgui_init(const char *base_path,
      uint16_t *buf, size_t buf_pitch,
      const uint8_t *font_buf,
      rgui_folder_enum_cb_t folder_cb, void *userdata)
{
   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   rgui->frame_buf = buf;
   rgui->frame_buf_pitch = buf_pitch;
   rgui->font_buf = font_buf;

   rgui->folder_cb = folder_cb;
   rgui->userdata = userdata;

   rgui->path_stack = rgui_list_new();
   rgui->folder_buf = rgui_list_new();
   rgui_list_push(rgui->path_stack, base_path, RGUI_FILE_DIRECTORY, 0);

   init_font(rgui, "font.bmp");

   return rgui;
}

void rgui_free(rgui_handle_t *rgui)
{
   rgui_list_free(rgui->path_stack);
   rgui_list_free(rgui->folder_buf);
   free(rgui);
}

static uint16_t gray_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   uint16_t col = ((x + y) & 1) + 1;
   col <<= 1;
   return (col << 0) | (col << 5) | (col << 10);
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   uint16_t col = ((x + y) & 1) + 1;
   col <<= 1;
   return (col << 0) | (col << 6) | (col << 10);
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
            uint16_t col = green ? 
               rgui->font_green[(unsigned char)*message][j][i] :
               rgui->font_white[(unsigned char)*message][j][i];

            if (col)
               rgui->frame_buf[(y + j) * (rgui->frame_buf_pitch >> 1) + (x + i)] = col;
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

static void render_text(rgui_handle_t *rgui, size_t begin, size_t end)
{
   render_background(rgui);

   char title[TERM_WIDTH];
   const char *dir;
   rgui_file_type_t menu_type;
   rgui_list_back(rgui->path_stack, &dir, &menu_type, NULL);
   if (!rgui_is_controller_menu(menu_type) && menu_type != RGUI_SETTINGS)
   {
      snprintf(title, sizeof(title), "FILE BROWSER: %s", dir); 
   }
   else
   {
      snprintf(title, sizeof(title), "SETTINGS: %s", dir); 
   }
   blit_line(rgui, TERM_START_X + 15, 15, title, true);

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path;
      rgui_file_type_t type;
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
         case RGUI_SETTINGS_VIDEO_FILTER:
            snprintf(type_str, sizeof(type_str), g_settings.video.smooth ? "Bilinear filtering" : "Point filtering");
            break;
         case RGUI_SETTINGS_AUDIO_MUTE:
            snprintf(type_str, sizeof(type_str), g_extern.audio_data.mute ? "ON" : "OFF");
            break;
         case RGUI_SETTINGS_AUDIO_CONTROL_RATE:
            snprintf(type_str, sizeof(type_str), "%.3f", g_settings.audio.rate_control_delta);
            break;
         case RGUI_SETTINGS_CONTROLLER_1:
         case RGUI_SETTINGS_CONTROLLER_2:
         case RGUI_SETTINGS_CONTROLLER_3:
         case RGUI_SETTINGS_CONTROLLER_4:
            snprintf(type_str, sizeof(type_str), "...");
            break;
         case RGUI_SETTINGS_BIND_DEVICE:
            snprintf(type_str, sizeof(type_str), "%s", rgui_device_lables[g_settings.input.device[port]]);
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
      snprintf(message, sizeof(message), "%c %-*s %-*s\n",
            i == rgui->directory_ptr ? '>' : ' ',
            TERM_WIDTH - (w + 1 + 2),
            path,
            w,
            type_str);

      blit_line(rgui, x, y, message, i == rgui->directory_ptr);
   }
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

static void rgui_settings_toggle_setting(rgui_file_type_t setting, rgui_action_t action, rgui_file_type_t menu_type)
{
   unsigned port = menu_type - RGUI_SETTINGS_CONTROLLER_1;
   switch (setting)
   {
      case RGUI_SETTINGS_VIDEO_FILTER:
         if (action == RGUI_ACTION_START)
            rarch_settings_default(S_DEF_HW_TEXTURE_FILTER);
         else
            rarch_settings_change(S_HW_TEXTURE_FILTER);
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
         input_wii.set_default_keybind_lut(g_settings.input.device[port], port);
         rarch_input_set_default_keybinds(port);
         input_wii.set_analog_dpad_mapping(g_settings.input.device[port], g_settings.input.dpad_emulation[port], port);
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

   RGUI_MENU_ITEM("Hardware filtering", RGUI_SETTINGS_VIDEO_FILTER);
   RGUI_MENU_ITEM("Mute Audio", RGUI_SETTINGS_AUDIO_MUTE);
   RGUI_MENU_ITEM("Audio Control Rate", RGUI_SETTINGS_AUDIO_CONTROL_RATE);
   RGUI_MENU_ITEM("Controller #1 Config", RGUI_SETTINGS_CONTROLLER_1);
   RGUI_MENU_ITEM("Controller #2 Config", RGUI_SETTINGS_CONTROLLER_2);
   RGUI_MENU_ITEM("Controller #3 Config", RGUI_SETTINGS_CONTROLLER_3);
   RGUI_MENU_ITEM("Controller #4 Config", RGUI_SETTINGS_CONTROLLER_4);
}

static void rgui_settings_controller_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->folder_buf);

   RGUI_MENU_ITEM("Device", RGUI_SETTINGS_BIND_DEVICE);
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

static const char *rgui_settings_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rgui_file_type_t type;
   const char *label;
   rgui_list_at(rgui->folder_buf, rgui->directory_ptr, &label, &type, NULL);
   rgui_file_type_t menu_type;
   rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);
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
      {
         size_t directory_ptr;
         rgui_list_back(rgui->path_stack, NULL, NULL, &directory_ptr);
         rgui_list_pop(rgui->path_stack);
         rgui->directory_ptr = directory_ptr;
         rgui->need_refresh = true;
         rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);
         if (menu_type != RGUI_SETTINGS && !rgui_is_controller_menu(menu_type))
            return NULL;
         break;
      }

      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if (rgui_is_controller_menu(type) && action == RGUI_ACTION_OK)
         {
            rgui_list_push(rgui->path_stack, label, type, rgui->directory_ptr);
            rgui->directory_ptr = 0;
            rgui->need_refresh = true;
         }
         else
         {
            rgui_settings_toggle_setting(type, action, menu_type);
         }
         break;

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      default:
         break;
   }

   if (rgui->need_refresh)
   {
      if (rgui_is_controller_menu(menu_type))
         rgui_settings_controller_populate_entries(rgui);
      else
         rgui_settings_populate_entries(rgui);
   }

   size_t begin = rgui->directory_ptr >= TERM_HEIGHT / 2 ?
      rgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->directory_ptr + TERM_HEIGHT <= rgui_list_size(rgui->folder_buf) ?
      rgui->directory_ptr + TERM_HEIGHT : rgui_list_size(rgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_text(rgui, begin, end);

   return NULL;
}

const char *rgui_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   rgui_file_type_t menu_type;
   rgui_list_back(rgui->path_stack, NULL, &menu_type, NULL);

   if (menu_type == RGUI_SETTINGS || rgui_is_controller_menu(menu_type))
   {
      return rgui_settings_iterate(rgui, action);
   }

   bool found = false;
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
            size_t directory_ptr;
            rgui_list_back(rgui->path_stack, NULL, NULL, &directory_ptr);
            rgui_list_pop(rgui->path_stack);
            rgui->directory_ptr = directory_ptr;
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_OK:
      {
         if (rgui_list_size(rgui->folder_buf) == 0)
            return NULL;

         const char *path;
         rgui_file_type_t type;
         rgui_list_at(rgui->folder_buf, rgui->directory_ptr, &path, &type, NULL);

         const char *dir;
         size_t directory_ptr;
         rgui_list_back(rgui->path_stack, &dir, NULL, &directory_ptr);

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
            snprintf(rgui->path_buf, sizeof(rgui->path_buf), "%s/%s", dir, path);
            rarch_console_load_game_wrap(rgui->path_buf, g_console.zip_extract_mode, S_DELAY_1);
            found = true;
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_SETTINGS:
         rgui_list_push(rgui->path_stack, "", RGUI_SETTINGS, rgui->directory_ptr);
         rgui->directory_ptr = 0;
         return rgui_settings_iterate(rgui, RGUI_ACTION_REFRESH);

      default:
         break;
   }

   if (rgui->need_refresh)
   {
      rgui->need_refresh = false;
      rgui_list_clear(rgui->folder_buf);

      const char *path = NULL;
      rgui_list_back(rgui->path_stack, &path, NULL, NULL);

      rgui->folder_cb(path, (rgui_file_enum_cb_t)rgui_list_push,
         rgui->userdata, rgui->folder_buf);

      if (*path)
         rgui_list_sort(rgui->folder_buf);
   }

   size_t begin = rgui->directory_ptr >= TERM_HEIGHT / 2 ?
      rgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->directory_ptr + TERM_HEIGHT <= rgui_list_size(rgui->folder_buf) ?
      rgui->directory_ptr + TERM_HEIGHT : rgui_list_size(rgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_text(rgui, begin, end);

   render_messagebox(rgui, msg_queue_pull(g_extern.msg_queue));

   return found ? rgui->path_buf : NULL;
}

