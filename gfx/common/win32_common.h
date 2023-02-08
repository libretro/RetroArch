/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <retro_environment.h>
#include "../../retroarch.h"

#ifndef _XBOX
#include "../../ui/drivers/ui_win32_resource.h"
#include "../../ui/drivers/ui_win32.h"

#if (defined(_MSC_VER) && (_MSC_VER >= 1400)) || defined(__MINGW32__)
#ifndef HAVE_CLIP_WINDOW
#define HAVE_CLIP_WINDOW
#endif
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* Windows 2000 and higher */

/* Supports taskbar */
#ifndef HAVE_TASKBAR
#define HAVE_TASKBAR
#endif

/* Supports window transparency */
#ifndef HAVE_WINDOW_TRANSP
#define HAVE_WINDOW_TRANSP
#endif

#endif

#endif

RETRO_BEGIN_DECLS

enum win32_common_flags
{
   WIN32_CMN_FLAG_QUIT            = (1 << 0),
   WIN32_CMN_FLAG_RESIZED         = (1 << 1),
   WIN32_CMN_FLAG_TASKBAR_CREATED = (1 << 2),
   WIN32_CMN_FLAG_RESTORE_DESKTOP = (1 << 3),
   WIN32_CMN_FLAG_INITED          = (1 << 4),
   WIN32_CMN_FLAG_SWAP_MOUSE_BTNS = (1 << 5)
};

extern uint8_t g_win32_flags;

#if !defined(_XBOX)
extern unsigned g_win32_resize_width;
extern unsigned g_win32_resize_height;
extern float g_win32_refresh_rate;
extern ui_window_win32_t main_window;

void win32_monitor_get_info(void);

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id);

int win32_change_display_settings(const char *str, void *devmode_data,
      unsigned flags);

void create_wgl_context(HWND hwnd, bool *quit);

#if defined(HAVE_VULKAN)
void create_vk_context(HWND hwnd, bool *quit);
#endif

#if defined(HAVE_GDI)
void create_gdi_context(HWND hwnd, bool *quit);
#endif

bool win32_get_video_output(DEVMODE *dm, int mode, size_t len);

#if !defined(__WINRT__)
bool win32_window_init(WNDCLASSEX *wndclass, bool fullscreen, const char *class_name);

void win32_set_style(MONITORINFOEX *current_mon, HMONITOR *hm_to_use,
      unsigned *width, unsigned *height, bool fullscreen, bool windowed_full,
      RECT *rect, RECT *mon_rect, DWORD *style);
#endif
void win32_monitor_from_window(void);
#endif

void win32_monitor_init(void);

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen);

bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen);

bool win32_suppress_screensaver(void *data, bool enable);

bool win32_get_metrics(void *data,
      enum display_metric_types type, float *value);

void win32_show_cursor(void *data, bool state);

HWND win32_get_window(void);

bool win32_get_client_rect(RECT* rect);

bool is_running_on_xbox(void);

bool win32_has_focus(void *data);

#ifdef HAVE_CLIP_WINDOW
void win32_clip_window(bool grab);
#endif

void win32_check_window(void *data,
      bool *quit,
      bool *resize, unsigned *width, unsigned *height);

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data);

void win32_get_video_output_size(
      unsigned *width, unsigned *height, char *desc, size_t desc_len);

void win32_get_video_output_prev(
      unsigned *width, unsigned *height);

void win32_get_video_output_next(
      unsigned *width, unsigned *height);

void win32_window_reset(void);

void win32_destroy_window(void);

uint8_t win32_get_flags(void);

float win32_get_refresh_rate(void *data);

#if defined(HAVE_D3D8) || defined(HAVE_D3D9) || defined (HAVE_D3D10) || defined (HAVE_D3D11) || defined (HAVE_D3D12)
LRESULT CALLBACK wnd_proc_d3d_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_d3d_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_d3d_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
LRESULT CALLBACK wnd_proc_wgl_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_wgl_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_wgl_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

#if defined(HAVE_VULKAN)
LRESULT CALLBACK wnd_proc_vk_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_vk_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_vk_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

#if defined(HAVE_GDI)
LRESULT CALLBACK wnd_proc_gdi_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_gdi_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wnd_proc_gdi_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
#endif

#ifdef _XBOX
BOOL IsIconic(HWND hwnd);
#endif

bool win32_load_content_from_gui(const char *szFilename);

void win32_setup_pixel_format(HDC hdc, bool supports_gl);

void win32_update_title(void);

RETRO_END_DECLS

#endif
