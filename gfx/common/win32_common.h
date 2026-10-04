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
extern HACCEL window_accelerators;

void win32_monitor_get_info(void);

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id);

int win32_change_display_settings(const char *str, void *devmode_data,
      unsigned flags);

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
      unsigned dims,
      bool fullscreen);

/* Takes the existing window between windowed and borderless fullscreen
 * without destroying it. False if there is no window, or fullscreen is
 * the exclusive kind. Call on the thread that owns the window. */
bool win32_window_set_fullscreen(unsigned dims, bool fullscreen);

/* Whether a borderless fullscreen toggle is to restyle the window in
 * place (win32_window_set_fullscreen()) rather than restart the
 * drivers: yes, unless RETROARCH_FULLSCREEN_IN_PLACE=0 is in the
 * environment. For a driver or context to offer
 * GFX_CTX_FLAGS_FULLSCREEN_IN_PLACE by. */
bool win32_fullscreen_in_place(void);

bool win32_suspend_screensaver(void *data, bool enable);

void win32_show_cursor(void *data, bool state);

HWND win32_get_window(void);

bool win32_get_client_rect(RECT* rect);

bool is_running_on_xbox(void);

bool win32_has_focus(void *data);

/* When the compositor last saw a vertical blank, on the QPC clock
 * cpu_features_get_time_usec() keeps here, from
 * DwmGetCompositionTimingInfo; 0 when DWM cannot say (pre-Vista, or
 * composition off). A reported timestamp, not a scanline estimate, and
 * valid for any presentation that goes through the compositor - which
 * on current Windows is every windowed and borderless swapchain, GL
 * and Vulkan alike. A context that bypasses the compositor gets a
 * timestamp that stops advancing, which callers already treat as
 * absent. dwmapi is resolved at runtime and not linked. */
retro_time_t win32_dwm_last_vblank_time(void);

#ifdef HAVE_CLIP_WINDOW
void win32_clip_window(bool grab);
#endif

#if !defined(_XBOX)
/* Size/move and menu-loop handling for any window whose wndproc runs on
 * the run loop's thread: content pauses, audio is stopped cleanly and a
 * timer re-presents the last frame. See win32_common.c. */
#define WIN32_SIZEMOVE_TIMER_ID 0x5241
void win32_sizemove_enter(HWND hwnd);
void win32_sizemove_exit(HWND hwnd);
void win32_sizemove_tick(void);
void win32_sizemove_abort(void);

/* HID hotplug settle timer. WM_DEVICECHANGE arrives once per HID
 * interface, so one composite device (a headset with volume keys, a
 * keyboard with a media collection) sends a burst, and each event
 * used to reinitialise the joypad driver - one full DirectInput
 * enumeration per event, back to back. The input driver arms this
 * timer instead; re-arming restarts it, so the reinit runs once,
 * WIN32_HOTPLUG_SETTLE_MS after the last event of the burst. */
#define WIN32_HOTPLUG_TIMER_ID  0x5242
#define WIN32_HOTPLUG_SETTLE_MS 250
void win32_hotplug_arm(void);
bool win32_hotplug_due(void);
#endif

void win32_check_window(void *data,
      bool *quit,
      bool *resize, unsigned *dims);

/* The menu bar kept while the window has none is dropped, so that the
 * next windowed window gets one built anew: for when what is in it
 * has changed (the language). */
void win32_menu_kept_drop(void);

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data);

void win32_window_reset(void);

void win32_destroy_window(void);

/* Leaves the window up for the video driver that comes next instead of
 * destroying it; win32_set_video_mode() then takes it rather than make
 * one. False if it cannot be left - the caller destroys it as before. */
bool win32_window_keep(void);

/* The size the window's client area really is, which is not always
 * the size win32_set_video_mode() was asked for; @dims with no window.
 * For the swap chain a driver makes on it. */
unsigned win32_window_client_dims(unsigned dims);

/* A window left up that the next driver did not take goes. Called
 * once that driver is up, or has failed to come up. */
void win32_window_release_kept(void);

/* Called after win32_window_proc_setup() by a Direct3D driver whose
 * window may be left up for the next driver of the same tag, and taken
 * by it: see win32_window_keep() in win32_common.c for what the driver
 * undertakes by it. */
void win32_window_tag(const char *tag);

/* True if the window win32_set_video_mode() gave this driver is one
 * the last driver left up. */
bool win32_window_was_taken(void);

/* Destroys the window and makes a new one, for a driver that cannot
 * use the one it took. */
bool win32_window_remake(void *data, unsigned dims, bool fullscreen);

uint8_t win32_get_flags(void);

/* Re-read the synchronous keyboard state and publish it as a
 * RETROKMOD_* mask. Only valid on the thread owning the main window's
 * message queue; use win32_get_keyboard_mods() everywhere else. */
uint16_t win32_update_keyboard_mods(void);

/* Returns the last mask published by win32_update_keyboard_mods().
 * Safe from any thread. */
uint16_t win32_get_keyboard_mods(void);

/* The video family a window belongs to: what creating it sets up. */
enum win32_window_family
{
   WIN32_WINDOW_D3D = 0,
   WIN32_WINDOW_WGL,
   WIN32_WINDOW_VULKAN,
   WIN32_WINDOW_GDI
};

/* The window procedure, for every video driver. A driver registers it
 * for its window class and says which family the window is before
 * creating it. */
LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam);
void win32_window_proc_setup(enum win32_window_family family);

#ifdef _XBOX
BOOL IsIconic(HWND hwnd);
#endif

void win32_setup_pixel_format(HDC hdc, bool supports_gl);

/* True when win32_setup_pixel_format selected an FP16 (scRGB) backbuffer
 * for HDR output.  Always false unless HDR was requested, the display is
 * in HDR mode, and the WGL float-pixel-format path succeeded; every
 * failure falls back to the ordinary 8-bit pixel format, so pre-HDR
 * setups behave exactly as before. */
bool win32_backbuffer_is_scrgb(void);

/* True when the display the window is on is currently in HDR mode.
 * Probed through the dynamically-loaded DXGI helper; reports false in
 * builds without a D3D driver or on systems without HDR. As a side
 * effect of the underlying DXGI check, the video display HDR support
 * flags are updated (set when supported; cleared -- and the HDR mode
 * setting forced off -- when not). */
bool win32_display_hdr_active(HWND hwnd);

#if !defined(__WINRT__)
/* Programmatic replacement for the menu, dialog, accelerator,
 * and manifest resources that were in media/rarch.rc.
 *
 * The icon resource remains in rarch.rc (compiled by windres/rc)
 * so that the .exe has an embedded icon visible in Explorer.
 * Everything else is created at runtime via Win32 API calls. */

/* Mark the process as DPI-aware (replaces the <dpiAware>true</dpiAware>
 * entry that used to live in media/rarch.manifest).
 *
 * MUST be called before any HWND is created — directly or transitively
 * (e.g. via CoInitialize or AllocConsole).  Call it at the very top of
 * rarch_main(), before anything else.  Safe to call on any Windows
 * version: falls back from SetProcessDpiAwareness (Win8.1+) to
 * SetProcessDPIAware (Vista/7), and is a no-op on older systems. */
void win32_apply_dpi_awareness(void);

/* Call once before creating any windows.
 * Creates the accelerator table and other programmatic resources. */
void win32_resources_init(void);

/* Release resources created by win32_resources_init(). */
void win32_resources_free(void);

/* Return the accelerator table
 * (replaces LoadAccelerators + MAKEINTRESOURCE(IDR_ACCELERATOR1)). */
HACCEL win32_resources_get_accelerator(void);
#endif /* !__WINRT__ */

#ifdef HAVE_D3DKMT
#include <sdkddkver.h>
#if !defined(NTDDI_VERSION) || NTDDI_VERSION < 0x06000000
# undef NTDDI_VERSION
# define NTDDI_VERSION 0x06000000   /* NTDDI_LONGHORN / Vista */
#endif
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0600
#endif
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0)
#include <d3dkmthk.h>
typedef NTSTATUS(CALLBACK* D3DKMTOPENADAPTERFROMHDC)(D3DKMT_OPENADAPTERFROMHDC*);
static D3DKMTOPENADAPTERFROMHDC pD3DKMTOpenAdapterFromHdc;
typedef NTSTATUS(CALLBACK* D3DKMTGETSCANLINE)(D3DKMT_GETSCANLINE*);
static D3DKMTGETSCANLINE pD3DKMTGetScanLine;
typedef NTSTATUS(CALLBACK* D3DKMTWAITFORVERTICALBLANKEVENT)(D3DKMT_WAITFORVERTICALBLANKEVENT*);
static D3DKMTWAITFORVERTICALBLANKEVENT pD3DKMTWaitForVerticalBlankEvent;

typedef struct d3dkmt_adapter
{
   D3DKMT_GETSCANLINE sl;
   D3DKMT_WAITFORVERTICALBLANKEVENT vb;
} d3dkmt_adapter_t;

extern int d3dkmt_scanline_get(void);

/* Block until the display signals vertical blank. Returns false when
 * the entry point is unavailable or the wait fails, in which case the
 * caller has no anchor and must fall back to polling.
 *
 * Measured on a 4K120 panel: 0 intervals outside +-20%% of the median
 * across 499 samples, p1..p99 spread 30 us, period accurate to 0.02%%.
 * That makes it a usable phase reference; GetScanLine at ~223 us a call
 * is not. */
extern bool d3dkmt_wait_vblank(void);
#endif /* HAVE_D3DKMT */

RETRO_END_DECLS

#endif
