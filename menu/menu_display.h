/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __MENU_DISPLAY_H__
#define __MENU_DISPLAY_H__

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#include <queues/message_queue.h>

#include "../gfx/font_renderer_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

enum menu_display_ctl_state
{
   MENU_DISPLAY_CTL_SET_VIEWPORT = 0,
   MENU_DISPLAY_CTL_UNSET_VIEWPORT,
   MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_GET_DPI,
   MENU_DISPLAY_CTL_UPDATE_PENDING,
   MENU_DISPLAY_CTL_WIDTH,
   MENU_DISPLAY_CTL_HEIGHT,
   MENU_DISPLAY_CTL_HEADER_HEIGHT,
   MENU_DISPLAY_CTL_SET_WIDTH,
   MENU_DISPLAY_CTL_SET_HEIGHT,
   MENU_DISPLAY_CTL_FB_DATA,
   MENU_DISPLAY_CTL_SET_FB_DATA,
   MENU_DISPLAY_CTL_FB_PITCH,
   MENU_DISPLAY_CTL_SET_FB_PITCH,
   MENU_DISPLAY_CTL_LIBRETRO,
   MENU_DISPLAY_CTL_FONT_DATA_INIT,
   MENU_DISPLAY_CTL_SET_FONT_DATA_INIT
};

typedef struct menu_display
{
   bool msg_force;

   struct
   {
      void *buf;
      int size;

      const uint8_t *framebuf;
      bool alloc_framebuf;
   } font;

   unsigned header_height;

   msg_queue_t *msg_queue;
} menu_display_t;

menu_display_t  *menu_display_get_ptr(void);

void menu_display_free(void);

bool menu_display_init(void);

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size);

bool menu_display_font_bind_block(void *data,
      const struct font_renderer *font_driver, void *userdata);

bool menu_display_font_flush_block(void *data,
      const struct font_renderer *font_driver);

bool menu_display_init_main_font(void *data,
      const char *font_path, float font_size);

void menu_display_free_main_font(void);


bool menu_display_ctl(enum menu_display_ctl_state state, void *data);

void menu_display_timedate(char *s, size_t len, unsigned time_mode);

void menu_display_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush);

#ifdef __cplusplus
}
#endif

#endif
