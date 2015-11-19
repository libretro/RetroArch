/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef WIN32_COMMON_H__
#define WIN32_COMMON_H__

#include <string.h>

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <boolean.h>
#include "../../driver.h"
#include "../video_context_driver.h"

#ifdef _XBOX
#include "../../defines/xdk_defines.h"
#endif

#ifndef _XBOX
#include "../../ui/drivers/ui_win32_resource.h"

extern unsigned g_resize_width;
extern unsigned g_resize_height;
extern bool g_inited;
extern bool g_restore_desktop;

LRESULT win32_handle_keyboard_event(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);

void win32_monitor_get_info(void);

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id);

void create_gl_context(HWND hwnd, bool *quit);
#endif

void win32_monitor_from_window(HWND data, bool destroy);

void win32_monitor_init(void);

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen);

bool win32_monitor_set_fullscreen(unsigned width,
      unsigned height, unsigned refresh, char *dev_name);

#ifndef _XBOX
extern "C" bool win32_window_init(WNDCLASSEX *wndclass, bool fullscreen, const char *class_name);
#endif

bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen);

bool win32_suppress_screensaver(void *data, bool enable);

bool win32_get_metrics(void *data,
	enum display_metric_types type, float *value);

void win32_show_cursor(bool state);

HWND win32_get_window(void);

bool win32_has_focus(void);

void win32_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height);

void win32_window_reset(void);

void win32_destroy_window(void);

#ifdef _XBOX
BOOL IsIconic(HWND hwnd);
#endif

#endif
