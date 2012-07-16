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
   size_t directory_ptr;
   bool need_refresh;
   rgui_mode_t mode;

   char path_buf[PATH_MAX];

   uint16_t font_white[256][FONT_HEIGHT][FONT_WIDTH];
   uint16_t font_green[256][FONT_HEIGHT][FONT_WIDTH];
};

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
   if (rgui->mode == RGUI_FILEBROWSER)
   {
      const char *dir;
      rgui_list_back(rgui->path_stack, &dir, NULL, NULL);
      snprintf(title, sizeof(title), "FILE BROWSER: %s", dir); 
   }
   else
   {
      snprintf(title, sizeof(title), "SETTINGS"); 
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
      char *type_str;
      int w;
      switch (type)
      {
         case RGUI_FILE_PLAIN:
            type_str = "(FILE)";
            w = 6;
            break;
         case RGUI_FILE_DIRECTORY:
            type_str = "(DIR)";
            w = 5;
            break;
         case RGUI_FILE_DEVICE:
            type_str = "(DEV)";
            w = 5;
            break;
         case RGUI_SETTINGS_VIDEO_FILTER:
            type_str = g_settings.video.smooth ? "Bilinear filtering" : "Point filtering";
            w = 8;
            break;
         case RGUI_SETTINGS_AUDIO_MUTE:
            type_str = g_extern.audio_data.mute ? "ON" : "OFF";
            w = 8;
            break;
         default:
            type_str = "";
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

static void rgui_settings_toggle_setting(rgui_file_type_t setting)
{
   switch (setting)
   {
      /*case RGUI_SETTINGS_VIDEO_STRETCH:
         
         break;*/

      case RGUI_SETTINGS_VIDEO_FILTER:
	 rarch_settings_change(S_HW_TEXTURE_FILTER);
         break;
      case RGUI_SETTINGS_AUDIO_MUTE:
	 rarch_settings_change(S_AUDIO_MUTE);
         break;
      default:
         break;
   }
}

static void rgui_settings_populate_entries(rgui_handle_t *rgui)
{
   rgui_list_clear(rgui->folder_buf);
   
   rgui_list_push(rgui->folder_buf, "Hardware filtering", RGUI_SETTINGS_VIDEO_FILTER, 0);
   rgui_list_push(rgui->folder_buf, "Mute Audio", RGUI_SETTINGS_AUDIO_MUTE, 1);
}

static bool rgui_settings_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
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
      case RGUI_ACTION_CANCEL:
      case RGUI_ACTION_SETTINGS:
         rgui->mode = RGUI_FILEBROWSER;
         return true;
         break;

      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      {
         rgui_file_type_t type;
         rgui_list_at(rgui->folder_buf, rgui->directory_ptr, NULL, &type, NULL);
         rgui_settings_toggle_setting(type);
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui_settings_populate_entries(rgui);
         rgui->directory_ptr = 0;
         break;

      default:
         break;
   }

   size_t begin = rgui->directory_ptr >= TERM_HEIGHT / 2 ?
      rgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->directory_ptr + TERM_HEIGHT <= rgui_list_size(rgui->folder_buf) ?
      rgui->directory_ptr + TERM_HEIGHT : rgui_list_size(rgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_text(rgui, begin, end);

   return false;
}

const char *rgui_iterate(rgui_handle_t *rgui, rgui_action_t action)
{
   start:
   if (rgui->mode == RGUI_SETTINGS)
   {
      if (rgui_settings_iterate(rgui, action))
      {
         action = RGUI_ACTION_REFRESH;
         goto start;
      }
      else
      {
         return NULL;
      }
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

      case RGUI_ACTION_RIGHT:
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
            rarch_console_load_game(rgui->path_buf);
            rarch_settings_msg(S_MSG_LOADING_ROM, S_DELAY_1);
            found = true;
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->directory_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_SETTINGS:
         rgui->mode = RGUI_SETTINGS;
         action = RGUI_ACTION_REFRESH;
         goto start;

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

