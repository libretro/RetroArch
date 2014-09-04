/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include "../backend/menu_common_backend.h"
#include "../menu_common.h"
#include "../../../general.h"
#include "../../../gfx/gfx_common.h"
#include "../../../config.def.h"
#include "../../../file.h"
#include "../../../dynamic.h"
#include "../../../compat/posix_string.h"
#include "../../../performance.h"
#include "../../../input/input_common.h"

#include "../../../settings_data.h"
#include "../../../screenshot.h"
#include "../../../gfx/fonts/bitmap.h"

#include "shared.h"

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#define HAVE_SHADER_MANAGER
#endif

#if defined(PSP)
static uint16_t __attribute((aligned(64))) menu_framebuf[400 * 240];
#else
static uint16_t menu_framebuf[400 * 240];
#endif

#define RGUI_TERM_START_X 15
#define RGUI_TERM_START_Y 27
#define RGUI_TERM_WIDTH (((driver.menu->width - RGUI_TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define RGUI_TERM_HEIGHT (((driver.menu->height - RGUI_TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

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
#if defined(GEKKO) || defined(PSP)
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
#if defined(GEKKO) || defined(PSP)
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

static void blit_line(int x, int y, const char *message, bool green)
{
   int j, i;

   if (!driver.menu)
      return;

   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (driver.menu->font[FONT_OFFSET
                  ((unsigned char)*message) + offset] & rem);

            if (col)
            {
               driver.menu->frame_buf[(y + j) *
                  (driver.menu->frame_buf_pitch >> 1) + (x + i)] = green ?
#if defined(GEKKO)|| defined(PSP)
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

static void init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));
   menu->alloc_font = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu->font = font;
}

static bool rguidisp_init_font(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;

   if (font_bmp_buf)
      init_font(menu, font_bmp_buf);
   else if (font_bin_buf)
      menu->font = font_bin_buf;
   else
      return false;

   return true;
}

static void rgui_render_background(void)
{
   if (!driver.menu)
      return;

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         0, 0, driver.menu->width, driver.menu->height, gray_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         5, 5, driver.menu->width - 10, 5, green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         5, driver.menu->height - 10, driver.menu->width - 10, 5,
         green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         5, 5, 5, driver.menu->height - 10, green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         driver.menu->width - 10, 5, 5, driver.menu->height - 10,
         green_filler);
}

static void rgui_render_messagebox(const char *message)
{
   size_t i;

   if (!driver.menu || !message || !*message)
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
      if (msglen > RGUI_TERM_WIDTH)
      {
         msg[RGUI_TERM_WIDTH - 2] = '.';
         msg[RGUI_TERM_WIDTH - 1] = '.';
         msg[RGUI_TERM_WIDTH - 0] = '.';
         msg[RGUI_TERM_WIDTH + 1] = '\0';
         msglen = RGUI_TERM_WIDTH;
      }

      unsigned line_width = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
      width = max(width, line_width);
      glyphs_width = max(glyphs_width, msglen);
   }

   unsigned height = FONT_HEIGHT_STRIDE * list->size + 6 + 10;
   int x = (driver.menu->width - width) / 2;
   int y = (driver.menu->height - height) / 2;

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         x + 5, y + 5, width - 10, height - 10, gray_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         x, y, width - 5, 5, green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         x + width - 5, y, 5, height - 5, green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         x + 5, y + height - 5, width - 5, 5, green_filler);

   fill_rect(driver.menu->frame_buf, driver.menu->frame_buf_pitch,
         x, y + 5, 5, height - 5, green_filler);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;
      int offset_x = FONT_WIDTH_STRIDE * (glyphs_width - strlen(msg)) / 2;
      int offset_y = FONT_HEIGHT_STRIDE * i;
      blit_line(x + 8 + offset_x, y + 8 + offset_y, msg, false);
   }

   string_list_free(list);
}

static void rgui_render(void)
{
   size_t begin = 0;
   size_t end;

   if (driver.menu->need_refresh &&
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !driver.menu->msg_force)
      return;

   if (driver.menu->selection_ptr >= RGUI_TERM_HEIGHT / 2)
      begin = driver.menu->selection_ptr - RGUI_TERM_HEIGHT / 2;
   end   = (driver.menu->selection_ptr + RGUI_TERM_HEIGHT <=
         file_list_get_size(driver.menu->selection_buf)) ?
      driver.menu->selection_ptr + RGUI_TERM_HEIGHT :
      file_list_get_size(driver.menu->selection_buf);

   /* Do not scroll if all items are visible. */
   if (file_list_get_size(driver.menu->selection_buf) <= RGUI_TERM_HEIGHT)
      begin = 0;

   if (end - begin > RGUI_TERM_HEIGHT)
      end = begin + RGUI_TERM_HEIGHT;

   rgui_render_background();

   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   unsigned menu_type_is = 0;
   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   if (driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->type_is)
      menu_type_is = driver.menu_ctx->backend->type_is(label, menu_type);

#if 0
   RARCH_LOG("Dir is: %s\n", label);
#endif

   get_title(label, dir, menu_type, menu_type_is,
         title, sizeof(title));

   char title_buf[256];
   menu_ticker_line(title_buf, RGUI_TERM_WIDTH - 3,
         g_extern.frame_count / 15, title, true);
   blit_line(RGUI_TERM_START_X + 15, 15, title_buf, true);

   char title_msg[64];
   const char *core_name = driver.menu->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = driver.menu->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
   blit_line(RGUI_TERM_START_X + 15, (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) +
         RGUI_TERM_START_Y + 2, title_msg, true);

   unsigned x, y;
   size_t i;

   x = RGUI_TERM_START_X;
   y = RGUI_TERM_START_Y;

   for (i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      char message[256], type_str[256];
      const char *path = NULL;
      const char *entry_label = NULL;
      unsigned type = 0;
      file_list_get_at_offset(driver.menu->selection_buf, i, &path,
            &entry_label, &type);
      rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
            setting_data_get_list(),
            driver.menu->selection_buf->list[i].label);

      unsigned w = 21; 
      (void)setting;

      if (!strcmp(label, "performance_counters"))
         w = 28;

#ifdef HAVE_SHADER_MANAGER
      if (type >= MENU_SETTINGS_SHADER_FILTER &&
            type <= MENU_SETTINGS_SHADER_LAST)
      {
         if (
               type == MENU_SETTINGS_SHADER_PRESET ||
               type == MENU_SETTINGS_SHADER_PARAMETERS ||
               type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == MENU_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else if (driver.menu_ctx && driver.menu_ctx->backend &&
               driver.menu_ctx->backend->shader_manager_get_str)
         {
            if (type >= MENU_SETTINGS_SHADER_PARAMETER_0 &&
                  type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
               driver.menu_ctx->backend->shader_manager_get_str(
                     driver.menu->parameter_shader, type_str,
                     sizeof(type_str), type);
            else
               driver.menu_ctx->backend->shader_manager_get_str(
                     driver.menu->shader, type_str, sizeof(type_str), type);
         }
      }
      else
#endif
      if (type == MENU_FILE_CORE)
      {
         strlcpy(type_str, "(CORE)", sizeof(type_str));
         file_list_get_alt_at_offset(driver.menu->selection_buf, i, &path);
         w = 6;
      }
      else if (type == MENU_FILE_PLAIN)
      {
         strlcpy(type_str, "(FILE)", sizeof(type_str));
         w = 6;
      }
      else if (type == MENU_FILE_USE_DIRECTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type == MENU_FILE_DIRECTORY)
      {
         strlcpy(type_str, "(DIR)", sizeof(type_str));
         type = MENU_FILE_DIRECTORY;
         w = 5;
      }
      else if (type >= MENU_SETTINGS_CORE_OPTION_START)
         strlcpy(
               type_str,
               core_option_get_val(g_extern.system.core_options,
                  type - MENU_SETTINGS_CORE_OPTION_START),
               sizeof(type_str));
      else if (type == MENU_FILE_SWITCH)
      {
         strlcpy(type_str, "...", sizeof(type_str));
         w = 5;
      }
      else if (driver.menu_ctx && driver.menu_ctx->backend &&
            driver.menu_ctx->backend->setting_set_label)
         driver.menu_ctx->backend->setting_set_label(type_str,
               sizeof(type_str), &w, type, i);

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == driver.menu->selection_ptr;

      menu_ticker_line(entry_title_buf, RGUI_TERM_WIDTH - (w + 1 + 2),
            g_extern.frame_count / 15, path, selected);
      menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15,
            type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            RGUI_TERM_WIDTH - (w + 1 + 2), RGUI_TERM_WIDTH - (w + 1 + 2),
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
      char msg[1024];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      rgui_render_messagebox(msg);
   }
}

static void *rgui_init(void)
{
   uint16_t *framebuf = menu_framebuf;
   size_t framebuf_pitch;

   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu->frame_buf = framebuf;
   menu->width = 320;
   menu->height = 240;
   framebuf_pitch = menu->width * sizeof(uint16_t);

   menu->frame_buf_pitch = framebuf_pitch;

   bool ret = rguidisp_init_font(menu);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      /* TODO - should be refactored - perhaps don't do rarch_fail but instead
       * exit program */
      rarch_main_command(RARCH_CMD_QUIT_RETROARCH);
      return NULL;
   }

   return menu;
}

static void rgui_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->alloc_font)
      free((uint8_t*)menu->font);
}

static int rgui_input_postprocess(uint64_t old_state)
{
   (void)old_state;

   if ((driver.menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      rarch_main_command(RARCH_CMD_RESUME);
      return -1;
   }

   return 0;
}

void rgui_set_texture(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (driver.video_data && driver.video_poke &&
         driver.video_poke->set_texture_frame)
      driver.video_poke->set_texture_frame(driver.video_data,
            menu_framebuf, false, menu->width, menu->height, 1.0f);
}

const menu_ctx_driver_t menu_ctx_rgui = {
   rgui_set_texture,
   rgui_render_messagebox,
   rgui_render,
   NULL,
   rgui_init,
   rgui_free,
   NULL,
   NULL,
   NULL,
   NULL,
   rgui_input_postprocess,
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
   &menu_ctx_backend_common,
   "rgui",
};
