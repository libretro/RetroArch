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

#include "menu_animation.h"
#include "../gfx/font_renderer_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_framebuf
{
   uint16_t *data;
   unsigned width;
   unsigned height;
   size_t pitch;
   bool dirty;
} menu_framebuf_t;

typedef struct menu_display
{
   bool msg_force;

   menu_framebuf_t frame_buf;
   menu_animation_t *animation;

   struct
   {
      void *buf;
      int size;

      const uint8_t *framebuf;
      bool alloc_framebuf;
   } font;

   unsigned header_height;

   /* This buffer can be used to display generic OK messages to the user.
    * Fill it and call
    * menu_list_push(driver->menu->menu_stack, "", "message", 0, 0);
    */
   char message_contents[PATH_MAX_LENGTH];

   msg_queue_t *msg_queue;
} menu_display_t;

menu_display_t  *menu_display_get_ptr(void);

menu_framebuf_t *menu_display_fb_get_ptr(void);

void menu_display_fb(void);

void menu_display_fb_set_dirty(void);

void menu_display_fb_unset_dirty(void);

void menu_display_free(void *data);

bool menu_display_init(void *data);

bool menu_display_update_pending(void);

float menu_display_get_dpi(void);

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size);

bool menu_display_font_bind_block(void *data,
      const struct font_renderer *font_driver, void *userdata);

bool menu_display_font_flush_block(void *data,
      const struct font_renderer *font_driver);

bool menu_display_init_main_font(void *data,
      const char *font_path, float font_size);

void menu_display_free_main_font(void *data);

void menu_display_set_viewport(void);

void menu_display_unset_viewport(void);

void menu_display_timedate(char *s, size_t len, unsigned time_mode);

#ifdef __cplusplus
}
#endif

#endif
