/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#if _WIN32_WINNT <= 0x0400
/* Windows versions below 98 do not support multiple monitors, so fake it */
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>
#endif

#endif

#include <boolean.h>
#include <retro_common_api.h>
#include "../../driver.h"
#include "../video_driver.h"

#ifdef _XBOX
#include "../../defines/xdk_defines.h"
#else
#include "../../ui/drivers/ui_win32_resource.h"
#include "../../ui/drivers/ui_win32.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _XBOX
extern unsigned g_resize_width;
extern unsigned g_resize_height;
extern bool g_inited;
extern bool g_restore_desktop;
extern ui_window_win32_t main_window;

void win32_monitor_get_info(void);

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id);

void create_graphics_context(HWND hwnd, bool *quit);

void create_gdi_context(HWND hwnd, bool *quit);

bool gdi_has_menu_frame(void);

bool win32_shader_dlg_init(void);
void shader_dlg_show(HWND parent_hwnd);
void shader_dlg_params_reload(void);
#endif

void win32_monitor_from_window(void);

void win32_monitor_init(void);

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen);

#ifndef _XBOX
RETRO_BEGIN_DECLS

bool win32_window_init(WNDCLASSEX *wndclass, bool fullscreen, const char *class_name);

RETRO_END_DECLS
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

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data);

#ifndef _XBOX
void win32_set_style(MONITORINFOEX *current_mon, HMONITOR *hm_to_use,
	unsigned *width, unsigned *height, bool fullscreen, bool windowed_full,
	RECT *rect, RECT *mon_rect, DWORD *style);
#endif

void win32_get_video_output_size(
      unsigned *width, unsigned *height);

void win32_get_video_output_prev(
      unsigned *width, unsigned *height);

void win32_get_video_output_next(
      unsigned *width, unsigned *height);

void win32_window_reset(void);

void win32_destroy_window(void);

#if defined(HAVE_D3D9) || defined(HAVE_D3D8)
LRESULT CALLBACK WndProcD3D(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_VULKAN)
LRESULT CALLBACK WndProcGL(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

LRESULT CALLBACK WndProcGDI(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);

#ifdef _XBOX
BOOL IsIconic(HWND hwnd);
#endif

#ifdef __cplusplus
}
#endif

#endif
