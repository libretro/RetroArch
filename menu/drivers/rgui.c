/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_inline.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../widgets/menu_input_dialog.h"

#include "../../configuration.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#define RGUI_TERM_START_X(width)        (width / 21)
#define RGUI_TERM_START_Y(height)       (height / 9)
#define RGUI_TERM_WIDTH(width)          (((width - RGUI_TERM_START_X(width) - RGUI_TERM_START_X(width)) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT(width, height) (((height - RGUI_TERM_START_Y(height) - RGUI_TERM_START_X(width)) / (FONT_HEIGHT_STRIDE)) - 1)

typedef struct
{
   bool bg_modified;
   bool force_redraw;
   bool mouse_show;
   unsigned last_width;
   unsigned last_height;
   unsigned frame_count;
   bool bg_thickness;
   bool border_thickness;
   float scroll_y;
   char *msgbox;
} rgui_t;

static uint16_t *rgui_framebuf_data      = NULL;

#if defined(GEKKO)|| defined(PSP)
#define HOVER_COLOR(settings)    ((3 << 0) | (10 << 4) | (3 << 8) | (7 << 12))
#define NORMAL_COLOR(settings)   0x7FFF
#define TITLE_COLOR(settings)    HOVER_COLOR(settings)
#else
#define HOVER_COLOR(settings)    (argb32_to_rgba4444(settings->uints.menu_entry_hover_color))
#define NORMAL_COLOR(settings)   (argb32_to_rgba4444(settings->uints.menu_entry_normal_color))
#define TITLE_COLOR(settings)    (argb32_to_rgba4444(settings->uints.menu_title_color))

static uint16_t argb32_to_rgba4444(uint32_t col)
{
   unsigned a = ((col >> 24) & 0xff) >> 4;
   unsigned r = ((col >> 16) & 0xff) >> 4;
   unsigned g = ((col >> 8)  & 0xff) >> 4;
   unsigned b = ((col & 0xff)      ) >> 4;
   return (r << 12) | (g << 8) | (b << 4) | a;
}
#endif


static uint16_t rgui_gray_filler(rgui_t *rgui, unsigned x, unsigned y)
{
   unsigned shft        = (rgui->bg_thickness ? 1 : 0);
   unsigned col         = (((x >> shft) + (y >> shft)) & 1) + 1;
#if defined(GEKKO) || defined(PSP)
   return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
#elif defined(HAVE_LIBNX) && !defined(HAVE_OPENGL)
   return (((31 * (54)) / 255) << 11) |
           (((63 * (54)) / 255) << 5) |
           ((31 * (54)) / 255);
#else
   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
#endif
}

static uint16_t rgui_green_filler(rgui_t *rgui, unsigned x, unsigned y)
{
   unsigned shft        = (rgui->border_thickness ? 1 : 0);
   unsigned col         = (((x >> shft) + (y >> shft)) & 1) + 1;
#if defined(GEKKO) || defined(PSP)
   return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
#elif defined(HAVE_LIBNX) && !defined(HAVE_OPENGL)
    return (((31 * (54)) / 255) << 11) |
           (((63 * (109)) / 255) << 5) |
           ((31 * (54)) / 255);
#else
   return (col << 13) | (col << 10) | (col << 5) | (12 << 0);
#endif
}

static void rgui_fill_rect(
      rgui_t *rgui,
      uint16_t *data,
      size_t pitch,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(rgui_t *rgui, unsigned x, unsigned y))
{
   unsigned i, j;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         data[j * (pitch >> 1) + i] = col(rgui, i, j);
}

static void rgui_color_rect(
      uint16_t *data,
      size_t pitch,
      unsigned fb_width, unsigned fb_height,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t color)
{
   unsigned i, j;

   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         if (i < fb_width && j < fb_height)
            data[j * (pitch >> 1) + i] = color;
}

static void blit_line(int x, int y,
      const char *message, uint16_t color)
{
   size_t pitch = menu_display_get_framebuffer_pitch();
   const uint8_t *font_fb = menu_display_get_font_framebuffer();

   if (font_fb)
   {
      while (!string_is_empty(message))
      {
         unsigned i, j;
         char symbol = *message++;

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            for (i = 0; i < FONT_WIDTH; i++)
            {
               uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
               int offset  = (i + j * FONT_WIDTH) >> 3;
               bool col    = (font_fb[FONT_OFFSET(symbol) + offset] & rem);

               if (col)
                  rgui_framebuf_data[(y + j) * (pitch >> 1) + (x + i)] = color;
            }
         }

         x += FONT_WIDTH_STRIDE;
      }
   }
}

#if 0
static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int x, y;

   if (!glyph)
      return;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col    =
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

static bool init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   bool fb_font_inited  = true;
   uint8_t        *font = (uint8_t *)calloc(1, FONT_OFFSET(256));

   if (!font)
      return false;

   menu_display_set_font_data_init(fb_font_inited);

   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu_display_set_font_framebuffer(font);

   return true;
}
#endif

static bool rguidisp_init_font(menu_handle_t *menu)
{
#if 0
   const uint8_t *font_bmp_buf = NULL;
#endif
   const uint8_t *font_bin_buf = bitmap_bin;

   if (!menu)
      return false;

#if 0
   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);
#endif

   menu_display_set_font_framebuffer(font_bin_buf);

   return true;
}

static void rgui_render_background(rgui_t *rgui)
{
   size_t pitch_in_pixels, size;
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   uint16_t             *src  = NULL;
   uint16_t             *dst  = NULL;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   pitch_in_pixels = fb_pitch >> 1;
   size            = fb_pitch * 4;
   src             = rgui_framebuf_data + pitch_in_pixels * fb_height;
   dst             = rgui_framebuf_data;

   while (dst < src)
   {
      memcpy(dst, src, size);
      dst += pitch_in_pixels * 4;
   }

   if (rgui_framebuf_data)
   {
      settings_t *settings       = config_get_ptr();

      if (settings->bools.menu_rgui_border_filler_enable)
      {
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, 5, fb_width - 10, 5, rgui_green_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, fb_height - 10, fb_width - 10, 5, rgui_green_filler);

         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, 5, 5, 5, fb_height - 10, rgui_green_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data, fb_pitch, fb_width - 10, 5, 5, fb_height - 10,
               rgui_green_filler);
      }
   }
}

static void rgui_set_message(void *data, const char *message)
{
   rgui_t           *rgui = (rgui_t*)data;

   if (!rgui || !message || !*message)
      return;

   if (!string_is_empty(rgui->msgbox))
      free(rgui->msgbox);
   rgui->msgbox       = strdup(message);
   rgui->force_redraw = true;
}

static void rgui_render_messagebox(rgui_t *rgui, const char *message)
{
   int x, y;
   uint16_t color;
   size_t i, fb_pitch;
   unsigned fb_width, fb_height;
   unsigned width, glyphs_width, height;
   struct string_list *list   = NULL;
   settings_t *settings       = config_get_ptr();

   (void)settings;

   if (!message || !*message)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;
   if (list->elems == 0)
      goto end;

   width        = 0;
   glyphs_width = 0;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   for (i = 0; i < list->size; i++)
   {
      unsigned line_width;
      char     *msg   = list->elems[i].data;
      unsigned msglen = (unsigned)utf8len(msg);

      if (msglen > RGUI_TERM_WIDTH(fb_width))
      {
         msg[RGUI_TERM_WIDTH(fb_width) - 2] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) - 1] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) - 0] = '.';
         msg[RGUI_TERM_WIDTH(fb_width) + 1] = '\0';
         msglen = RGUI_TERM_WIDTH(fb_width);
      }

      line_width   = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width        = MAX(width, line_width);
      glyphs_width = MAX(glyphs_width, msglen);
   }

   height = (unsigned)(FONT_HEIGHT_STRIDE * list->size + 6 + 10);
   x      = (fb_width  - width) / 2;
   y      = (fb_height - height) / 2;

   if (rgui_framebuf_data)
   {
      rgui_fill_rect(rgui, rgui_framebuf_data,
            fb_pitch, x + 5, y + 5, width - 10,
            height - 10, rgui_gray_filler);

      if (settings->bools.menu_rgui_border_filler_enable)
      {
         rgui_fill_rect(rgui, rgui_framebuf_data,
               fb_pitch, x, y, width - 5, 5, rgui_green_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data,
               fb_pitch, x + width - 5, y, 5,
               height - 5, rgui_green_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data,
               fb_pitch, x + 5, y + height - 5,
               width - 5, 5, rgui_green_filler);
         rgui_fill_rect(rgui, rgui_framebuf_data,
               fb_pitch, x, y + 5, 5,
               height - 5, rgui_green_filler);
      }
   }

   color = NORMAL_COLOR(settings);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x    = (int)(FONT_WIDTH_STRIDE * (glyphs_width - utf8len(msg)) / 2);
      int offset_y    = (int)(FONT_HEIGHT_STRIDE * i);

      if (rgui_framebuf_data)
         blit_line(x + 8 + offset_x, y + 8 + offset_y, msg, color);
   }

end:
   string_list_free(list);
}

static void rgui_blit_cursor(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   int16_t        x   = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t        y   = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (rgui_framebuf_data)
   {
      rgui_color_rect(rgui_framebuf_data, fb_pitch, fb_width, fb_height, x, y - 5, 1, 11, 0xFFFF);
      rgui_color_rect(rgui_framebuf_data, fb_pitch, fb_width, fb_height, x - 5, y, 11, 1, 0xFFFF);
   }
}

static void rgui_frame(void *data, video_frame_info_t *video_info)
{
   rgui_t *rgui                   = (rgui_t*)data;
   settings_t *settings           = config_get_ptr();

   if ((settings->bools.menu_rgui_background_filler_thickness_enable != rgui->bg_thickness) ||
       (settings->bools.menu_rgui_border_filler_thickness_enable     != rgui->border_thickness)
      )
      rgui->bg_modified           = true;

   rgui->bg_thickness             = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness         = settings->bools.menu_rgui_border_filler_thickness_enable;

   rgui->frame_count++;
}

static void rgui_render(void *data, bool is_idle)
{
   menu_animation_ctx_ticker_t ticker;
   unsigned x, y;
   uint16_t hover_color, normal_color;
   size_t i, end, fb_pitch, old_start;
   unsigned fb_width, fb_height;
   int bottom;
   char title[255];
   char title_buf[255];
   char title_msg[64];
   char msg[255];
   size_t entries_end             = 0;
   bool msg_force                 = false;
   settings_t *settings           = config_get_ptr();
   rgui_t *rgui                   = (rgui_t*)data;
   uint64_t frame_count           = rgui->frame_count;

   msg[0] = title[0] = title_buf[0] = title_msg[0] = '\0';

   if (!rgui->force_redraw)
   {
      msg_force = menu_display_get_msg_force();

      if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
            && menu_driver_is_alive() && !msg_force)
         return;

      if (is_idle || !menu_display_get_update_pending())
         return;
   }

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   /* if the framebuffer changed size, recache the background */
   if (rgui->bg_modified || rgui->last_width != fb_width || rgui->last_height != fb_height)
   {
      if (rgui_framebuf_data)
         rgui_fill_rect(rgui, rgui_framebuf_data,
               fb_pitch, 0, fb_height, fb_width, 4, rgui_gray_filler);
      rgui->last_width  = fb_width;
      rgui->last_height = fb_height;
   }
   
   if (rgui->bg_modified)
      rgui->bg_modified = false;

   menu_display_set_framebuffer_dirty_flag();
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

   rgui->force_redraw        = false;

   if (settings->bools.menu_pointer_enable)
   {
      unsigned new_val;

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

      new_val = (unsigned)(menu_input_pointer_state(MENU_POINTER_Y_AXIS)
         / (11 - 2 + old_start));

      menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &new_val);

      if (menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
      {
         size_t start;
         int16_t delta_y = menu_input_pointer_state(MENU_POINTER_DELTA_Y_AXIS);
         rgui->scroll_y += delta_y;

         start = -rgui->scroll_y / 11 + 2;

         menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

         if (rgui->scroll_y > 0)
            rgui->scroll_y = 0;
      }
   }

   if (settings->bools.menu_mouse_enable)
   {
      unsigned new_mouse_ptr;
      int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

      menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

      new_mouse_ptr = (unsigned)(mouse_y / 11 - 2 + old_start);

      menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &new_mouse_ptr);
   }

   /* Do not scroll if all items are visible. */
   if (menu_entries_get_size() <= RGUI_TERM_HEIGHT(fb_width, fb_height))
   {
      size_t start = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   }

   bottom    = (int)(menu_entries_get_size() - RGUI_TERM_HEIGHT(fb_width, fb_height));
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   if (old_start > (unsigned)bottom)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &bottom);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

   entries_end = menu_entries_get_size();

   end         = ((old_start + RGUI_TERM_HEIGHT(fb_width, fb_height)) <= (entries_end)) ?
      old_start + RGUI_TERM_HEIGHT(fb_width, fb_height) : entries_end;

   rgui_render_background(rgui);

   menu_entries_get_title(title, sizeof(title));

   ticker.s        = title_buf;
   ticker.len      = RGUI_TERM_WIDTH(fb_width) - 10;
   ticker.idx      = frame_count / RGUI_TERM_START_X(fb_width);
   ticker.str      = title;
   ticker.selected = true;

   menu_animation_ticker(&ticker);

   hover_color  = HOVER_COLOR(settings);
   normal_color = NORMAL_COLOR(settings);

   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
   {
      char back_buf[32];
      char back_msg[32];

      back_buf[0] = back_msg[0] = '\0';

      strlcpy(back_buf, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK), sizeof(back_buf));
      string_to_upper(back_buf);
      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_START_X(fb_width),
               RGUI_TERM_START_X(fb_width),
               back_msg,
               TITLE_COLOR(settings));
   }

   string_to_upper(title_buf);

   if (rgui_framebuf_data)
      blit_line(
            (int)(RGUI_TERM_START_X(fb_width) + (RGUI_TERM_WIDTH(fb_width)
                  - utf8len(title_buf)) * FONT_WIDTH_STRIDE / 2),
            RGUI_TERM_START_X(fb_width),
            title_buf, TITLE_COLOR(settings));

   if (settings->bools.menu_core_enable &&
         menu_entries_get_core_title(title_msg, sizeof(title_msg)) == 0)
   {
      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_START_X(fb_width),
               (RGUI_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
               RGUI_TERM_START_Y(fb_height) + 2, title_msg, hover_color);
   }

   if (settings->bools.menu_timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];

      timedate[0]        = '\0';

      datetime.s         = timedate;
      datetime.len       = sizeof(timedate);
      datetime.time_mode = 4;

      menu_display_timedate(&datetime);

      if (rgui_framebuf_data)
         blit_line(
               RGUI_TERM_WIDTH(fb_width) * FONT_WIDTH_STRIDE - RGUI_TERM_START_X(fb_width),
               (RGUI_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
               RGUI_TERM_START_Y(fb_height) + 2, timedate, hover_color);
   }

   x = RGUI_TERM_START_X(fb_width);
   y = RGUI_TERM_START_Y(fb_height);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      menu_entry_t entry;
      menu_animation_ctx_ticker_t ticker;
      char entry_value[255];
      char message[255];
      char entry_title_buf[255];
      char type_str_buf[255];
      char *entry_path                      = NULL;
      unsigned entry_spacing                = 0;
      size_t entry_title_buf_utf8len        = 0;
      size_t entry_title_buf_len            = 0;
      bool                entry_selected    = menu_entry_is_currently_selected((unsigned)i);
      size_t selection                      = menu_navigation_get_selection();

      if (i > (selection + 100))
         continue;

      entry_value[0]     = '\0';
      message[0]         = '\0';
      entry_title_buf[0] = '\0';
      type_str_buf[0]    = '\0';

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

      entry_spacing = menu_entry_get_spacing(&entry);
      menu_entry_get_value(&entry, entry_value, sizeof(entry_value));
      entry_path      = menu_entry_get_rich_label(&entry);

      ticker.s        = entry_title_buf;
      ticker.len      = RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2);
      ticker.idx      = frame_count / RGUI_TERM_START_X(fb_width);
      ticker.str      = entry_path;
      ticker.selected = entry_selected;

      menu_animation_ticker(&ticker);

      ticker.s        = type_str_buf;
      ticker.len      = entry_spacing;
      ticker.str      = entry_value;

      menu_animation_ticker(&ticker);

      entry_title_buf_utf8len = utf8len(entry_title_buf);
      entry_title_buf_len     = strlen(entry_title_buf);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            entry_selected ? '>' : ' ',
            (int)(RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
            (int)(RGUI_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
            entry_title_buf,
            entry_spacing,
            type_str_buf);

      if (rgui_framebuf_data)
         blit_line(x, y, message,
               entry_selected ? hover_color : normal_color);

      menu_entry_free(&entry);
      if (!string_is_empty(entry_path))
         free(entry_path);
   }

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      rgui_render_messagebox(rgui, msg);
   }

   if (!string_is_empty(rgui->msgbox))
   {
      rgui_render_messagebox(rgui, rgui->msgbox);
      free(rgui->msgbox);
      rgui->msgbox       = NULL;
      rgui->force_redraw = true;
   }

   if (rgui->mouse_show)
   {
      settings_t *settings = config_get_ptr();
      bool cursor_visible  = settings->bools.video_fullscreen ||
         !video_driver_has_windowed();

      if (settings->bools.menu_mouse_enable && cursor_visible)
         rgui_blit_cursor();
   }

}

static void rgui_framebuffer_free(void)
{
   if (rgui_framebuf_data)
      free(rgui_framebuf_data);
   rgui_framebuf_data = NULL;
}

static void *rgui_init(void **userdata, bool video_is_threaded)
{
   size_t fb_pitch, start;
   unsigned fb_width, fb_height, new_font_height;
   rgui_t               *rgui = NULL;
   bool                   ret = false;
   settings_t *settings       = config_get_ptr();
   menu_handle_t        *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   rgui = (rgui_t*)calloc(1, sizeof(rgui_t));

   if (!rgui)
      goto error;

   *userdata              = rgui;

   /* 4 extra lines to cache  the checked background */
   rgui_framebuf_data = (uint16_t*)
      calloc(400 * (240 + 4), sizeof(uint16_t));

   if (!rgui_framebuf_data)
      goto error;

   fb_width                   = 320;
   fb_height                  = 240;
   fb_pitch                   = fb_width * sizeof(uint16_t);
   new_font_height            = FONT_HEIGHT_STRIDE * 2;

   menu_display_set_width(fb_width);
   menu_display_set_height(fb_height);
   menu_display_set_header_height(new_font_height);
   menu_display_set_framebuffer_pitch(fb_pitch);

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

   ret = rguidisp_init_font(menu);

   if (!ret)
      goto error;

   rgui->bg_thickness             = settings->bools.menu_rgui_background_filler_thickness_enable;
   rgui->border_thickness         = settings->bools.menu_rgui_border_filler_thickness_enable;
   rgui->bg_modified              = true;

   rgui->last_width  = fb_width;
   rgui->last_height = fb_height;

   return menu;

error:
   rgui_framebuffer_free();
   if (menu)
      free(menu);
   return NULL;
}


static void rgui_free(void *data)
{
   const uint8_t *font_fb;
   bool fb_font_inited   = false;

   fb_font_inited = menu_display_get_font_data_init();
   font_fb = menu_display_get_font_framebuffer();

   if (fb_font_inited)
      free((void*)font_fb);

   fb_font_inited = false;
   menu_display_set_font_data_init(fb_font_inited);
}

static void rgui_set_texture(void)
{
   size_t fb_pitch;
   unsigned fb_width, fb_height;

   if (!menu_display_get_framebuffer_dirty_flag())
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   menu_display_unset_framebuffer_dirty_flag();

   video_driver_set_texture_frame(rgui_framebuf_data,
         false, fb_width, fb_height, 1.0f);
}

static void rgui_navigation_clear(void *data, bool pending_push)
{
   size_t start;
   rgui_t           *rgui = (rgui_t*)data;
   if (!rgui)
      return;

   start = 0;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
   rgui->scroll_y = 0;
}

static void rgui_navigation_set(void *data, bool scroll)
{
   size_t start, fb_pitch;
   unsigned fb_width, fb_height;
   bool do_set_start              = false;
   size_t end                     = menu_entries_get_size();
   size_t selection               = menu_navigation_get_selection();

   if (!scroll)
      return;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (selection < RGUI_TERM_HEIGHT(fb_width, fb_height) /2)
   {
      start        = 0;
      do_set_start = true;
   }
   else if (selection >= (RGUI_TERM_HEIGHT(fb_width, fb_height) /2)
         && selection < (end - RGUI_TERM_HEIGHT(fb_width, fb_height) /2))
   {
      start        = selection - RGUI_TERM_HEIGHT(fb_width, fb_height) /2;
      do_set_start = true;
   }
   else if (selection >= (end - RGUI_TERM_HEIGHT(fb_width, fb_height) /2))
   {
      start        = end - RGUI_TERM_HEIGHT(fb_width, fb_height);
      do_set_start = true;
   }

   if (do_set_start)
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
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

static void rgui_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   rgui_navigation_set(data, true);
}

static int rgui_environ(enum menu_environ_cb type,
      void *data, void *userdata)
{
   rgui_t           *rgui = (rgui_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!rgui)
            return -1;
         rgui->mouse_show = true;
         menu_display_set_framebuffer_dirty_flag();
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!rgui)
            return -1;
         rgui->mouse_show = false;
         menu_display_unset_framebuffer_dirty_flag();
         break;
      case 0:
      default:
         break;
   }

   return -1;
}

static int rgui_pointer_tap(void *data,
      unsigned x, unsigned y,
      unsigned ptr, menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned header_height = menu_display_get_header_height();

   if (y < header_height)
   {
      size_t selection = menu_navigation_get_selection();
      return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      size_t selection         = menu_navigation_get_selection();

      if (ptr == selection && cbs && cbs->action_select)
         return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_SELECT);

      menu_navigation_set_selection(ptr);
      menu_driver_navigation_set(false);
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_set_message,
   generic_menu_iterate,
   rgui_render,
   rgui_frame,
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
   generic_menu_init_list,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "rgui",
   rgui_environ,
   rgui_pointer_tap,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};
