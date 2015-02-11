/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../menu.h"
#include "../menu_input.h"
#include "../../retroarch.h"
#include <compat/posix_string.h>
#include <file/file_path.h>

#include "../../gfx/drivers_font_renderer/bitmap.h"

#include "shared.h"

#define RGUI_TERM_START_X (driver.menu->width / 21)
#define RGUI_TERM_START_Y (driver.menu->height / 9)
#define RGUI_TERM_WIDTH (((driver.menu->width - RGUI_TERM_START_X - RGUI_TERM_START_X) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT (((driver.menu->height - RGUI_TERM_START_Y - RGUI_TERM_START_X) / (FONT_HEIGHT_STRIDE)) - 1)

static int rgui_entry_iterate(unsigned action)
{
   const char *label = NULL;

   if (!driver.menu || !driver.menu->menu_list)
      return -1;

   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, NULL);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (!cbs)
      return -1;
   if (!cbs->action_iterate)
      return -1;

   return cbs->action_iterate(label, action);
}

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int x, y;

   if (!glyph)
      return;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem     = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static uint16_t gray_filler(unsigned x, unsigned y)
{
   unsigned col;

   x >>= 1;
   y >>= 1;
   col = ((x + y) & 1) + 1;

#if defined(GEKKO) || defined(PSP)
   return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
#else
   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
#endif
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   unsigned col;

   x >>= 1;
   y >>= 1;
   col = ((x + y) & 1) + 1;
#if defined(GEKKO) || defined(PSP)
   return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
#else
   return (col << 13) | (col << 10) | (col << 5) | (12 << 0);
#endif
}

static void fill_rect(menu_handle_t *menu,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   unsigned i, j;
    
   if (!menu->frame_buf.data || !col)
      return;
    
   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         menu->frame_buf.data[j * (menu->frame_buf.pitch >> 1) + i] = col(i, j);
}

static void color_rect(menu_handle_t *menu,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned i, j;

   if (!menu->frame_buf.data)
      return;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         if (i < driver.menu->width && j < driver.menu->height)
            menu->frame_buf.data[j * (menu->frame_buf.pitch >> 1) + i] = color;
}

static void blit_line(int x, int y, const char *message, bool green)
{
   unsigned i, j;
   menu_handle_t *menu = driver.menu;

   if (!menu)
      return;

   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset  = (i + j * FONT_WIDTH) >> 3;
            bool col    = (menu->font[FONT_OFFSET
                  ((unsigned char)*message) + offset] & rem);

            if (!col)
               continue;

            menu->frame_buf.data[(y + j) *
               (menu->frame_buf.pitch >> 1) + (x + i)] = green ?
#if defined(GEKKO)|| defined(PSP)
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

static bool init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));

   if (!font)
   {
      RARCH_ERR("Font memory allocation failed.\n");
      return false;
   }

   menu->alloc_font = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu->font = font;
   return true;
}

static bool rguidisp_init_font(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;

   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);

   if (!font_bin_buf)
      return false;

   menu->font = font_bin_buf;

   return true;
}

static void rgui_render_background(menu_handle_t *menu)
{
   if (!menu)
      return;

   fill_rect(menu, 0, 0, menu->width, menu->height, gray_filler);
   fill_rect(menu, 5, 5, menu->width - 10, 5, green_filler);
   fill_rect(menu, 5, menu->height - 10, menu->width - 10, 5,
         green_filler);

   fill_rect(menu, 5, 5, 5, menu->height - 10, green_filler);
   fill_rect(menu, menu->width - 10, 5, 5, menu->height - 10,
         green_filler);
}

static void rgui_render_messagebox(const char *message)
{
   size_t i;
   int x, y;
   unsigned width, glyphs_width, height;
   struct string_list *list = NULL;

   if (!driver.menu || !message || !*message)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   width        = 0;
   glyphs_width = 0;

   for (i = 0; i < list->size; i++)
   {
      unsigned line_width;
      char     *msg   = list->elems[i].data;
      unsigned msglen = strlen(msg);

      if (msglen > RGUI_TERM_WIDTH)
      {
         msg[RGUI_TERM_WIDTH - 2] = '.';
         msg[RGUI_TERM_WIDTH - 1] = '.';
         msg[RGUI_TERM_WIDTH - 0] = '.';
         msg[RGUI_TERM_WIDTH + 1] = '\0';
         msglen = RGUI_TERM_WIDTH;
      }

      line_width = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width = max(width, line_width);
      glyphs_width = max(glyphs_width, msglen);
   }

   height = FONT_HEIGHT_STRIDE * list->size + 6 + 10;
   x      = (driver.menu->width - width) / 2;
   y      = (driver.menu->height - height) / 2;

   fill_rect(driver.menu, x + 5, y + 5, width - 10, height - 10, gray_filler);
   fill_rect(driver.menu, x, y, width - 5, 5, green_filler);
   fill_rect(driver.menu, x + width - 5, y, 5, height - 5, green_filler);
   fill_rect(driver.menu, x + 5, y + height - 5, width - 5, 5, green_filler);
   fill_rect(driver.menu, x, y + 5, 5, height - 5, green_filler);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x = FONT_WIDTH_STRIDE * (glyphs_width - strlen(msg)) / 2;
      int offset_y = FONT_HEIGHT_STRIDE * i;
      blit_line(x + 8 + offset_x, y + 8 + offset_y, msg, false);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(menu_handle_t *menu)
{
   int16_t x = menu->mouse.x;
   int16_t y = menu->mouse.y;

   color_rect(driver.menu, x, y - 5, 1, 11, 0xFFFF);
   color_rect(driver.menu, x - 5, y, 11, 1, 0xFFFF);
}

static void rgui_render(void)
{
   size_t i, end;
   char title[256], title_buf[256], title_msg[64];
   char timedate[PATH_MAX_LENGTH];
   unsigned x, y, menu_type  = 0;
   const char *dir     = NULL;
   const char *label   = NULL;
   const char *core_name = NULL;
   const char *core_version = NULL;

   if (driver.menu->need_refresh 
         && g_extern.is_menu
         && !driver.menu->msg_force)
      return;

   driver.menu->mouse.ptr = driver.menu->mouse.y / 11 - 2 + driver.menu->begin;

   if (driver.menu->mouse.wheeldown && driver.menu->begin
         < menu_list_get_size(driver.menu->menu_list) - RGUI_TERM_HEIGHT)
      driver.menu->begin++;

   if (driver.menu->mouse.wheelup && driver.menu->begin > 0)
      driver.menu->begin--;

   /* Do not scroll if all items are visible. */
   if (menu_list_get_size(driver.menu->menu_list) <= RGUI_TERM_HEIGHT)
      driver.menu->begin = 0;

   end = (driver.menu->begin + RGUI_TERM_HEIGHT <=
         menu_list_get_size(driver.menu->menu_list)) ?
      driver.menu->begin + RGUI_TERM_HEIGHT :
      menu_list_get_size(driver.menu->menu_list);

   rgui_render_background(driver.menu);

   menu_list_get_last_stack(driver.menu->menu_list,
         &dir, &label, &menu_type);

#if 0
   RARCH_LOG("Dir is: %s\n", label);
#endif

   get_title(label, dir, menu_type, title, sizeof(title));

   menu_animation_ticker_line(title_buf, RGUI_TERM_WIDTH - 3,
         g_extern.frame_count / RGUI_TERM_START_X, title, true);
   blit_line(RGUI_TERM_START_X + RGUI_TERM_START_X, RGUI_TERM_START_X, title_buf, true);

   core_name = g_extern.menu.info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   core_version = g_extern.menu.info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   disp_timedate_set_label(timedate, sizeof(timedate), 3);

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
   blit_line(
         RGUI_TERM_START_X + RGUI_TERM_START_X,
         (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
         RGUI_TERM_START_Y + 2, title_msg, true);

   if (g_settings.menu.timedate_enable)
      blit_line(
            (RGUI_TERM_WIDTH * FONT_HEIGHT_STRIDE) + (60),
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
            RGUI_TERM_START_Y + 2, timedate, true);


   x = RGUI_TERM_START_X;
   y = RGUI_TERM_START_Y;

   for (i = driver.menu->begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      char message[PATH_MAX_LENGTH], type_str[PATH_MAX_LENGTH],
           entry_title_buf[PATH_MAX_LENGTH], type_str_buf[PATH_MAX_LENGTH],
           path_buf[PATH_MAX_LENGTH];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      bool selected = false;
      menu_file_list_cbs_t *cbs = NULL;

      menu_list_get_at_offset(driver.menu->menu_list->selection_buf, i, &path,
            &entry_label, &type);

      cbs = (menu_file_list_cbs_t*)
         menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
               i);

      if (cbs && cbs->action_get_representation)
         cbs->action_get_representation(driver.menu->menu_list->selection_buf,
               &w, type, i, label,
               type_str, sizeof(type_str), 
               entry_label, path,
               path_buf, sizeof(path_buf));

      selected = (i == driver.menu->selection_ptr);

      if (i > (driver.menu->selection_ptr + 100))
         continue;

      menu_animation_ticker_line(entry_title_buf, RGUI_TERM_WIDTH - (w + 1 + 2),
            g_extern.frame_count / RGUI_TERM_START_X, path_buf, selected);
      menu_animation_ticker_line(type_str_buf, w, g_extern.frame_count / RGUI_TERM_START_X,
            type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            RGUI_TERM_WIDTH - (w + 1 + 2),
            RGUI_TERM_WIDTH - (w + 1 + 2),
            entry_title_buf,
            w,
            type_str_buf);

      blit_line(x, y, message, selected);
   }

#ifdef GEKKO
   const char *message_queue;

   if (driver.menu->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      driver.menu->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   rgui_render_messagebox(message_queue);
#endif

   if (driver.menu->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      rgui_render_messagebox(msg);
   }

   if (driver.menu->mouse.enable)
      rgui_blit_cursor(driver.menu);
}

static void *rgui_init(void)
{
   bool ret = false;
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu->frame_buf.data = (uint16_t*)malloc(400 * 240 * sizeof(uint16_t)); 

   if (!menu->frame_buf.data)
      goto error;

   menu->width           = 320;
   menu->height          = 240;
   menu->begin           = 0;
   menu->frame_buf.pitch = menu->width * sizeof(uint16_t);

   ret = rguidisp_init_font(menu);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");

      rarch_main_command(RARCH_CMD_QUIT_RETROARCH);
      goto error;
   }

   return menu;

error:
   if (menu)
   {
      if (menu->frame_buf.data)
         free(menu->frame_buf.data);
      if (menu->userdata)
         free(menu->userdata);
      free(menu);
   }
   return NULL;
}

static void rgui_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   if (menu->userdata)
      free(menu->userdata);
   driver.menu->userdata = NULL;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);
}

static void rgui_set_texture(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;
   if (!driver.video_data)
      return;
   if (!driver.video_poke)
      return;
   if (!driver.video_poke->set_texture_frame)
      return;

   driver.video_poke->set_texture_frame(driver.video_data,
         menu->frame_buf.data, false, menu->width, menu->height, 1.0f);
}

static void rgui_navigation_clear(void *data, bool pending_push)
{
   driver.menu->begin = 0;
}

static void rgui_navigation_set(void *data, bool scroll)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;
   if (!scroll)
      return;

   if (menu->selection_ptr < RGUI_TERM_HEIGHT/2)
      menu->begin = 0;
   else if (menu->selection_ptr >= RGUI_TERM_HEIGHT/2
         && menu->selection_ptr <
         menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT/2)
      menu->begin = menu->selection_ptr - RGUI_TERM_HEIGHT/2;
   else if (menu->selection_ptr >=
         menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT/2)
      menu->begin = menu_list_get_size(menu->menu_list)
            - RGUI_TERM_HEIGHT;
}

static void rgui_navigation_set_last(void *data)
{
   rgui_navigation_set(data, true);
}

static void rgui_navigation_descend_alphabet(void *data, size_t *unused)
{
   rgui_navigation_set(data, true);
}

static void rgui_navigation_ascend_alphabet(void *data, size_t *unused)
{
   rgui_navigation_set(data, true);
}

static void rgui_populate_entries(void *data, const char *path,
      const char *label, unsigned k)
{
   rgui_navigation_set(data, true);
}

menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_render_messagebox,
   rgui_render,
   NULL,
   rgui_init,
   rgui_free,
   NULL,
   NULL,
   rgui_populate_entries,
   NULL,
   rgui_navigation_clear,
   NULL,
   NULL,
   rgui_navigation_set,
   rgui_navigation_set_last,
   rgui_navigation_descend_alphabet,
   rgui_navigation_ascend_alphabet,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   rgui_entry_iterate,
   "rgui",
};
