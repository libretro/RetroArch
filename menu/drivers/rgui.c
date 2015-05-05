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
#include "../menu_display.h"
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_inline.h>

#include "../../gfx/drivers_font_renderer/bitmap.h"

#include "shared.h"

#define RGUI_TERM_START_X (menu->frame_buf.width / 21)
#define RGUI_TERM_START_Y (menu->frame_buf.height / 9)
#define RGUI_TERM_WIDTH (((menu->frame_buf.width - RGUI_TERM_START_X - RGUI_TERM_START_X) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT (((menu->frame_buf.height - RGUI_TERM_START_Y - RGUI_TERM_START_X) / (FONT_HEIGHT_STRIDE)) - 1)

#if defined(GEKKO)|| defined(PSP)
#define HOVER_COLOR(settings) ((3 << 0) | (10 << 4) | (3 << 8) | (7 << 12))
#define NORMAL_COLOR(settings) 0x7FFF
#define TITLE_COLOR(settings) HOVER_COLOR(settings)
#else
#define HOVER_COLOR(settings) (argb32_to_rgba4444(settings->menu.entry_hover_color))
#define NORMAL_COLOR(settings) (argb32_to_rgba4444(settings->menu.entry_normal_color))
#define TITLE_COLOR(settings) (argb32_to_rgba4444(settings->menu.title_color))
#endif

static INLINE uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8) & 0xff) >> 4;
   unsigned b = (col & 0xff) >> 4;
   return (r << 12) | (g << 8)  | (b << 4) | a;
}

static int rgui_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t *menu = menu_driver_get_ptr();
   runloop_t *runloop  = rarch_main_get_ptr();

   if (!menu)
      return -1;
   if (!menu->menu_list)
      return -1;

   if (action != MENU_ACTION_NOOP || menu->need_refresh || menu_display_update_pending())
      runloop->frames.video.current.menu.framebuf.dirty   = true;

   cbs = (menu_file_list_cbs_t*)menu_list_get_actiondata_at_offset(
         menu->menu_list->selection_buf, menu->navigation.selection_ptr);

   menu_list_get_last_stack(menu->menu_list, NULL, &label, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);

   return -1;
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

static void fill_rect(menu_framebuf_t *frame_buf,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   unsigned i, j;
    
   if (!frame_buf->data || !col)
      return;
    
   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         frame_buf->data[j * (frame_buf->pitch >> 1) + i] = col(i, j);
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
         if (i < menu->frame_buf.width && j < menu->frame_buf.height)
            menu->frame_buf.data[j * (menu->frame_buf.pitch >> 1) + i] = color;
}

static void blit_line(menu_handle_t *menu, int x, int y,
      const char *message, uint16_t color)
{
   unsigned i, j;

   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset  = (i + j * FONT_WIDTH) >> 3;
            bool col    = (menu->font.framebuf[FONT_OFFSET
                  ((unsigned char)*message) + offset] & rem);

            if (!col)
               continue;

            menu->frame_buf.data[(y + j) *
               (menu->frame_buf.pitch >> 1) + (x + i)] = color;
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

   menu->font.alloc_framebuf = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu->font.framebuf = font;
   return true;
}

static bool rguidisp_init_font(menu_handle_t *menu)
{
   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;

   if (!menu)
      return false;

   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);

   if (!font_bin_buf)
      return false;

   menu->font.framebuf = font_bin_buf;

   return true;
}

static void rgui_render_background(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   size_t pitch_in_pixels = menu->frame_buf.pitch >> 1;
   size_t size = menu->frame_buf.pitch * 4;
   uint16_t *src = menu->frame_buf.data + pitch_in_pixels * menu->frame_buf.height;
   uint16_t *dst = menu->frame_buf.data;

   while (dst < src)
   {
      memcpy(dst, src, size);
      dst += pitch_in_pixels * 4;
   }

   fill_rect(&menu->frame_buf, 5, 5, menu->frame_buf.width - 10, 5, green_filler);
   fill_rect(&menu->frame_buf, 5, menu->frame_buf.height - 10,
         menu->frame_buf.width - 10, 5,
         green_filler);

   fill_rect(&menu->frame_buf, 5, 5, 5, menu->frame_buf.height - 10, green_filler);
   fill_rect(&menu->frame_buf, menu->frame_buf.width - 10, 5, 5,
         menu->frame_buf.height - 10,
         green_filler);
}

static void rgui_render_messagebox(const char *message)
{
   size_t i;
   int x, y;
   unsigned width, glyphs_width, height;
   uint16_t color;
   struct string_list *list = NULL;
   menu_handle_t *menu  = menu_driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu)
      return;
   if (!message || !*message)
      return;

   (void)settings;

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
   x      = (menu->frame_buf.width - width) / 2;
   y      = (menu->frame_buf.height - height) / 2;

   fill_rect(&menu->frame_buf, x + 5, y + 5, width - 10,
         height - 10, gray_filler);
   fill_rect(&menu->frame_buf, x, y, width - 5, 5, green_filler);
   fill_rect(&menu->frame_buf, x + width - 5, y, 5,
         height - 5, green_filler);
   fill_rect(&menu->frame_buf, x + 5, y + height - 5,
         width - 5, 5, green_filler);
   fill_rect(&menu->frame_buf, x, y + 5, 5,
         height - 5, green_filler);

   color = NORMAL_COLOR(settings);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x = FONT_WIDTH_STRIDE * (glyphs_width - strlen(msg)) / 2;
      int offset_y = FONT_HEIGHT_STRIDE * i;
      blit_line(menu, x + 8 + offset_x, y + 8 + offset_y, msg, color);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(menu_handle_t *menu)
{
   int16_t x = menu->mouse.x;
   int16_t y = menu->mouse.y;

   color_rect(menu, x, y - 5, 1, 11, 0xFFFF);
   color_rect(menu, x - 5, y, 11, 1, 0xFFFF);
}

static void rgui_render(void)
{
   size_t i, end;
   int bottom;
   char title[256], title_buf[256], title_msg[64];
   char timedate[PATH_MAX_LENGTH];
   unsigned x, y, menu_type  = 0;
   uint16_t hover_color, normal_color;
   const char *dir          = NULL;
   const char *label        = NULL;
   const char *core_name    = NULL;
   const char *core_version = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   runloop_t *runloop       = rarch_main_get_ptr();
   driver_t *driver         = driver_get_ptr();
   global_t *global         = global_get_ptr();
   settings_t *settings     = config_get_ptr();

   (void)driver;

   if (!menu)
      return;

   if (menu->need_refresh && runloop->is_menu
         && !menu->msg_force)
      return;

   if (runloop->is_idle)
      return;

   if (!menu_display_update_pending())
      return;

   /* ensures the framebuffer will be rendered on the screen */
   runloop->frames.video.current.menu.framebuf.dirty = true;
   runloop->frames.video.current.menu.animation.is_active = false;
   runloop->frames.video.current.menu.label.is_updated    = false;

   if (settings->menu.pointer.enable)
   {
      menu->pointer.ptr = menu->pointer.y / 11 - 2 + menu->begin;

      if (menu->pointer.dragging)
      {
         menu->scroll_y += menu->pointer.dy;
         menu->begin = -menu->scroll_y / 11 + 2;
         if (menu->scroll_y > 0)
            menu->scroll_y = 0;
      }
   }
   
   if (settings->menu.mouse.enable)
   {
      if (menu->mouse.scrolldown && menu->begin
            < menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT)
         menu->begin++;

      if (menu->mouse.scrollup && menu->begin > 0)
         menu->begin--;

      menu->mouse.ptr = menu->mouse.y / 11 - 2 + menu->begin;
   }

   /* Do not scroll if all items are visible. */
   if (menu_list_get_size(menu->menu_list) <= RGUI_TERM_HEIGHT)
      menu->begin = 0;

   bottom = menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT;
   if (menu->begin > bottom)
      menu->begin = bottom;

   end = (menu->begin + RGUI_TERM_HEIGHT <=
         menu_list_get_size(menu->menu_list)) ?
      menu->begin + RGUI_TERM_HEIGHT :
      menu_list_get_size(menu->menu_list);

   rgui_render_background();

   menu_list_get_last_stack(menu->menu_list,
         &dir, &label, &menu_type);

#if 0
   RARCH_LOG("Dir is: %s\n", label);
#endif

   get_title(label, dir, menu_type, title, sizeof(title));

   menu_animation_ticker_line(title_buf, RGUI_TERM_WIDTH - 10,
         runloop->frames.video.count / RGUI_TERM_START_X, title, true);

   hover_color = HOVER_COLOR(settings);
   normal_color = NORMAL_COLOR(settings);

   if (file_list_get_size(menu->menu_list->menu_stack) > 1)
      blit_line(menu,
            RGUI_TERM_START_X, RGUI_TERM_START_X,
            "BACK", TITLE_COLOR(settings));

   blit_line(menu,
         RGUI_TERM_START_X + (RGUI_TERM_WIDTH - strlen(title_buf)) * FONT_WIDTH_STRIDE / 2,
         RGUI_TERM_START_X, title_buf, TITLE_COLOR(settings));

   core_name = global->menu.info.library_name;
   if (!core_name)
      core_name = global->system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   if (settings->menu.core_enable)
   {
      core_version = global->menu.info.library_version;
      if (!core_version)
         core_version = global->system.info.library_version;
      if (!core_version)
         core_version = "";

      snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
            core_name, core_version);
      blit_line(menu,
            RGUI_TERM_START_X,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
            RGUI_TERM_START_Y + 2, title_msg, hover_color);
   }

   if (settings->menu.timedate_enable)
   {
      disp_timedate_set_label(timedate, sizeof(timedate), 3);

      blit_line(menu,
            RGUI_TERM_WIDTH * FONT_WIDTH_STRIDE - RGUI_TERM_START_X,
            (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
            RGUI_TERM_START_Y + 2, timedate, hover_color);
   }

   x = RGUI_TERM_START_X;
   y = RGUI_TERM_START_Y;

   for (i = menu->begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      char message[PATH_MAX_LENGTH], type_str[PATH_MAX_LENGTH],
           entry_title_buf[PATH_MAX_LENGTH], type_str_buf[PATH_MAX_LENGTH],
           path_buf[PATH_MAX_LENGTH];
      unsigned w = 0;
      bool selected = false;

      menu_display_setting_label(i, &w, label,
            type_str, sizeof(type_str),
            path_buf, sizeof(path_buf));

      selected = (i == menu->navigation.selection_ptr);

      if (i > (menu->navigation.selection_ptr + 100))
         continue;

      menu_animation_ticker_line(entry_title_buf, RGUI_TERM_WIDTH - (w + 1 + 2),
            runloop->frames.video.count / RGUI_TERM_START_X, path_buf, selected);
      menu_animation_ticker_line(type_str_buf, w,
            runloop->frames.video.count / RGUI_TERM_START_X,
            type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            RGUI_TERM_WIDTH - (w + 1 + 2),
            RGUI_TERM_WIDTH - (w + 1 + 2),
            entry_title_buf,
            w,
            type_str_buf);

      blit_line(menu, x, y, message, selected ? hover_color : normal_color);
   }

#ifdef GEKKO
   const char *message_queue;

   if (menu->msg_force)
   {
      message_queue = rarch_main_msg_queue_pull();
      menu->msg_force = false;
   }
   else
      message_queue = driver->current_msg;

   rgui_render_messagebox( message_queue);
#endif

   if (menu->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char *str = *menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", menu->keyboard.label, str);
      rgui_render_messagebox(msg);
   }

   if (settings->menu.mouse.enable)
      rgui_blit_cursor(menu);
}

static void *rgui_init(void)
{
   bool ret = false;
   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   /* 4 extra lines to cache  the checked background */
   menu->frame_buf.data = (uint16_t*)calloc(400 * (240 + 4), sizeof(uint16_t));

   if (!menu->frame_buf.data)
      goto error;

   menu->frame_buf.width           = 320;
   menu->frame_buf.height          = 240;
   menu->header_height             = FONT_HEIGHT_STRIDE * 2;
   menu->begin                     = 0;
   menu->frame_buf.pitch           = menu->frame_buf.width * sizeof(uint16_t);

   ret = rguidisp_init_font(menu);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      goto error;
   }

   fill_rect(&menu->frame_buf, 0, menu->frame_buf.height,
         menu->frame_buf.width, 4, gray_filler);

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
   menu->userdata = NULL;

   if (menu->font.alloc_framebuf)
      free((uint8_t*)menu->font.framebuf);
}

static void rgui_set_texture(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   runloop_t *runloop  = rarch_main_get_ptr();

   if (!menu)
      return;
   if (!runloop->frames.video.current.menu.framebuf.dirty)
      return;

   runloop->frames.video.current.menu.framebuf.dirty = false;

   video_driver_set_texture_frame(
         menu->frame_buf.data, false,
         menu->frame_buf.width, menu->frame_buf.height, 1.0f);
}

static void rgui_navigation_clear(bool pending_push)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->begin = 0;
   menu->scroll_y = 0;
}

static void rgui_navigation_set(bool scroll)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   if (!scroll)
      return;

   if (menu->navigation.selection_ptr < RGUI_TERM_HEIGHT/2)
      menu->begin = 0;
   else if (menu->navigation.selection_ptr >= RGUI_TERM_HEIGHT/2
         && menu->navigation.selection_ptr <
         menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT/2)
      menu->begin = menu->navigation.selection_ptr - RGUI_TERM_HEIGHT/2;
   else if (menu->navigation.selection_ptr >=
         menu_list_get_size(menu->menu_list) - RGUI_TERM_HEIGHT/2)
      menu->begin = menu_list_get_size(menu->menu_list)
            - RGUI_TERM_HEIGHT;
}

static void rgui_navigation_set_last(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_navigation_descend_alphabet(size_t *unused)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_navigation_ascend_alphabet(size_t *unused)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
}

static void rgui_populate_entries(const char *path,
      const char *label, unsigned k)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (menu)
      rgui_navigation_set(true);
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
   NULL,
   "rgui",
};
