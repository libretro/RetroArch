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

#include "menu_driver.h"
#include "../gfx/video_thread_wrapper.h"
#include "../gfx/font_renderer_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void menu_display_fb(void);

void menu_display_free(menu_handle_t *menu);

bool menu_display_init(menu_handle_t *menu);

bool menu_display_update_pending(void);

float menu_display_get_dpi(menu_handle_t *menu);

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size);

bool menu_display_font_bind_block(menu_handle_t *menu,
      const struct font_renderer *font_driver, void *userdata);

bool menu_display_font_flush_block(menu_handle_t *menu,
      const struct font_renderer *font_driver);

/** Shortcuts to handle menu->font.buf */
bool menu_display_init_main_font(menu_handle_t *menu,
      const char *font_path, float font_size);
void menu_display_free_main_font(menu_handle_t *menu);

void menu_display_set_viewport(menu_handle_t *menu);

void menu_display_unset_viewport(menu_handle_t *menu);

#ifdef __cplusplus
}
#endif

#endif
