/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sgui.h"
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

#define TERM_WIDTH (((SGUI_WIDTH - 30) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((SGUI_HEIGHT - 30) / (FONT_HEIGHT_STRIDE)))
#define TERM_START_X 15
#define TERM_START_Y 15

struct sgui_handle
{
   uint16_t *frame_buf;
   size_t frame_buf_pitch;
   const uint8_t *font_buf;

   sgui_folder_enum_cb_t folder_cb;
   void *userdata;

   sgui_list_t *path_stack;
   sgui_list_t *folder_buf;
   size_t directory_ptr;
   bool need_refresh;

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

static void init_font(sgui_handle_t *sgui, const char *path)
{
   for (unsigned i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      copy_glyph(sgui->font_white[i],
            sgui->font_green[i],
            sgui->font_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }
}

sgui_handle_t *sgui_init(const char *base_path,
      uint16_t *buf, size_t buf_pitch,
      const uint8_t *font_buf,
      sgui_folder_enum_cb_t folder_cb, void *userdata)
{
   sgui_handle_t *sgui = (sgui_handle_t*)calloc(1, sizeof(*sgui));

   sgui->frame_buf = buf;
   sgui->frame_buf_pitch = buf_pitch;
   sgui->font_buf = font_buf;

   sgui->folder_cb = folder_cb;
   sgui->userdata = userdata;

   sgui->path_stack = sgui_list_new();
   sgui->folder_buf = sgui_list_new();
   sgui_list_push(sgui->path_stack, base_path, SGUI_FILE_DIRECTORY);

   init_font(sgui, "font.bmp");

   return sgui;
}

void sgui_free(sgui_handle_t *sgui)
{
   sgui_list_free(sgui->path_stack);
   sgui_list_free(sgui->folder_buf);
   free(sgui);
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

static void blit_line(sgui_handle_t *sgui,
      unsigned x, unsigned y, const char *message, bool green)
{
   while (*message)
   {
      for (unsigned j = 0; j < FONT_HEIGHT; j++)
      {
         for (unsigned i = 0; i < FONT_WIDTH; i++)
         {
            uint16_t col = green ? 
               sgui->font_green[(unsigned char)*message][j][i] :
               sgui->font_white[(unsigned char)*message][j][i];

            if (col)
               sgui->frame_buf[(y + j) * (sgui->frame_buf_pitch >> 1) + (x + i)] = col;
         }
      }

      x += FONT_WIDTH_STRIDE;
      message++;
   }
}

static void render_text(sgui_handle_t *sgui, size_t begin, size_t end)
{
   fill_rect(sgui->frame_buf, sgui->frame_buf_pitch,
         0, 0, SGUI_WIDTH, SGUI_HEIGHT, gray_filler);

   fill_rect(sgui->frame_buf, sgui->frame_buf_pitch,
         5, 5, SGUI_WIDTH - 10, 5, green_filler);

   fill_rect(sgui->frame_buf, sgui->frame_buf_pitch,
         5, SGUI_HEIGHT - 10, SGUI_WIDTH - 10, 5, green_filler);

   fill_rect(sgui->frame_buf, sgui->frame_buf_pitch,
         5, 5, 5, SGUI_HEIGHT - 10, green_filler);

   fill_rect(sgui->frame_buf, sgui->frame_buf_pitch,
         SGUI_WIDTH - 10, 5, 5, SGUI_HEIGHT - 10, green_filler);

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path;
      sgui_file_type_t type;
      sgui_list_at(sgui->folder_buf, i, &path, &type);

      char message[TERM_WIDTH + 1];
      snprintf(message, sizeof(message), "%c %-*s %6s\n",
            i == sgui->directory_ptr ? '>' : ' ',
            TERM_WIDTH - (6 + 1 + 2),
            path,
            type == SGUI_FILE_PLAIN ? "(FILE)" : "(DIR)");

      blit_line(sgui, x, y, message, i == sgui->directory_ptr);
   }
}

const char *sgui_iterate(sgui_handle_t *sgui, sgui_action_t action)
{
   switch (action)
   {
      case SGUI_ACTION_UP:
         if (sgui->directory_ptr > 0)
            sgui->directory_ptr--;
         break;

      case SGUI_ACTION_DOWN:
         if (sgui->directory_ptr + 1 < sgui_list_size(sgui->folder_buf))
            sgui->directory_ptr++;
         break;

      case SGUI_ACTION_LEFT:
      case SGUI_ACTION_CANCEL:
         if (sgui_list_size(sgui->path_stack) > 1)
         {
            sgui_list_pop(sgui->path_stack);
            sgui->need_refresh = true;
         }
         break;

      case SGUI_ACTION_RIGHT:
      case SGUI_ACTION_OK:
      {
         if (sgui_list_size(sgui->folder_buf) == 0)
            return NULL;

         const char *path = NULL;
         sgui_file_type_t type = SGUI_FILE_PLAIN;
         sgui_list_at(sgui->folder_buf, sgui->directory_ptr,
               &path, &type);

         const char *dir;
         sgui_list_back(sgui->path_stack, &dir, NULL);

         if (type == SGUI_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            snprintf(cat_path, sizeof(cat_path), "%s/%s",
                  strcmp(dir, "/") == 0 ? "" : dir, path);

            sgui_list_push(sgui->path_stack, cat_path, SGUI_FILE_DIRECTORY);
            sgui->need_refresh = true;
         }
         else
         {
            snprintf(sgui->path_buf, sizeof(sgui->path_buf), "%s/%s",
                  strcmp(dir, "/") == 0 ? "" : dir, path);
            return sgui->path_buf;
         }
         break;
      }

      case SGUI_ACTION_REFRESH:
         sgui->need_refresh = true;
         break;

      default:
         return NULL;
   }

   if (sgui->need_refresh)
   {
      sgui->directory_ptr = 0;
      sgui_list_clear(sgui->folder_buf);

      const char *path = NULL;
      sgui_list_back(sgui->path_stack, &path, NULL);

      if (!sgui->folder_cb(path,
            (sgui_file_enum_cb_t)sgui_list_push,
            sgui->userdata, sgui->folder_buf))
         return NULL;

      sgui_list_sort(sgui->folder_buf);

      sgui->need_refresh = false;
   }

   size_t begin = sgui->directory_ptr >= TERM_HEIGHT / 2 ?
      sgui->directory_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = sgui->directory_ptr + TERM_HEIGHT <= sgui_list_size(sgui->folder_buf) ?
      sgui->directory_ptr + TERM_HEIGHT : sgui_list_size(sgui->folder_buf);

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

#if 0
   printf("========================================\n");
   for (size_t i = begin; i < end; i++)
   {
      const char *path;
      sgui_file_type_t type;
      sgui_list_at(sgui->folder_buf, i, &path, &type);

      printf("%c %-50s %s\n",
            i == sgui->directory_ptr ? '>' : ' ',
            path,
            type == SGUI_FILE_PLAIN ? "(FILE)" : "(DIR)");

   }
   printf("========================================\n");
#endif

   render_text(sgui, begin, end);
   return NULL;
}

