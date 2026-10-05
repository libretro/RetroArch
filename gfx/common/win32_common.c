/*  RetroArch - A frontend for libretro.
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

#if !defined(_XBOX)

#define WIN32_LEAN_AND_MEAN

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 */
#endif

#if !defined(_MSC_VER) || _WIN32_WINNT >= 0x0601
#undef WINVER
#define WINVER 0x0601
#endif

#define IDI_ICON 1

#include <windows.h>
#ifndef _XBOX
/* win32_dwm_last_vblank_time() reads the compositor's last vblank
 * through DwmGetCompositionTimingInfo, resolved at runtime: dwmapi is
 * not linked, and its header is not included either, since the SDKs
 * the oldest MSVC job builds with do not have it. The structure below
 * is DWM_TIMING_INFO exactly as the SDK lays it out; only cbSize and
 * qpcVBlank are read, but the size must match for the call to accept
 * it, so every field is here. UNSIGNED_RATIO is two UINT32s, and the
 * SDK declares the whole thing byte-packed, which changes its size. */
#pragma pack(push, 1)
typedef struct
{
   UINT32 uiNumerator;
   UINT32 uiDenominator;
} win32_dwm_ratio_t;

typedef struct
{
   UINT32 cbSize;
   win32_dwm_ratio_t rateRefresh;
   ULONGLONG qpcRefreshPeriod;
   win32_dwm_ratio_t rateCompose;
   ULONGLONG qpcVBlank;
   ULONGLONG cRefresh;
   UINT cDXRefresh;
   ULONGLONG qpcCompose;
   ULONGLONG cFrame;
   UINT cDXPresent;
   ULONGLONG cRefreshFrame;
   ULONGLONG cFrameSubmitted;
   UINT cDXPresentSubmitted;
   ULONGLONG cFrameConfirmed;
   UINT cDXPresentConfirmed;
   ULONGLONG cRefreshConfirmed;
   UINT cDXRefreshConfirmed;
   ULONGLONG cFramesLate;
   UINT cFramesOutstanding;
   ULONGLONG cFrameDisplayed;
   ULONGLONG qpcFrameDisplayed;
   ULONGLONG cRefreshFrameDisplayed;
   ULONGLONG cFrameComplete;
   ULONGLONG qpcFrameComplete;
   ULONGLONG cFramePending;
   ULONGLONG qpcFramePending;
   ULONGLONG cFramesDisplayed;
   ULONGLONG cFramesComplete;
   ULONGLONG cFramesPending;
   ULONGLONG cFramesAvailable;
   ULONGLONG cFramesDropped;
   ULONGLONG cFramesMissed;
   ULONGLONG cRefreshNextDisplayed;
   ULONGLONG cRefreshNextPresented;
   ULONGLONG cRefreshesDisplayed;
   ULONGLONG cRefreshesPresented;
   ULONGLONG cRefreshStarted;
   ULONGLONG cPixelsReceived;
   ULONGLONG cPixelsDrawn;
   ULONGLONG cBuffersEmpty;
} win32_dwm_timing_info_t;
#pragma pack(pop)

/* Where the SDK header exists, the local layout is checked against it
 * at compile time; a mismatch is a build error, not a wrong vblank. */
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <dwmapi.h>
typedef char win32_dwm_timing_info_size_check[
   sizeof(win32_dwm_timing_info_t) == sizeof(DWM_TIMING_INFO) ? 1 : -1];
#endif
#endif
#endif /* !defined(_XBOX) */
#include <math.h>
#include <wchar.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#ifdef HAVE_DYLIB
#include <dynamic/dylib.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "win32_common.h"
#include "win32_msg_route.h"


#ifdef HAVE_GDI
#include "gdi_defines.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../video_driver.h"
#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif
#include "../../runloop.h"
#include "../../audio/audio_driver.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include "../../core_info.h"
#include "../../ui/drivers/ui_win32.h"

#if !defined(_XBOX)

#include <commdlg.h>
#include <dbt.h>
#include "../../input/input_keymaps.h"
#include <shellapi.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <encodings/utf.h>

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

/* For some reason this is missing from mingw winuser.h */
#ifndef EDS_ROTATEDMODE
#define EDS_ROTATEDMODE 4
#endif

/* These are defined in later SDKs, thus ifdeffed. */
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL                  0x20e
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif

#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE                0x0245
#endif

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN                  0x0246
#endif

#ifndef WM_POINTERUP
#define WM_POINTERUP                    0x0247
#endif

/* Win32 UI resource identifiers (formerly ui_win32_resource.h) */

const GUID GUID_DEVINTERFACE_HID = { 0x4d1e55b2, 0xf16f, 0x11Cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
static HDEVNOTIFY notification_handler;
#endif

/* A window left up across a driver restart (see win32_window_keep()),
 * and one of those on its way out: destroying it is not the program
 * being closed. */
static HWND  win32_kept_hwnd;
static DWORD win32_kept_tid;
static int   win32_kept_family;
static HWND  win32_retiring_hwnd;
/* Which driver's window it is, within a family whose drivers cannot
 * all use each other's (win32_window_tag()), and whether the window in
 * use was taken from the last driver rather than made. */
static const char *win32_wnd_tag;
static const char *win32_kept_tag;
static bool        win32_wnd_taken;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
static HDEVNOTIFY win32_kept_notification;
#endif

#ifdef HAVE_DINPUT
extern bool dinput_handle_message(void *dinput, UINT message,
      WPARAM wParam, LPARAM lParam);
#endif

#if !defined(_XBOX)
extern bool winraw_handle_message(UINT message,
      WPARAM wParam, LPARAM lParam);
#endif

HACCEL window_accelerators;

/* Power Request APIs */

#if !defined(_XBOX) && (_MSC_VER == 1310)
typedef struct _REASON_CONTEXT
{
   ULONG Version;
   DWORD Flags;
   union
   {
      struct
      {
         HMODULE LocalizedReasonModule;
         ULONG LocalizedreasonId;
         ULONG ReasonStringCount;
         LPWSTR *ReasonStrings;
      } Detailed;
      LPWSTR SimpleReasonString;
   } Reason;
} REASON_CONTEXT, *PREASON_CONTEXT;

typedef enum _POWER_REQUEST_TYPE
{
   PowerRequestDisplayRequired,
   PowerRequestSystemRequired,
   PowerRequestAwayModeRequired,
   PowerRequestExecutionRequired
} POWER_REQUEST_TYPE, *PPOWER_REQUEST_TYPE;

#define POWER_REQUEST_CONTEXT_VERSION         0
#define POWER_REQUEST_CONTEXT_SIMPLE_STRING   1
#define POWER_REQUEST_CONTEXT_DETAILED_STRING 2
#endif

#ifdef _WIN32_WINNT_WIN7
typedef REASON_CONTEXT POWER_REQUEST_CONTEXT, *PPOWER_REQUEST_CONTEXT, *LPPOWER_REQUEST_CONTEXT;
#endif

#ifndef MAX_MONITORS
#define MAX_MONITORS 9
#endif

#define MIN_WIDTH  320
#define MIN_HEIGHT 240

#ifdef HAVE_D3DKMT
static d3dkmt_adapter_t d3dkmt_adapter;

static void d3dkmt_init(void)
{
   if (!pD3DKMTOpenAdapterFromHdc)
   {
      unsigned d3dkmt_adapter_hAdapter = 0;
      unsigned d3dkmt_adapter_VidPnSourceId = 0;
      unsigned adapter_index = 0;
      DISPLAY_DEVICE add;

      add.cb = sizeof(add);

      pD3DKMTOpenAdapterFromHdc = (D3DKMTOPENADAPTERFROMHDC)
            GetProcAddress(GetModuleHandle("gdi32.dll"), "D3DKMTOpenAdapterFromHdc");
      pD3DKMTGetScanLine = (D3DKMTGETSCANLINE)
            GetProcAddress(GetModuleHandle("gdi32.dll"), "D3DKMTGetScanLine");
      /* Optional: only the phase anchor depends on it, and
       * d3dkmt_wait_vblank() reports its absence so the caller can
       * fall back. Not part of the guard below for that reason. */
      pD3DKMTWaitForVerticalBlankEvent = (D3DKMTWAITFORVERTICALBLANKEVENT)
            GetProcAddress(GetModuleHandle("gdi32.dll"),
                  "D3DKMTWaitForVerticalBlankEvent");

      /* Both exports are WDDM, so they are absent under XDDM - there
       * are no D3DKMT entry points in gdi32 before Vista at all.
       * pD3DKMTGetScanLine is null-checked at its two use sites;
       * pD3DKMTOpenAdapterFromHdc was not, and it is called inside the
       * loop below, so a missing export was a call through NULL on the
       * first display device. Leave the scanline state zeroed and let
       * d3dkmt_scanline_get() report -1, which
       * video_driver_scanline_after_frame() treats as unsupported. */
      if (!pD3DKMTOpenAdapterFromHdc || !pD3DKMTGetScanLine)
      {
         memset(&d3dkmt_adapter, 0, sizeof(d3dkmt_adapter_t));
         return;
      }

      while (EnumDisplayDevices(NULL, adapter_index, &add, 0))
      {
         HDC hdc = CreateDC(NULL, add.DeviceName, NULL, NULL);
         if (hdc != NULL)
         {
            D3DKMT_OPENADAPTERFROMHDC OpenAdapterData = {0};
            OpenAdapterData.hDc = hdc;
            if (pD3DKMTOpenAdapterFromHdc(&OpenAdapterData) == STATUS_SUCCESS)
            {
               d3dkmt_adapter_hAdapter      = OpenAdapterData.hAdapter;
               d3dkmt_adapter_VidPnSourceId = OpenAdapterData.VidPnSourceId;
            }
            DeleteDC(hdc);

            if (d3dkmt_adapter_hAdapter)
               break;
         }
         adapter_index++;
      }

      memset(&d3dkmt_adapter, 0, sizeof(d3dkmt_adapter_t));

      if (pD3DKMTGetScanLine)
      {
         D3DKMT_GETSCANLINE sl = {0};
         sl.hAdapter           = d3dkmt_adapter_hAdapter;
         sl.VidPnSourceId      = d3dkmt_adapter_VidPnSourceId;
         d3dkmt_adapter.sl     = sl;
      }

      {
         D3DKMT_WAITFORVERTICALBLANKEVENT vb = {0};
         vb.hAdapter           = d3dkmt_adapter_hAdapter;
         vb.VidPnSourceId      = d3dkmt_adapter_VidPnSourceId;
         /* hDevice is documented optional and is not needed to wait on
          * a VidPn source. */
         d3dkmt_adapter.vb     = vb;
      }
   }
}

bool d3dkmt_wait_vblank(void)
{
   if (!pD3DKMTWaitForVerticalBlankEvent || !d3dkmt_adapter.vb.hAdapter)
      return false;
   return (pD3DKMTWaitForVerticalBlankEvent(&d3dkmt_adapter.vb)
         == STATUS_SUCCESS);
}

int d3dkmt_scanline_get(void)
{
   if (pD3DKMTGetScanLine)
   {
      if (pD3DKMTGetScanLine(&d3dkmt_adapter.sl) == STATUS_SUCCESS)
         return d3dkmt_adapter.sl.ScanLine;
   }
   return -1;
}
#endif /* HAVE_D3DKMT */

typedef struct win32_common_state
{
   /* Where the window sits and how big its frame is, in
    * VIDEO_POS_PACK's and VIDEO_SCALE_PACK's layouts. The origin
    * goes negative on a display left of or above the primary one,
    * and only means anything once pos_set is up. */
   unsigned pos;
   unsigned pos_dims;
#ifdef HAVE_TASKBAR
   unsigned taskbar_message;
#endif
   unsigned monitor_count;
   /* Up once a position has come from the config or from the window
    * itself. Until then the window is created wherever Windows
    * decides to put it. */
   bool pos_set;
} win32_common_state_t;

/* Module-level state: resize dimensions, refresh rate, and main window handle.
 * These are written from the window message loop and read by the video driver. */
unsigned g_win32_resize_width       = 0;
unsigned g_win32_resize_height      = 0;
float g_win32_refresh_rate          = 0.0f;
ui_window_win32_t main_window;

/* Module-level flags byte (WIN32_CMN_FLAG_*). */
uint8_t g_win32_flags               = 0;
static HMONITOR win32_monitor_last;
static HMONITOR win32_monitor_all[MAX_MONITORS];

static win32_common_state_t win32_st =
{
   0,                   /* pos */
   0,                   /* pos_dims */
#ifdef HAVE_TASKBAR
   0,                   /* taskbar_message */
#endif
   0,                   /* monitor_count */
   false,               /* pos_set */
};

uint8_t win32_get_flags(void)
{
   return g_win32_flags;
}


static BOOL CALLBACK win32_monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   win32_common_state_t
      *g_win32           = (win32_common_state_t*)&win32_st;
   if (g_win32->monitor_count >= MAX_MONITORS)
      return FALSE;
   win32_monitor_all[g_win32->monitor_count++] = hMonitor;
   return TRUE;
}

#ifndef _XBOX
/* The foreground across a driver restart.
 *
 * A restart of the video driver - content loaded or closed, a setting
 * changed - destroys the window and makes another. When the window
 * goes, Windows hands the foreground to the next window down, which
 * is some other program's: the terminal RetroArch was started from,
 * say. The new window then asks for the foreground and is refused,
 * because Windows only lets the foreground process, or the one that
 * got the last keyboard or mouse input, take it - and with the content
 * chosen on a controller, that last input was the Enter typed in the
 * terminal. So the new window came up behind the terminal.
 *
 * If the window being destroyed is the foreground window, that is
 * noted, and the process gives itself leave to take the foreground
 * (AllowSetForegroundWindow(), which a process may call for itself
 * while it still has the foreground). The window made next asks for
 * the foreground as it always did; if it is still refused - the leave
 * is withdrawn by any keyboard or mouse input in between - it joins
 * the input queue of whichever thread has the foreground for the one
 * call, which is the way the refusal is got round. Only for a window
 * that replaces one that had the foreground a moment before: a window
 * that did not have it does not take it. */
static DWORD win32_had_foreground_at;

static void win32_foreground_note(HWND hwnd)
{
   typedef BOOL (WINAPI *allow_t)(DWORD);
   HMODULE user32;
   allow_t allow;

   win32_had_foreground_at = 0;
   if (!hwnd || GetForegroundWindow() != hwnd)
      return;
   win32_had_foreground_at = GetTickCount() | 1;
   /* by name: not in every user32 this is built for */
   if (     (user32 = GetModuleHandleA("user32.dll"))
         && (allow = (allow_t)GetProcAddress(user32,
               "AllowSetForegroundWindow")))
      allow(GetCurrentProcessId());
}

static void win32_foreground_take_back(HWND hwnd)
{
   HWND  fg;
   DWORD at = win32_had_foreground_at;

   win32_had_foreground_at = 0;
   if (!at || !hwnd || GetTickCount() - at > 15000)
      return;
   if (SetForegroundWindow(hwnd) || GetForegroundWindow() == hwnd)
      return;
   if ((fg = GetForegroundWindow()))
   {
      DWORD fg_tid = GetWindowThreadProcessId(fg, NULL);
      DWORD tid    = GetCurrentThreadId();
      if (fg_tid && fg_tid != tid && AttachThreadInput(tid, fg_tid, TRUE))
      {
         SetForegroundWindow(hwnd);
         AttachThreadInput(tid, fg_tid, FALSE);
      }
   }
}

void win32_monitor_from_window(void)
{
   ui_window_t *window       = NULL;

   win32_monitor_last        =
      MonitorFromWindow(main_window.hwnd, MONITOR_DEFAULTTONEAREST);
   win32_foreground_note(main_window.hwnd);

   window = (ui_window_t*)ui_companion_driver_get_window_ptr();

   if (window)
      window->destroy(&main_window);
}
#endif

int win32_change_display_settings(const char *str, void *devmode_data,
      unsigned flags)
{
#if _WIN32_WINDOWS >= 0x0410 || _WIN32_WINNT >= 0x0410
   /* Windows 98 and later codepath */
   return ChangeDisplaySettingsEx(str, (DEVMODE*)devmode_data,
         NULL, flags, NULL);
#else
   /* Windows 95 / NT codepath */
   return ChangeDisplaySettings((DEVMODE*)devmode_data, flags);
#endif
}

void win32_monitor_get_info(void)
{
   MONITORINFOEX current_mon;

   memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);

   GetMonitorInfo(win32_monitor_last, (LPMONITORINFO)&current_mon);

   win32_change_display_settings(current_mon.szDevice, NULL, 0);
}

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id)
{
   unsigned i;
   settings_t *settings  = config_get_ptr();
   MONITORINFOEX *mon    = (MONITORINFOEX*)data;
   HMONITOR *hm_to_use   = (HMONITOR*)hm_data;
   /* Reached from the display server's teardown as well as from
    * window setup, and the settings are gone by the end of a
    * shutdown: without one, the monitor the window is on is the
    * only answer there is. */
   unsigned fs_monitor   = settings ? settings->uints.video_monitor_index : 0;
   win32_common_state_t
      *g_win32           = (win32_common_state_t*)&win32_st;

   if (!win32_monitor_last)
      win32_monitor_last = MonitorFromWindow(GetDesktopWindow(),
            MONITOR_DEFAULTTONEAREST);

   *hm_to_use            = win32_monitor_last;

   if (fs_monitor && fs_monitor <= g_win32->monitor_count
         && win32_monitor_all[fs_monitor - 1])
   {
      *hm_to_use = win32_monitor_all[fs_monitor - 1];
      *mon_id    = fs_monitor - 1;
   }
   else
   {
      for (i = 0; i < g_win32->monitor_count; i++)
      {
         if (win32_monitor_all[i] != *hm_to_use)
            continue;

         *mon_id = i;
         break;
      }
   }

   if (*hm_to_use)
   {
      memset(mon, 0, sizeof(*mon));
      mon->cbSize = sizeof(MONITORINFOEX);

      GetMonitorInfo(*hm_to_use, (LPMONITORINFO)mon);
   }
}

void win32_get_video_size(void *data,
      unsigned *dims)
{
   HWND         window     = win32_get_window();

   if (window)
   {
      *dims = VIDEO_SCALE_PACK(g_win32_resize_width, g_win32_resize_height);
   }
   else
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id      = 0;
      HMONITOR hm_to_use   = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect             = current_mon.rcMonitor;
      *dims = VIDEO_SCALE_PACK(mon_rect.right - mon_rect.left,
            mon_rect.bottom - mon_rect.top);
   }
}



static void win32_resize_after_display_change(HWND hwnd, HMONITOR monitor)
{
   MONITORINFO info;
   memset(&info, 0, sizeof(info));
   info.cbSize = sizeof(info);
   if (GetMonitorInfo(monitor, &info))
      SetWindowPos(hwnd, 0, 0, 0,
            info.rcMonitor.right  - info.rcMonitor.left,
            info.rcMonitor.bottom - info.rcMonitor.top,
            SWP_NOMOVE);
}



static void win32_save_position(void)
{
   RECT rect;
   WINDOWPLACEMENT placement;
   win32_common_state_t *g_win32     = (win32_common_state_t*)&win32_st;
   settings_t *settings              = config_get_ptr();
   bool window_save_positions        = settings->bools.video_window_save_positions;

   placement.length                  = sizeof(placement);
   placement.flags                   = 0;
   placement.showCmd                 = 0;
   placement.ptMinPosition.x         = 0;
   placement.ptMinPosition.y         = 0;
   placement.ptMaxPosition.x         = 0;
   placement.ptMaxPosition.y         = 0;
   placement.rcNormalPosition.left   = 0;
   placement.rcNormalPosition.top    = 0;
   placement.rcNormalPosition.right  = 0;
   placement.rcNormalPosition.bottom = 0;

   /* If SETTINGS_FLG_SKIP_WINDOW_POSITIONS is set, it means we've
    * just unloaded an override that had fullscreen mode
    * enabled while we have windowed mode set globally,
    * in this case we skip the following blocks to not
    * end up with fullscreen size and position. */
   if (!(settings->flags & SETTINGS_FLG_SKIP_WINDOW_POSITIONS))
   {
      if (GetWindowPlacement(main_window.hwnd, &placement))
      {
         g_win32->pos        = VIDEO_POS_PACK(
               placement.rcNormalPosition.left,
               placement.rcNormalPosition.top);
         g_win32->pos_set    = true;
      }

      if (GetWindowRect(main_window.hwnd, &rect))
      {
         g_win32->pos_dims   = VIDEO_SCALE_PACK(
               rect.right  - rect.left,
               rect.bottom - rect.top);
      }
   }
   else
      settings->flags &= ~SETTINGS_FLG_SKIP_WINDOW_POSITIONS;

   if (window_save_positions)
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      uint32_t video_st_flags        = (uint32_t)retro_atomic_load_relaxed_int(&video_st->flags);
      bool video_fullscreen          = settings->bools.video_fullscreen;

      if (     !video_fullscreen
            && !(video_st_flags & VIDEO_FLAG_FORCE_FULLSCREEN)
            && !(video_st_flags & VIDEO_FLAG_IS_SWITCHING_DISPLAY_MODE))
      {
         bool ui_menubar_enable                     = settings->bools.ui_menubar_enable;
         bool window_show_decor                     = settings->bools.video_window_show_decorations;
         unsigned win_w                             =
               VIDEO_SCALE_W(g_win32->pos_dims);
         unsigned win_h                             =
               VIDEO_SCALE_H(g_win32->pos_dims);
         settings->uints.window_position_pos        = g_win32->pos;
         /* The frame the window reports includes whatever chrome it
          * is wearing; the setting holds the client area, so take the
          * chrome off both axes before the pair is stored. */
         if (window_show_decor)
         {
            int border_thickness                    = GetSystemMetrics(SM_CXSIZEFRAME);
            int title_bar_height                    = GetSystemMetrics(SM_CYCAPTION);
            win_w                                  -= border_thickness * 2;
            win_h                                  -= border_thickness * 2;
            win_h                                  -= title_bar_height;
         }
         if (ui_menubar_enable)
         {
            int menu_bar_height   = GetSystemMetrics(SM_CYMENU);
            win_h                                  -= menu_bar_height;
         }
         settings->uints.window_position_dims       =
               VIDEO_SCALE_PACK(win_w, win_h);
      }
   }
}

/* Get minimum window size for running core. */
static void win32_get_av_info_geometry(unsigned *width, unsigned *height)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();

   /* Don't bother while fast-forwarding. */
   if (!video_st || runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
      return;

   if (video_st->av_info.geometry.aspect_ratio > 0)
      *width                      = roundf(
              video_st->av_info.geometry.base_height
            * video_st->av_info.geometry.aspect_ratio);
   else
      *width                      = video_st->av_info.geometry.base_width;

   *height                        = video_st->av_info.geometry.base_height;
}

/* Published RETROKMOD_* mask, written only by
 * win32_update_keyboard_mods() from the thread that owns the main
 * window's message queue, read from anywhere. */
static retro_atomic_int_t win32_kb_mods;

/* GetKeyState and GetKeyboardState read the same per-thread
 * synchronous key table, but only GetKeyboardState exposes it in its
 * documented form: a 256-byte array with 0x80 for down and 0x01 for
 * toggled. GetKeyState repacks those bits into a SHORT in which only
 * bit 15 is contractual.
 *
 * Every implementation happens to also set bit 7 on the down value,
 * so the 0x80 mask this code used to apply did work - user.exe
 * 4.10.2222 (Windows 98 SE, 16-bit) builds it with mov bx,0xff80,
 * user32 5.1.2600.2180 (XP SP2, x86) with or edi,0xff80, and user32
 * 10.0.26100.7462 (24H2, x64) with or ax,0xff80. Three unrelated
 * implementations, twenty-six years, no shared code, same constant.
 * It is still undocumented, and GetAsyncKeyState in those same three
 * binaries returns 0x8000 with bit 7 clear, so the mask is not even
 * consistent between the two calls. Use the documented byte form.
 *
 * One call is also cheaper, and more so the older the target. On NT
 * the GetKeyState cache covers virtual-key codes below 0x20 only, so
 * VK_NUMLOCK, VK_SCROLL, VK_LWIN and VK_RWIN each took an
 * unconditional kernel transition per key. On 9x user32 does not
 * implement either function: both are four-byte stubs that select a
 * thunk ordinal and jump into 16-bit user.exe through FT_Thunk, so
 * every query was a 32->16 transition. There GetKeyboardState is a
 * flat 256-byte copy of the table in its native layout while
 * GetKeyState is the call doing extra work to repack it.
 *
 * The table is per-thread and is only advanced as that thread
 * dispatches keyboard messages, so this must run on the thread owning
 * the main window. Every caller below is a window procedure for that
 * window, or window creation on the same thread. Everyone else reads
 * the published value through win32_get_keyboard_mods(). */
uint16_t win32_update_keyboard_mods(void)
{
   BYTE ks[256];
   uint16_t mod = 0;

   if (!GetKeyboardState(ks))
      return (uint16_t)retro_atomic_load_acquire_int(&win32_kb_mods);

   if (ks[VK_SHIFT]   & 0x80)
      mod |= RETROKMOD_SHIFT;
   if (ks[VK_CONTROL] & 0x80)
      mod |= RETROKMOD_CTRL;
   if (ks[VK_MENU]    & 0x80)
      mod |= RETROKMOD_ALT;
   if (ks[VK_CAPITAL] & 0x01)
      mod |= RETROKMOD_CAPSLOCK;
   if (ks[VK_SCROLL]  & 0x01)
      mod |= RETROKMOD_SCROLLOCK;
   if (ks[VK_NUMLOCK] & 0x01)
      mod |= RETROKMOD_NUMLOCK;
   if ((ks[VK_LWIN] | ks[VK_RWIN]) & 0x80)
      mod |= RETROKMOD_META;

   retro_atomic_store_release_int(&win32_kb_mods, (int)mod);
   return mod;
}

uint16_t win32_get_keyboard_mods(void)
{
   return (uint16_t)retro_atomic_load_acquire_int(&win32_kb_mods);
}

#if !defined(_XBOX)
#ifndef WM_ENTERSIZEMOVE
#define WM_ENTERSIZEMOVE 0x0231
#endif
#ifndef WM_EXITSIZEMOVE
#define WM_EXITSIZEMOVE  0x0232
#endif
#ifndef WM_ENTERMENULOOP
#define WM_ENTERMENULOOP 0x0211
#endif
#ifndef WM_EXITMENULOOP
#define WM_EXITMENULOOP  0x0212
#endif

/* Title-bar drags, border resizes and menu bars run a modal loop inside
 * DefWindowProc that does not return until the user lets go. With
 * non-threaded video the run loop is on this thread, so content pauses
 * for the duration - that is the norm for a windowed game and is not
 * changed here. What is done: the audio driver is stopped so the sink
 * does not underrun and pop, and a plain timer keeps the last frame on
 * screen at the current window size. Nothing in here runs the run
 * loop, the menu, input or the task queue; running those nested inside
 * a captured-mouse modal loop is what 91920289 did and why it was
 * reverted.
 *
 * Size/move and menu loops can nest (system menu opened while sizing):
 * arm on the first entry, disarm on the last exit. One timer for the
 * process; the window that armed it owns it, so the companion and the
 * main window never kill each other's. */
#define WIN32_SIZEMOVE_TIMER_ID 0x5241

static uint8_t win32_sizemove_depth;
static bool    win32_sizemove_stopped_audio;
/* The routed window changed size since the last present. A move never
 * invalidates the client area - the compositor keeps the last buffer -
 * so a plain drag presents nothing at all. */
static bool    win32_sizemove_dirty;
static HWND    win32_sizemove_timer_hwnd;

void win32_sizemove_enter(HWND hwnd)
{
   if (win32_sizemove_depth++)
      return;
   /* The window lives on the video thread; the run loop is elsewhere
    * and keeps going on its own. */
   if (video_driver_is_threaded())
      return;

   /* A user who pressed P already stopped the driver and must not get
    * it restarted on release. audio_driver_stop() returns false when
    * the driver is not alive, so the latch is a real transition. */
   win32_sizemove_stopped_audio = false;
   if (!(runloop_state_get_ptr()->flags & RUNLOOP_FLAG_PAUSED))
      win32_sizemove_stopped_audio = audio_driver_stop();

   win32_sizemove_dirty = false;
   if (SetTimer(hwnd, WIN32_SIZEMOVE_TIMER_ID, 16, NULL))
      win32_sizemove_timer_hwnd = hwnd;
}

void win32_sizemove_exit(HWND hwnd)
{
   (void)hwnd;
   if (!win32_sizemove_depth || --win32_sizemove_depth)
      return;
   if (video_driver_is_threaded())
      return;

   if (win32_sizemove_timer_hwnd)
   {
      KillTimer(win32_sizemove_timer_hwnd, WIN32_SIZEMOVE_TIMER_ID);
      win32_sizemove_timer_hwnd = NULL;
   }
   /* A failed start clears AUDIO_FLAG_ACTIVE for the session; say so. */
   if (win32_sizemove_stopped_audio && !audio_driver_start(false))
      RARCH_WARN("[Win32] Audio did not restart after a window size/move.\n");
   win32_sizemove_stopped_audio = false;
}

/* A routed window is going away mid-drag. No audio restart: the driver
 * may already be gone, and audio_driver_start() failing mutes the
 * session. */
void win32_sizemove_abort(void)
{
   if (win32_sizemove_timer_hwnd)
      KillTimer(win32_sizemove_timer_hwnd, WIN32_SIZEMOVE_TIMER_ID);
   win32_sizemove_timer_hwnd    = NULL;
   win32_sizemove_depth         = 0;
   win32_sizemove_stopped_audio = false;
   win32_sizemove_dirty         = false;
}

/* WM_TIMER with WIN32_SIZEMOVE_TIMER_ID, delivered on the thread that
 * owns the window, which is the thread that created the driver: the
 * video thread when video is threaded, the run loop's otherwise.
 * Either way the two calls below land on the thread that may touch
 * the driver, and video_thread_frame() takes its direct path when it
 * finds itself already on the video thread. Presents only after a resize: with vsync on, a
 * present blocks for a refresh, and one per tick starved the modal
 * loop on D3D12 and Vulkan (drag lagged the mouse, picture refreshed
 * late). Then the same two calls the run loop makes per frame and
 * nothing else: the driver's alive() is where win32_check_window()
 * consumes WIN32_CMN_FLAG_RESIZED and arms the swapchain resize, and
 * video_driver_cached_frame() then presents the last frame into the
 * resized chain - the pause picture. current_video and data have
 * independent lifetimes during teardown, hence both checks. */
void win32_sizemove_tick(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!win32_sizemove_depth)
      return;
   if (!win32_sizemove_dirty)
      return;
   win32_sizemove_dirty = false;
   if (video_st->current_video && video_st->data)
      video_st->current_video->alive(video_st->data);
   video_driver_cached_frame();
}

/* Older SDK headers stop short of the resume notification. */
#ifndef WM_POWERBROADCAST
#define WM_POWERBROADCAST 0x0218
#endif
#ifndef PBT_APMRESUMEAUTOMATIC
#define PBT_APMRESUMEAUTOMATIC 0x0012
#endif

#ifdef HAVE_DINPUT
/* dinput_joypad.c; also set and cleared by xinput_hybrid_joypad.c */
extern volatile bool g_dinput_enum_inflight;
#endif

/* Called from the input driver's WM_DEVICECHANGE, on the thread that
 * owns the main window - the one the device notification is
 * registered on. */
void win32_hotplug_arm(void)
{
   if (main_window.hwnd)
      SetTimer(main_window.hwnd, WIN32_HOTPLUG_TIMER_ID,
            WIN32_HOTPLUG_SETTLE_MS, NULL);
}

/* Called from the input driver's WM_TIMER. Returns true when the
 * joypad driver should be reinitialised now. While a DirectInput
 * enumeration from the previous reinit is still walking the device
 * tree, re-arm and look again one settle period later instead:
 * reinitialising now would discard that walk and queue a fresh one
 * behind it, since the task queue runs one task at a time. */
bool win32_hotplug_due(void)
{
   if (!main_window.hwnd)
      return false;
   KillTimer(main_window.hwnd, WIN32_HOTPLUG_TIMER_ID);
#ifdef HAVE_DINPUT
   if (g_dinput_enum_inflight)
   {
      win32_hotplug_arm();
      return false;
   }
#endif
   return true;
}
#endif

static LRESULT CALLBACK wnd_proc_common(
      bool *quit, HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
#ifdef HAVE_TASKBAR
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   if (   !(g_win32_flags & WIN32_CMN_FLAG_TASKBAR_CREATED)
       && g_win32->taskbar_message
       && message == g_win32->taskbar_message)
      g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif

   switch (message)
   {
      case WM_SYSCOMMAND:
         /* Prevent screensavers, etc, while running. */
         switch (wparam)
         {
            case SC_SCREENSAVE:
            case SC_MONITORPOWER:
               *quit = true;
               break;
         }
         break;
      case WM_DROPFILES:
         win32_drag_query_file(hwnd, wparam);
         DragFinish((HDROP)wparam);
         break;
      case WM_CHAR:
         *quit = true;
         {
            uint16_t mod          = 0;

            mod = win32_update_keyboard_mods();

            /* Seems to be hard to synchronize
             * WM_CHAR and WM_KEYDOWN properly.
             */
            input_keyboard_event(true, RETROK_UNKNOWN,
                  wparam, mod, RETRO_DEVICE_KEYBOARD);
         }
         return TRUE;
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
         /* a window that was left up for a driver that did not take
          * it, being taken down */
         if (hwnd && hwnd == win32_retiring_hwnd)
            break;
#if !defined(_XBOX)
         win32_sizemove_abort();
#endif
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
         *quit          = true;
         /* fall-through */
      case WM_MOVE:
         win32_save_position();
         /* It may be on another monitor now */
         video_driver_window_output_changed();
         break;
#if !defined(_XBOX)
      case WM_ENTERSIZEMOVE:
      case WM_ENTERMENULOOP:
         win32_sizemove_enter(hwnd);
         break;
      case WM_EXITSIZEMOVE:
      case WM_EXITMENULOOP:
         win32_sizemove_exit(hwnd);
         break;
      case WM_POWERBROADCAST:
         /* The system has woken from sleep or hibernation. A
          * controller can come back from that with its handle dead
          * and no device-change notification to say so: a DirectInput
          * pad then reports itself lost, is dropped, and nothing
          * brings it back short of replugging it. Look for
          * controllers again the way a device change does - through
          * the same settle timer, so the reinit runs once and after
          * the bus has had a moment to come back. A device that
          * takes longer still announces itself with a device change
          * of its own. */
         if (wparam == PBT_APMRESUMEAUTOMATIC)
            win32_hotplug_arm();
         break;
      case WM_TIMER:
         /* A hotplug timer reaching here was armed by an input driver
          * this window's wndproc does not route it to. It is one-shot
          * by intent; stop it rather than let it tick forever. */
         if (wparam == WIN32_HOTPLUG_TIMER_ID)
         {
            KillTimer(hwnd, WIN32_HOTPLUG_TIMER_ID);
            *quit = true;
            return 0;
         }
         /* Someone else's timer falls through to DefWindowProc. */
         if (wparam != WIN32_SIZEMOVE_TIMER_ID)
            break;
         win32_sizemove_tick();
         *quit = true;
         return 0;
#endif
      case WM_SIZE:
         /* Do not send resize message if we minimize. */
         if (     wparam != SIZE_MAXHIDE
               && wparam != SIZE_MINIMIZED)
         {
            if (     LOWORD(lparam) != g_win32_resize_width
                  || HIWORD(lparam) != g_win32_resize_height)
            {
               g_win32_resize_width  = LOWORD(lparam);
               g_win32_resize_height = HIWORD(lparam);
               g_win32_flags        |= WIN32_CMN_FLAG_RESIZED;
#if !defined(_XBOX)
               win32_sizemove_dirty  = true;
#endif
            }
         }
         *quit = true;
         break;
      case WM_GETMINMAXINFO:
         {
            MINMAXINFO FAR *lpMinMaxInfo   = (MINMAXINFO FAR *)lparam;
            settings_t *settings           = config_get_ptr();
            unsigned min_width             = MIN_WIDTH;
            unsigned min_height            = MIN_HEIGHT;
            bool window_show_decor         = settings ? settings->bools.video_window_show_decorations : true;
            bool ui_menubar_enable         = settings ? settings->bools.ui_menubar_enable : true;

            if (settings && settings->bools.video_window_save_positions)
               break;

            win32_get_av_info_geometry(&min_width, &min_height);

            if (window_show_decor)
            {
               int border_thickness        = GetSystemMetrics(SM_CXSIZEFRAME);
               int title_bar_height        = GetSystemMetrics(SM_CYCAPTION);

               min_width                  += border_thickness * 2;
               min_height                 += border_thickness * 2 + title_bar_height;
            }

            if (ui_menubar_enable)
            {
               int menu_bar_height         = GetSystemMetrics(SM_CYMENU);

               min_height                 += menu_bar_height;
            }

            lpMinMaxInfo->ptMinTrackSize.x = min_width;
            lpMinMaxInfo->ptMinTrackSize.y = min_height;

            lpMinMaxInfo->ptMaxTrackSize.x = min_width  * 20;
            lpMinMaxInfo->ptMaxTrackSize.y = min_height * 20;
         }
         break;
      case WM_COMMAND:
         win32_menu_loop(main_window.hwnd, wparam);
         break;
#ifdef HAVE_THREADS
      case WM_BROWSER_OPEN_RESULT:
         /* The threaded file-dialog picked a file.
          * LPARAM is a heap-allocated win32_browser_thread_data_t*. */
         {
            win32_browser_thread_data_t *td =
               (win32_browser_thread_data_t *)lparam;
            if (td)
            {
               content_ctx_info_t content_info;
               settings_t      *settings = config_get_ptr();
               video_driver_state_t *video_st = video_state_get_ptr();

               switch (td->mode)
               {
                  case WIN32_BROWSER_MODE_LOAD_CORE:
                     content_info.argc        = 0;
                     content_info.argv        = NULL;
                     content_info.args        = NULL;
                     content_info.environ_get = NULL;
                     if (task_push_load_new_core(
                              td->path, NULL,
                              &content_info,
                              CORE_TYPE_PLAIN,
                              NULL, NULL))
                     {
#ifdef HAVE_MENU
                        /* Force the main menu to rebuild so that entries
                         * which depend on a loaded core (Start Core for
                         * contentless cores, Unload Core, etc.) appear
                         * on the fly instead of only after the next
                         * user-driven menu interaction. */
                        struct menu_state *menu_st = menu_state_get_ptr();
                        menu_st->flags            |=
                              MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                            | MENU_ST_FLAG_PREVENT_POPULATE;
#endif
                     }
                     break;
                  case WIN32_BROWSER_MODE_LOAD_CONTENT:
                     win32_load_content_from_gui(td->path);
                     break;
                  default:
                     break;
               }

               /* Full screen: hide mouse now that the dialog is gone */
               if (settings->bools.video_fullscreen)
               {
                  if (     video_st->poke
                        && video_st->poke->show_mouse)
                     video_st->poke->show_mouse(video_st->data, false);
               }

               free(td);
            }
         }
         break;
      case WM_BROWSER_CANCELLED:
         /* The threaded file-dialog was cancelled / closed.
          * LPARAM is a heap-allocated win32_browser_thread_data_t*. */
         {
            win32_browser_thread_data_t *td =
               (win32_browser_thread_data_t *)lparam;
            if (td)
            {
               settings_t      *settings = config_get_ptr();
               video_driver_state_t *video_st = video_state_get_ptr();

               /* Full screen: hide mouse now that the dialog is gone */
               if (settings->bools.video_fullscreen)
               {
                  if (     video_st->poke
                        && video_st->poke->show_mouse)
                     video_st->poke->show_mouse(video_st->data, false);
               }

               free(td);
            }
         }
         break;
#endif /* HAVE_THREADS */
   }
   return 0;
}

/* The kind of input driver the window's messages are routed for, and
 * the video family the window belongs to. Both are set by
 * win32_window_proc_setup() before the window is created. */
static enum win32_input_kind   win32_wnd_input  = WIN32_INPUT_OTHER;
static enum win32_window_family win32_wnd_family = WIN32_WINDOW_D3D;

/* win32_msg_route.h spells out Windows' message numbers so that it can
 * be built without the SDK. They are the SDK's. */
typedef char win32_msg_route_numbers_check[
   (     WIN32_MSG_DESTROY            == WM_DESTROY
      && WIN32_MSG_MOVE               == WM_MOVE
      && WIN32_MSG_SIZE               == WM_SIZE
      && WIN32_MSG_SETFOCUS           == WM_SETFOCUS
      && WIN32_MSG_KILLFOCUS          == WM_KILLFOCUS
      && WIN32_MSG_CLOSE              == WM_CLOSE
      && WIN32_MSG_QUIT               == WM_QUIT
      && WIN32_MSG_GETMINMAXINFO      == WM_GETMINMAXINFO
      && WIN32_MSG_DISPLAYCHANGE      == WM_DISPLAYCHANGE
      && WIN32_MSG_NCLBUTTONDBLCLK    == WM_NCLBUTTONDBLCLK
      && WIN32_MSG_KEYDOWN            == WM_KEYDOWN
      && WIN32_MSG_KEYUP              == WM_KEYUP
      && WIN32_MSG_CHAR               == WM_CHAR
      && WIN32_MSG_SYSKEYDOWN         == WM_SYSKEYDOWN
      && WIN32_MSG_SYSKEYUP           == WM_SYSKEYUP
      && WIN32_MSG_IME_ENDCOMPOSITION == WM_IME_ENDCOMPOSITION
      && WIN32_MSG_IME_COMPOSITION    == WM_IME_COMPOSITION
      && WIN32_MSG_COMMAND            == WM_COMMAND
      && WIN32_MSG_SYSCOMMAND         == WM_SYSCOMMAND
      && WIN32_MSG_TIMER              == WM_TIMER
      && WIN32_MSG_MOUSEMOVE          == WM_MOUSEMOVE
      && WIN32_MSG_MOUSEWHEEL         == WM_MOUSEWHEEL
      && WIN32_MSG_MOUSEHWHEEL        == WM_MOUSEHWHEEL
      && WIN32_MSG_ENTERMENULOOP      == WM_ENTERMENULOOP
      && WIN32_MSG_EXITMENULOOP       == WM_EXITMENULOOP
      && WIN32_MSG_POWERBROADCAST     == WM_POWERBROADCAST
      && WIN32_MSG_DEVICECHANGE       == WM_DEVICECHANGE
      && WIN32_MSG_ENTERSIZEMOVE      == WM_ENTERSIZEMOVE
      && WIN32_MSG_EXITSIZEMOVE       == WM_EXITSIZEMOVE
      && WIN32_MSG_DROPFILES          == WM_DROPFILES
      && WIN32_MSG_POINTERUPDATE      == WM_POINTERUPDATE
      && WIN32_MSG_POINTERDOWN        == WM_POINTERDOWN
      && WIN32_MSG_POINTERUP          == WM_POINTERUP
#ifdef HAVE_THREADS
      && WIN32_MSG_BROWSER_OPEN_RESULT == WM_BROWSER_OPEN_RESULT
      && WIN32_MSG_BROWSER_CANCELLED   == WM_BROWSER_CANCELLED
#endif
      && WIN32_MSG_HOTPLUG_TIMER_ID   == WIN32_HOTPLUG_TIMER_ID) ? 1 : -1];

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "Imm32")
#endif

/* Offer a message to the input driver. True if the driver took it. */
static bool win32_wnd_input_message(UINT message,
      WPARAM wparam, LPARAM lparam)
{
   switch (win32_wnd_input)
   {
#ifdef HAVE_DINPUT
      case WIN32_INPUT_DINPUT:
         {
            void *input_data = (void*)(LONG_PTR)GetWindowLongPtr(
                  main_window.hwnd, GWLP_USERDATA);
            return input_data && dinput_handle_message(input_data,
                  message, wparam, lparam);
         }
#endif
#if defined(HAVE_WINRAWINPUT) && !defined(_XBOX)
      case WIN32_INPUT_WINRAW:
         return winraw_handle_message(message, wparam, lparam);
#endif
      default:
         break;
   }
   return false;
}

/* The window procedure, past the window's creation. What it does with
 * a message is win32_msg_route()'s answer for the kind of input driver
 * in use; the steps are taken here, in order. */
static LRESULT win32_wnd_proc_route(HWND hwnd,
      UINT message, WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   unsigned route;

#ifdef HAVE_TASKBAR
   if (   !(g_win32_flags & WIN32_CMN_FLAG_TASKBAR_CREATED)
       && g_win32->taskbar_message
       && message == g_win32->taskbar_message)
      g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif

   route = win32_msg_route(win32_wnd_input,
#if defined(_XBOX)
         false,
#else
         true,
#endif
#ifdef HAVE_THREADS
         true,
#else
         false,
#endif
         (unsigned)message, wparam == WIN32_HOTPLUG_TIMER_ID);

   if (!route)
      return DefWindowProc(hwnd, message, wparam, lparam);

#ifdef HAVE_DINPUT
   if (route & WIN32_ROUTE_IME_END)
      input_keyboard_event(true, 1, 0x80000000, 0, RETRO_DEVICE_KEYBOARD);

   if (route & WIN32_ROUTE_IME_TEXT)
   {
      HIMC    hIMC = ImmGetContext(hwnd);
      /* Process composition and result strings separately;
       * ImmGetCompositionStringW expects a single flag per call. */
      unsigned gcs_flags[2] = { GCS_RESULTSTR, GCS_COMPSTR };
      int f;
      for (f = 0; f < 2; f++)
      {
         unsigned gcs_flag = gcs_flags[f];
         if (!(lparam & gcs_flag))
            continue;
         {
            int i;
            /* Request up to 2 wide chars (4 bytes). Return value is in bytes. */
            wchar_t wstr[3] = {0, 0, 0};
            LONG byte_len   = ImmGetCompositionStringW(
                  hIMC, gcs_flag, wstr, 2 * sizeof(wchar_t));
            int char_count;

            if (byte_len <= 0 || byte_len > (LONG)(2 * sizeof(wchar_t)))
               continue;

            char_count = byte_len / (int)sizeof(wchar_t);

            for (i = 0; i < char_count; i++)
            {
               wchar_t single[2];
               char *utf8;
               size_t utf8_len;
               uint32_t packed = 0;

               single[0] = wstr[i];
               single[1] = 0;

               utf8 = utf16_to_utf8_string_alloc(single);
               if (!utf8)
                  continue;

               utf8_len = strlen(utf8);

               /* Pack up to 3 UTF-8 bytes into the low 24 bits and
                * the composition/result flag into the high byte.
                * This matches what the receiver expects as a uint32. */
               if (utf8_len >= 1 && utf8_len <= 3)
               {
                  memcpy(&packed, utf8, utf8_len);
                  if (utf8_len >= 2)
                     ((unsigned char*)&packed)[3] =
                        (unsigned char)((gcs_flag) | (gcs_flag >> 4));
                  input_keyboard_event(true, 1, (uint32_t)packed, 0,
                        RETRO_DEVICE_KEYBOARD);
               }
               free(utf8);
            }
         }
      }
      ImmReleaseContext(hwnd, hIMC);
      return 0;
   }
#endif

   if (route & WIN32_ROUTE_KEY)
   {
      if (route & (WIN32_ROUTE_KEY_MODS | WIN32_ROUTE_KEY_EVENT))
      {
         bool keydown     = !(message == WM_KEYUP || message == WM_SYSKEYUP);
         unsigned keysym  = (lparam >> 16) & 0xff;
         bool extended    = (lparam >> 24) & 0x1;

         /* NumLock vs Pause correction */
         if (keysym == 0x45 && (wparam == VK_NUMLOCK || wparam == VK_PAUSE))
            extended = !extended;

         /* extended keys will map to dinput if the high bit is set */
         if (extended)
            keysym |= 0x80;

         /* tell the driver about shift and alt key events */
         if (     (route & WIN32_ROUTE_KEY_MODS)
               && (     keysym == 0x2A/*DIK_LSHIFT*/
                     || keysym == 0x36/*DIK_RSHIFT*/
                     || keysym == 0x38/*DIK_LMENU*/
                     || keysym == 0xB8/*DIK_RMENU*/)
               && win32_wnd_input_message(message, wparam, lparam))
            return 0; /* key up already handled by the driver */

         if (route & WIN32_ROUTE_KEY_EVENT)
         {
            unsigned keycode = input_keymaps_translate_keysym_to_rk(keysym);
            uint16_t mod     = win32_update_keyboard_mods();

            input_keyboard_event(keydown, keycode,
                  0, mod, RETRO_DEVICE_KEYBOARD);
         }
      }

      if (message != WM_SYSKEYDOWN)
         return 0;

      if (     wparam == VK_F10
            || wparam == VK_MENU
            || wparam == VK_RSHIFT)
         return 0;
   }

#ifdef HAVE_CLIP_WINDOW
   if (     (route & (WIN32_ROUTE_CLIP_ON | WIN32_ROUTE_CLIP_OFF))
         && (input_driver_get_flags() & INP_FLAG_GRAB_MOUSE_STATE))
      win32_clip_window((route & WIN32_ROUTE_CLIP_ON) != 0);
#endif

   if (     (route & WIN32_ROUTE_INPUT)
         && win32_wnd_input_message(message, wparam, lparam))
      return 0;

   if (route & WIN32_ROUTE_COMMON)
   {
      bool quit   = false;
      LRESULT ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
      if (quit)
         return ret;
   }

   if (route & WIN32_ROUTE_DISPLAY)
   {
      /* Fix size after display mode switch when using SR */
      HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      if (mon)
         win32_resize_after_display_change(hwnd, mon);
      video_driver_window_output_changed();
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
extern void create_gl_context(HWND hwnd, bool *quit);
extern void create_gles_context(HWND hwnd, bool *quit);

static LRESULT wnd_proc_wgl_wm_create(HWND hwnd)
{
   extern enum gfx_ctx_api win32_api;
   bool is_quit = false;
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)) && !defined(HAVE_OPENGLES)
         create_gl_context(hwnd, &is_quit);
#endif
         break;

      case GFX_CTX_OPENGL_ES_API:
#if defined (HAVE_OPENGLES)
         create_gles_context(hwnd, &is_quit);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
   if (is_quit)
      g_win32_flags |= WIN32_CMN_FLAG_QUIT;
   if (DragAcceptFiles_func)
      DragAcceptFiles_func(hwnd, true);
   g_win32_flags |= WIN32_CMN_FLAG_INITED;
   return 0;
}

#endif

#ifdef HAVE_VULKAN
#include "vulkan_common.h"

static LRESULT wnd_proc_wm_vk_create(HWND hwnd)
{
   RECT rect;
   extern int win32_vk_interval;
   extern gfx_ctx_vulkan_data_t win32_vk;
   unsigned width     = 0;
   unsigned height    = 0;
   HINSTANCE instance = GetModuleHandle(NULL);

   GetClientRect(hwnd, &rect);

   width              = rect.right - rect.left;
   height             = rect.bottom - rect.top;

   if (!vulkan_surface_create(&win32_vk,
            VULKAN_WSI_WIN32,
            &instance, &hwnd,
            VIDEO_SCALE_PACK(width, height), win32_vk_interval))
      g_win32_flags |= WIN32_CMN_FLAG_QUIT;
   g_win32_flags    |= WIN32_CMN_FLAG_INITED;
   if (DragAcceptFiles_func)
      DragAcceptFiles_func(hwnd, true);
   return 0;
}

#endif

#ifdef HAVE_GDI
static LRESULT wnd_proc_wm_gdi_create(HWND hwnd)
{
   extern HDC win32_gdi_hdc;
   win32_gdi_hdc = GetDC(hwnd);
   win32_setup_pixel_format(win32_gdi_hdc, false);
   g_win32_flags |= WIN32_CMN_FLAG_INITED;
   if (DragAcceptFiles_func)
      DragAcceptFiles_func(hwnd, true);
   return 0;
}

/* The WM_PAINT body for a GDI window.  Presents gdi->bmp scaled into the
 * aspect-ratio-aware viewport rect (gdi->vp), filling the area
 * outside the rect with black to produce letterbox / pillarbox
 * bars.  Reads bmp_dims for the source rect (the DDB's actual
 * size); when RGUI is alive bmp holds the menu image at the menu's
 * resolution while frame_dims still tracks the core, so frame_dims
 * is only the fallback. */
static void wnd_proc_gdi_paint(gdi_t *gdi)
{
   int       vp_x   = VIDEO_POS_X(gdi->vp.pos);
   int       vp_y   = VIDEO_POS_Y(gdi->vp.pos);
   unsigned  vp_dims  = gdi->vp.dims  ? gdi->vp.dims  : gdi->screen_dims;
   unsigned  src_dims = gdi->bmp_dims ? gdi->bmp_dims : gdi->frame_dims;
   unsigned  vp_w   = VIDEO_SCALE_W(vp_dims);
   unsigned  vp_h   = VIDEO_SCALE_H(vp_dims);
   unsigned  src_w  = VIDEO_SCALE_W(src_dims);
   unsigned  src_h  = VIDEO_SCALE_H(src_dims);

   /* Letterbox / pillarbox bars: paint the four areas outside the
    * viewport rect black before the StretchBlt.  We do this even
    * when the viewport happens to fill the whole window — extra
    * FillRects on zero-area regions are cheap. */
   if (vp_x > 0 || vp_y > 0
         || vp_x + (int)vp_w  < (int)VIDEO_SCALE_W(gdi->screen_dims)
         || vp_y + (int)vp_h  < (int)VIDEO_SCALE_H(gdi->screen_dims))
   {
      RECT rect;
      HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
      /* Top */
      if (vp_y > 0)
      {
         rect.left = 0; rect.top = 0;
         rect.right = (LONG)VIDEO_SCALE_W(gdi->screen_dims); rect.bottom = vp_y;
         FillRect(gdi->winDC, &rect, black);
      }
      /* Bottom */
      if (vp_y + (int)vp_h < (int)VIDEO_SCALE_H(gdi->screen_dims))
      {
         rect.left = 0; rect.top = vp_y + (int)vp_h;
         rect.right = (LONG)VIDEO_SCALE_W(gdi->screen_dims);
         rect.bottom = (LONG)VIDEO_SCALE_H(gdi->screen_dims);
         FillRect(gdi->winDC, &rect, black);
      }
      /* Left */
      if (vp_x > 0)
      {
         rect.left = 0; rect.top = vp_y;
         rect.right = vp_x; rect.bottom = vp_y + (int)vp_h;
         FillRect(gdi->winDC, &rect, black);
      }
      /* Right */
      if (vp_x + (int)vp_w < (int)VIDEO_SCALE_W(gdi->screen_dims))
      {
         rect.left = vp_x + (int)vp_w; rect.top = vp_y;
         rect.right = (LONG)VIDEO_SCALE_W(gdi->screen_dims);
         rect.bottom = vp_y + (int)vp_h;
         FillRect(gdi->winDC, &rect, black);
      }
   }

   gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);
   StretchBlt(gdi->winDC,
         vp_x, vp_y, vp_w, vp_h,
         gdi->memDC,
         0, 0, src_w, src_h,
         SRCCOPY);
   SelectObject(gdi->memDC, gdi->bmp_old);
}

#endif

/* The window procedure: one, for every video driver and every input
 * driver.
 *
 * A video family differs from another in what creating the window
 * sets up - a GL context, a Vulkan surface, a device context - and GDI
 * also paints from here. Everything else is win32_wnd_proc_route(). */
/* Text entry and the IME.
 *
 * A window has an input context by default, and while it has one every
 * key that comes off its queue is first offered to the IME: on
 * Windows 8 and later that is the text services framework, for every
 * keyboard layout and not only the East Asian ones. RetroArch has a
 * use for what the IME produces in one place, a line of text being
 * typed in the menu. Everywhere else the offer is wasted work on the
 * thread that pumps the window, on each key down and up and each
 * repeat, and an IME left switched on puts its composition in front
 * of a game.
 *
 * So the window is without its input context except while a line of
 * text is open. The frontend says when one opens and closes
 * (win32_text_entry()); what it last said is kept here, and applied to
 * the window on the thread that owns it - when the window is made, and
 * on a posted message after that. Nothing waits for the message.
 *
 * Text from a layout that needs no IME is not touched by this: it
 * comes from TranslateMessage(). The other windows of the process -
 * the desktop menu, file dialogs - have input contexts of their own.
 *
 * ImmAssociateContextEx() is looked up when first wanted, as Windows
 * NT 4 and 95 do not have it; without it nothing changes. */
#define WIN32_WM_TEXT_ENTRY   (WM_APP + 0x7e)
#define WIN32_IACE_DEFAULT    0x0010

static retro_atomic_int_t win32_text_entry_on;

/* On the thread that owns @hwnd. */
static void win32_ime_apply(HWND hwnd, bool attach)
{
   typedef BOOL (WINAPI *imm_associate_ex_t)(HWND, HANDLE, DWORD);
   static imm_associate_ex_t associate_ex;
   static bool looked_up;

   if (!looked_up)
   {
      HMODULE imm = GetModuleHandleA("imm32.dll");
      if (!imm)
         imm       = LoadLibraryA("imm32.dll");
      if (imm)
         associate_ex = (imm_associate_ex_t)
            GetProcAddress(imm, "ImmAssociateContextEx");
      looked_up    = true;
   }

   if (associate_ex)
      associate_ex(hwnd, NULL, attach ? WIN32_IACE_DEFAULT : 0);
}

/* A line of text has been opened, or closed. From the frontend's
 * thread; the window's own thread does the work. */
void win32_text_entry(bool active)
{
   int now = active ? 1 : 0;

   if (retro_atomic_load_relaxed_int(&win32_text_entry_on) == now)
      return;
   retro_atomic_store_release_int(&win32_text_entry_on, now);
   if (main_window.hwnd)
      PostMessage(main_window.hwnd, WIN32_WM_TEXT_ENTRY, 0, 0);
}

LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WIN32_WM_TEXT_ENTRY)
   {
      /* what the frontend says now, which is what the last of these
       * messages stands for */
      win32_ime_apply(hwnd,
            retro_atomic_load_acquire_int(&win32_text_entry_on) != 0);
      return 0;
   }

   if (message == WM_CREATE)
   {
      /* a new window comes with an input context: it keeps it only if
       * a line of text is open already */
      win32_ime_apply(hwnd,
            retro_atomic_load_acquire_int(&win32_text_entry_on) != 0);

      switch (win32_wnd_family)
      {
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         case WIN32_WINDOW_WGL:
            return wnd_proc_wgl_wm_create(hwnd);
#endif
#ifdef HAVE_VULKAN
         case WIN32_WINDOW_VULKAN:
            return wnd_proc_wm_vk_create(hwnd);
#endif
#ifdef HAVE_GDI
         case WIN32_WINDOW_GDI:
            return wnd_proc_wm_gdi_create(hwnd);
#endif
         case WIN32_WINDOW_D3D:
         default:
            break;
      }

      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);

      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

#ifdef HAVE_GDI
   if (message == WM_PAINT && win32_wnd_family == WIN32_WINDOW_GDI)
   {
      gdi_t *gdi = (gdi_t*)video_driver_get_ptr();
      if (gdi && gdi->memDC)
         wnd_proc_gdi_paint(gdi);
   }
#endif

   return win32_wnd_proc_route(hwnd, message, wparam, lparam);
}

/* Called by a video driver before it creates its window: which family
 * the window is, so that its creation sets up the right thing.
 *
 * The kind of input driver is read from the setting here, once, the
 * way each video driver used to read it to choose among its three
 * procedures. */
void win32_window_proc_setup(enum win32_window_family family)
{
   settings_t *settings = config_get_ptr();

   win32_wnd_family     = family;
   /* a driver that has one says so after this */
   win32_wnd_tag        = NULL;
   win32_wnd_input      = WIN32_INPUT_OTHER;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      win32_wnd_input   = WIN32_INPUT_DINPUT;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      win32_wnd_input   = WIN32_INPUT_WINRAW;
#endif
   (void)settings;
}

/* The window across a driver restart.
 *
 * A restart of the video driver - content loaded or closed, a setting
 * changed - used to destroy the window and make another: the window
 * vanished and came back, lost its place in front of other windows,
 * and anything tied to it went with it. The driver that comes back is
 * nearly always the one that left, and has no need of a new window.
 *
 * So the context being freed may leave its window up
 * (win32_window_keep()), and win32_window_create() takes it instead of
 * making one: the style, place and size it would have been created
 * with are applied to it, and what creating a window sets up for the
 * video family - for Vulkan, the surface - is done for it.
 *
 * A window belongs to the thread that made it: only that thread can
 * take it back, or destroy it, and it goes when the thread ends. With
 * threaded video that thread is the video thread, which used to end
 * with the driver; it is held for the next one
 * (video_thread_host_hold()). If the driver that comes next runs on
 * another thread, or is of another kind, or does not come, the window
 * is taken down: win32_window_release_kept(), which the frontend
 * calls once the next driver is up.
 *
 * For the Vulkan and the GDI families - the ones whose window sets up
 * nothing a second driver of the same family cannot set up again on
 * the same window. Never when the program is shutting down.
 * RETROARCH_WINDOW_KEEP=0 in the environment turns it off.
 *
 * And for the OpenGL family, with one condition. A window's pixel
 * format is set once and cannot be changed, and the format OpenGL
 * wants depends on whether HDR is on. So a kept OpenGL window is
 * taken only if the format it has is the one that would be chosen for
 * it now (win32_window_pixel_format_fits()); otherwise it goes and a
 * new one is made.
 *
 * And for a Direct3D driver that has given its window a tag
 * (win32_window_tag()), which says two things: that it frees
 * everything it had on the window before it leaves it, and that it
 * makes a new window if it cannot have the one it took
 * (win32_window_remake()). The tag is also what keeps one Direct3D
 * driver from taking another's window. */
static bool win32_window_family_keeps(void)
{
   if (     win32_wnd_family == WIN32_WINDOW_VULKAN
         || win32_wnd_family == WIN32_WINDOW_GDI
         || win32_wnd_family == WIN32_WINDOW_WGL)
      return true;
   return win32_wnd_family == WIN32_WINDOW_D3D && win32_wnd_tag;
}

static bool win32_window_pixel_format_fits(HWND hwnd);

void win32_window_tag(const char *tag)
{
   win32_wnd_tag = tag;
}

bool win32_window_was_taken(void)
{
   return win32_wnd_taken;
}

static void win32_window_destroy_kept(HWND hwnd)
{
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
   if (win32_kept_notification)
      UnregisterDeviceNotification(win32_kept_notification);
   win32_kept_notification = NULL;
#endif
   win32_retiring_hwnd = hwnd;
   DestroyWindow(hwnd);
   win32_retiring_hwnd = NULL;
}

/* 1, the default: the window is left up and taken back. With
 * RETROARCH_WINDOW_KEEP=0 it is destroyed with the driver, as it used
 * to be; with =2 it is left up and then not taken, so that the way
 * out for a window nothing takes can be run (the tests do). */
static int win32_window_keep_mode(void)
{
   const char *env = getenv("RETROARCH_WINDOW_KEEP");
   if (env && (env[0] == '0' || env[0] == '2'))
      return env[0] - '0';
   return 1;
}

/* the hook of video_thread_host_hold(): the video thread is ending
 * with the kept window still on it */
static void win32_window_kept_thread_ends(void)
{
   HWND hwnd = win32_kept_hwnd;
   if (hwnd && win32_kept_tid == GetCurrentThreadId())
   {
      win32_kept_hwnd = NULL;
      win32_window_destroy_kept(hwnd);
   }
}

bool win32_window_keep(void)
{
   HWND hwnd       = main_window.hwnd;

   if (     !hwnd
         || win32_kept_hwnd
         || !win32_window_keep_mode()
         || !win32_window_family_keeps()
         /* the display's mode was changed for it: put back as ever */
         || (g_win32_flags & WIN32_CMN_FLAG_RESTORE_DESKTOP)
         || (runloop_get_flags() & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
      return false;
   /* it lasts only as long as its thread does */
   if (!task_is_on_main_thread())
   {
#ifdef HAVE_THREADS
      if (!video_thread_host_hold(win32_window_kept_thread_ends))
#endif
         return false;
   }

   win32_monitor_last = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
   win32_kept_hwnd    = hwnd;
   win32_kept_tid     = GetCurrentThreadId();
   win32_kept_family  = (int)win32_wnd_family;
   win32_kept_tag     = win32_wnd_tag;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
   win32_kept_notification = notification_handler;
   notification_handler    = NULL;
#endif
   main_window.hwnd   = NULL;
   video_driver_window_set(0);
   RARCH_LOG("[Win32] The window is left up for the next video driver.\n");
   return true;
}

void win32_window_release_kept(void)
{
   HWND hwnd = win32_kept_hwnd;

   if (!hwnd)
      return;
   if (!IsWindow(hwnd))
      win32_kept_hwnd = NULL;
   else if (win32_kept_tid == GetCurrentThreadId())
   {
      win32_kept_hwnd = NULL;
      win32_window_destroy_kept(hwnd);
   }
#ifdef HAVE_THREADS
   else if (video_thread_host_is_held())
      /* its thread takes it down as it ends */
      video_thread_host_stop();
#endif
   else
      return; /* its own thread's to let go of */

   RARCH_LOG("[Win32] The window left up was not taken: destroyed.\n");
   if (!main_window.hwnd)
      UnregisterClass("RetroArch", GetModuleHandle(NULL));
}

/* The kept window, if this thread can have it and the family is the
 * one it was kept by. One this thread cannot use but can destroy
 * goes here. */
static HWND win32_window_take_kept(void)
{
   HWND hwnd = win32_kept_hwnd;

   if (!hwnd)
      return NULL;
   if (!IsWindow(hwnd))
   {
      win32_kept_hwnd = NULL;
      return NULL;
   }
   if (win32_kept_tid != GetCurrentThreadId())
      return NULL;
   win32_kept_hwnd = NULL;
   if (     (int)win32_wnd_family != win32_kept_family
         || win32_wnd_tag != win32_kept_tag
         || win32_window_keep_mode() != 1
         || (   win32_wnd_family == WIN32_WINDOW_WGL
             && !win32_window_pixel_format_fits(hwnd)))
   {
      win32_window_destroy_kept(hwnd);
      RARCH_LOG("[Win32] The window left up is not one this driver takes: destroyed.\n");
      return NULL;
   }
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
   notification_handler    = win32_kept_notification;
   win32_kept_notification = NULL;
#endif
   return hwnd;
}

/* The menu bar while the window has none.
 *
 * The menu bar belongs to the windowed state. It was destroyed
 * whenever the window went fullscreen, or was taken back from the
 * last driver, and a new one built - made from its template and
 * every item put into the user's language - each time the window was
 * windowed again: once for every return from fullscreen and every
 * driver restart. It is the same menu each time.
 *
 * It is now taken off the window and kept, and put back when the
 * window is windowed again. A new one is built only when there is
 * none kept, and the kept one is dropped when the language changes
 * (win32_menu_kept_drop(), from win32_menubar_rebuild()). A menu that
 * is on no window belongs to no window: it outlives the window it
 * was taken from and goes onto the next. */
static HMENU win32_menu_kept;

void win32_menu_kept_drop(void)
{
   if (win32_menu_kept)
      DestroyMenu(win32_menu_kept);
   win32_menu_kept = NULL;
}

/* The window's menu bar, if it has one, taken off it and kept. */
static void win32_menu_put_away(HWND hwnd)
{
   HMENU menu = GetMenu(hwnd);
   if (!menu)
      return;
   SetMenu(hwnd, NULL);
   if (win32_menu_kept && win32_menu_kept != menu)
      DestroyMenu(win32_menu_kept);
   win32_menu_kept = menu;
}

/* A menu bar for a windowed window: the kept one, or a new one. */
static HMENU win32_menu_take(void)
{
   HMENU menu      = win32_menu_kept;
   win32_menu_kept = NULL;
   if (!menu)
   {
      menu         = win32_resources_create_menu();
      win32_localize_menu(menu);
      RARCH_DBG("[Win32] Menu bar built.\n");
   }
   return menu;
}

static bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   settings_t       *settings    = config_get_ptr();
#ifdef HAVE_TASKBAR
   DEV_BROADCAST_DEVICEINTERFACE notification_filter;
#endif
#ifdef HAVE_WINDOW_TRANSP
   unsigned    window_opacity    = settings->uints.video_window_opacity;
#endif
   bool    window_save_positions = settings->bools.video_window_save_positions;
   unsigned    user_width        = width;
   unsigned    user_height       = height;
   const char *new_label         = msg_hash_to_str(MSG_PROGRAM);
#ifdef LEGACY_WIN32
   char *title_local             = utf8_to_local_string_alloc(new_label);
#else
   wchar_t *title_local          = utf8_to_utf16_string_alloc(new_label);
#endif

   bool        reused            = false;

   if (window_save_positions && !fullscreen)
   {
      user_width                 = VIDEO_SCALE_W(g_win32->pos_dims);
      user_height                = VIDEO_SCALE_H(g_win32->pos_dims);
   }

   win32_wnd_taken               = false;
   if ((main_window.hwnd = win32_window_take_kept()))
   {
      HWND  hwnd      = main_window.hwnd;
      HMENU menu      = GetMenu(hwnd);
      bool was_popup  = (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_POPUP) != 0;
      UINT swp        = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
      int  x          = mon_rect->left;
      int  y          = mon_rect->top;

      reused          = true;
      win32_wnd_taken = true;
      free(title_local);

      /* win32_set_window() gives a windowed window its menu again:
       * the same one, which is kept in the meantime */
      if (menu)
         win32_menu_put_away(hwnd);
      /* The style, place and size it would have been created with. A
       * windowed window stays where it is - unless it was fullscreen,
       * and is put where a new one would have been as far as that can
       * be said: at its saved position, or over the monitor's corner. */
      if (!fullscreen)
      {
         if (!was_popup)
            swp      |= SWP_NOMOVE;
         else if (g_win32->pos_set)
         {
            x         = VIDEO_POS_X(g_win32->pos);
            y         = VIDEO_POS_Y(g_win32->pos);
         }
      }
      SetWindowLongPtr(hwnd, GWL_STYLE,
            (LONG_PTR)style
            | (IsWindowVisible(hwnd) ? WS_VISIBLE : 0));
      SetWindowPos(hwnd, NULL, x, y, user_width, user_height, swp);

      /* what creating it sets up for the video family */
      g_win32_flags &= ~WIN32_CMN_FLAG_INITED;
      win32_window_proc(hwnd, WM_CREATE, 0, 0);
      RARCH_LOG("[Win32] Took the window left up by the last video driver.\n");
   }
   else
   {
#ifdef LEGACY_WIN32
   main_window.hwnd              = CreateWindowEx(0,
         "RetroArch", title_local,
#else
   main_window.hwnd              = CreateWindowExW(0,
         L"RetroArch", title_local,
#endif
         style,
         fullscreen ? mon_rect->left
            : (g_win32->pos_set ? VIDEO_POS_X(g_win32->pos)
                                : CW_USEDEFAULT),
         fullscreen ? mon_rect->top
            : (g_win32->pos_set ? VIDEO_POS_Y(g_win32->pos)
                                : CW_USEDEFAULT),
         user_width,
         user_height,
         NULL, NULL, NULL, data);
   free(title_local);
   }
   if (!main_window.hwnd)
      return false;

   window_accelerators = win32_resources_get_accelerator();

#ifdef HAVE_TASKBAR
   g_win32->taskbar_message            =
      RegisterWindowMessage("TaskbarButtonCreated");

   memset(&notification_filter, 0, sizeof(notification_filter));
   notification_filter.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
   notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
   notification_filter.dbcc_classguid  = GUID_DEVINTERFACE_HID;
   /* a window taken back is still registered */
   if (!reused)
      notification_handler             = RegisterDeviceNotification(
         main_window.hwnd, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE);

   if (!notification_handler)
      RARCH_ERR("[Win32] Error registering for notifications.\n");
#endif

   video_driver_display_type_set(RARCH_DISPLAY_WIN32);
   video_driver_display_set(0);
   video_driver_display_userdata_set((uintptr_t)&main_window);
   video_driver_window_set((uintptr_t)main_window.hwnd);

#ifdef HAVE_WINDOW_TRANSP
   /* Windows 2000 and above use layered windows to enable transparency */
   if (window_opacity < 100)
   {
      SetWindowLongPtr(main_window.hwnd,
           GWL_EXSTYLE,
           GetWindowLongPtr(main_window.hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
      SetLayeredWindowAttributes(main_window.hwnd, 0, (255 *
               window_opacity) / 100, LWA_ALPHA);
   }
#endif
   return true;
}
#endif

void win32_monitor_init(void)
{
#if !defined(_XBOX)
   win32_common_state_t
      *g_win32            = (win32_common_state_t*)&win32_st;
   g_win32->monitor_count = 0;
   EnumDisplayMonitors(NULL, NULL,
         win32_monitor_enum_proc, 0);
#endif
   g_win32_flags         &= ~WIN32_CMN_FLAG_QUIT;
}

#if !defined(_XBOX)
void win32_show_cursor(void *data, bool state)
{
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
}

void win32_check_window(void *data,
      bool *quit, bool *resize,
      unsigned *dims)
{
   bool video_is_threaded = video_driver_is_threaded();
   if (video_is_threaded)
      ui_companion_win32.application->process_events();
   *quit                  = (g_win32_flags & WIN32_CMN_FLAG_QUIT) ? true : false;

   if (g_win32_flags & WIN32_CMN_FLAG_RESIZED)
   {
      *resize             = true;
      *dims              = VIDEO_SCALE_PACK(g_win32_resize_width, g_win32_resize_height);
      g_win32_flags      &= ~WIN32_CMN_FLAG_RESIZED;
   }
}
#endif

#ifdef HAVE_CLIP_WINDOW
void win32_clip_window(bool state)
{
   if (state && main_window.hwnd)
   {
      WINDOWINFO info;
      RECT clip_rect;
      info.cbSize      = sizeof(WINDOWINFO);

      if (GetWindowInfo(main_window.hwnd, &info))
         clip_rect = info.rcClient;
      else
      {
         clip_rect.left   = 0;
         clip_rect.top    = 0;
         clip_rect.right  = 0;
         clip_rect.bottom = 0;
      }

      ClipCursor(&clip_rect);
   }
   else
      ClipCursor(NULL);
}
#endif


typedef HRESULT (WINAPI *win32_dwm_timing_fn)(HWND, win32_dwm_timing_info_t*);

retro_time_t win32_dwm_last_vblank_time(void)
{
#ifdef _XBOX
   return 0;
#else
   win32_dwm_timing_info_t info;
   static win32_dwm_timing_fn get_timing;
   static bool                resolved;
   static LARGE_INTEGER       freq;

   /* dwmapi does not exist before Vista and the tree still builds for
    * older targets, so the entry point is resolved once at runtime, as
    * the D3DKMT ones above are. */
   if (!resolved)
   {
      HMODULE dwm = LoadLibrary("dwmapi.dll");
      resolved    = true;
      if (dwm)
         get_timing = (win32_dwm_timing_fn)GetProcAddress(dwm,
               "DwmGetCompositionTimingInfo");
   }
   if (!get_timing)
      return 0;

   memset(&info, 0, sizeof(info));
   info.cbSize = sizeof(info);
   if (FAILED(get_timing(NULL, &info)))
      return 0;
   if (!info.qpcVBlank)
      return 0;
   if (!freq.QuadPart && !QueryPerformanceFrequency(&freq))
      return 0;
   return (retro_time_t)((info.qpcVBlank / freq.QuadPart * 1000000)
        + (info.qpcVBlank % freq.QuadPart * 1000000 / freq.QuadPart));
#endif
}

#ifdef _XBOX
static HWND GetForegroundWindow(void) { return main_window.hwnd; }
BOOL IsIconic(HWND hwnd) { return FALSE; }
bool win32_has_focus(void *data) { return true; }
HWND win32_get_window(void) { return NULL; }
#else
bool win32_has_focus(void *data)
{
   settings_t *settings           = config_get_ptr();

   /* Ensure window size is big enough for core geometry. */
   if (      settings
         && !settings->bools.video_fullscreen
         && !settings->bools.video_window_save_positions)
   {
      unsigned video_scale        = settings->uints.video_scale;
      unsigned extra_width        = 0;
      unsigned extra_height       = 0;
      unsigned min_width          = 0;
      unsigned min_height         = 0;

      win32_get_av_info_geometry(&min_width, &min_height);

      min_width                  *= video_scale;
      min_height                 *= video_scale;

      if (settings->bools.video_window_show_decorations)
      {
         int border_thickness     = GetSystemMetrics(SM_CXSIZEFRAME);
         int title_bar_height     = GetSystemMetrics(SM_CYCAPTION);

         extra_width             += border_thickness * 2;
         extra_height            += border_thickness * 2 + title_bar_height;
      }

      if (settings->bools.ui_menubar_enable)
         extra_height            += GetSystemMetrics(SM_CYMENU);

      if (     (     g_win32_resize_width  < min_width
                  || g_win32_resize_height < min_height)
            && min_width  - g_win32_resize_width  < MIN_WIDTH  / 1.5f
            && min_height - g_win32_resize_height < MIN_HEIGHT / 1.5f)
         SetWindowPos(main_window.hwnd, NULL, 0, 0,
               min_width  + extra_width,
               min_height + extra_height,
               SWP_NOMOVE);
   }

   if (g_win32_flags & WIN32_CMN_FLAG_INITED)
      if (GetForegroundWindow() == main_window.hwnd)
         return true;

   return false;
}

HWND win32_get_window(void) { return main_window.hwnd; }

bool win32_suspend_screensaver(void *data, bool enable)
{
   if (enable)
   {
      char tmp[PATH_MAX_LENGTH];
      int major                             = 0;
      int minor                             = 0;
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();

      if (!frontend)
         return false;

      if (frontend->get_os)
         frontend->get_os(tmp, sizeof(tmp), &major, &minor);

      if (major * 100 + minor >= 601)
      {
#if _WIN32_WINNT >= 0x0601
         /* Windows 7, 8, 10 codepath */
         typedef HANDLE(WINAPI * PowerCreateRequestPtr)(REASON_CONTEXT *context);
         typedef BOOL(WINAPI * PowerSetRequestPtr)(HANDLE PowerRequest,
            POWER_REQUEST_TYPE RequestType);
         PowerCreateRequestPtr powerCreateRequest;
         PowerSetRequestPtr    powerSetRequest;
         HMODULE kernel32 = GetModuleHandle("kernel32.dll");

         if (kernel32)
         {
            powerCreateRequest =
               (PowerCreateRequestPtr)GetProcAddress(
                     kernel32, "PowerCreateRequest");
            powerSetRequest =
               (PowerSetRequestPtr)GetProcAddress(
                     kernel32, "PowerSetRequest");

            if (powerCreateRequest && powerSetRequest)
            {
               POWER_REQUEST_CONTEXT RequestContext;
               HANDLE Request;

               RequestContext.Version                   =
                  POWER_REQUEST_CONTEXT_VERSION;
               RequestContext.Flags                     =
                  POWER_REQUEST_CONTEXT_SIMPLE_STRING;
               RequestContext.Reason.SimpleReasonString = (LPWSTR)
                  L"RetroArch running";

               Request                                  =
                  powerCreateRequest(&RequestContext);

               powerSetRequest(Request, PowerRequestDisplayRequired);
               /* TODO/FIXME - handle is never released so
                * technically counts as a memory leak. However, this
                * handle needs to be kept alive so long as the screensaver
                * should be suppressed. So this variable might need to
                * be bookkept somewhere else where it can be properly
                * closed upon shutdown */
               return true;
            }
         }
#endif
      }
      else if (major * 100 + minor >= 410)
      {
#if _WIN32_WINDOWS >= 0x0410 || _WIN32_WINNT >= 0x0410
         /* 98 / 2K / XP / Vista codepath */
         SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
         return true;
#endif
      }
      else
      {
         /* 95 / NT codepath */
         /* No way to block the screensaver. */
         return true;
      }
   }

   return false;
}

static bool win32_monitor_set_fullscreen(
      unsigned width, unsigned height,
      unsigned refresh, bool interlaced, char *dev_name)
{
   DEVMODE devmode;
   memset(&devmode, 0, sizeof(devmode));
   devmode.dmSize             = sizeof(DEVMODE);
   devmode.dmPelsWidth        = width;
   devmode.dmPelsHeight       = height;
   devmode.dmDisplayFrequency = refresh;
   devmode.dmFields           = DM_PELSWIDTH
                              | DM_PELSHEIGHT
                              | DM_DISPLAYFREQUENCY;
#if !(_MSC_VER && (_MSC_VER < 1600))
   devmode.dmDisplayFlags     = interlaced ? DM_INTERLACED : 0;
   if (interlaced)
      devmode.dmFields       |= DM_DISPLAYFLAGS;
#endif
   return win32_change_display_settings(dev_name, &devmode,
         CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
}

void win32_set_style(MONITORINFOEX *current_mon, HMONITOR *hm_to_use,
   unsigned *width, unsigned *height, bool fullscreen, bool windowed_full,
   RECT *rect, RECT *mon_rect, DWORD *style)
{
   settings_t *settings             = config_get_ptr();

   if (fullscreen)
   {
      /* Windows only reports the refresh rates for modelines as
       * an integer, so video_refresh_rate needs to be rounded. Also, account
       * for black frame insertion using video_refresh_rate set to a portion
       * of the display refresh rate, as well as higher vsync swap intervals. */
      float refresh_rate     = settings->floats.video_refresh_rate;
      unsigned bfi           = settings->uints.video_black_frame_insertion;
      unsigned swap_interval = settings->uints.video_swap_interval;
      unsigned
         shader_subframes    = settings->uints.video_shader_subframes;

      /* if refresh_rate is <=60hz, adjust for modifiers, if it is higher
         assume modifiers already factored into setting. Multiplying by
         modifiers will still leave result at original value when they
         are not set. Swap interval 0 is automatic, but at automatic
         we should default to checking for normal SI 1 for rate change*/
      if (swap_interval == 0)
        ++swap_interval;
      if ((int)refresh_rate <= 60)
         refresh_rate     = refresh_rate * (bfi + 1) * swap_interval * shader_subframes;

      if (windowed_full)
      {
         *style                = WS_EX_TOPMOST | WS_POPUP;
         g_win32_resize_width  = *width  = mon_rect->right  - mon_rect->left;
         g_win32_resize_height = *height = mon_rect->bottom - mon_rect->top;
      }
      else
      {
         *style          = WS_POPUP | WS_VISIBLE;

         if (win32_monitor_set_fullscreen(*width, *height,
               (int)refresh_rate, false, current_mon->szDevice))
         {
            RARCH_LOG("[Video] Fullscreen set to %ux%u @ %uHz on device %s.\n",
                  *width, *height, (int)refresh_rate, current_mon->szDevice);
         }

         /* Display settings might have changed, get new coordinates. */
         GetMonitorInfo(*hm_to_use, (LPMONITORINFO)current_mon);
         *mon_rect = current_mon->rcMonitor;
      }
   }
   else
   {
      win32_common_state_t *g_win32    = (win32_common_state_t*)&win32_st;
      bool position_set_from_config    = false;
      bool video_window_save_positions = settings->bools.video_window_save_positions;
      bool window_show_decor           = settings->bools.video_window_show_decorations;

      *style          = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      rect->right     = *width;
      rect->bottom    = *height;

      if (!window_show_decor)
      {
         *style &= ~WS_OVERLAPPEDWINDOW;
         *style |= WS_POPUP;
      }

      AdjustWindowRect(rect, *style, FALSE);

      if (video_window_save_positions)
      {
         /* Set position from config */
         int border_thickness             = window_show_decor ? GetSystemMetrics(SM_CXSIZEFRAME) : 0;
         int title_bar_height             = window_show_decor ? GetSystemMetrics(SM_CYCAPTION) : 0;
         unsigned window_position_width   =
               VIDEO_SCALE_W(settings->uints.window_position_dims);
         unsigned window_position_height  =
               VIDEO_SCALE_H(settings->uints.window_position_dims);

         g_win32->pos                     =
               settings->uints.window_position_pos;
         g_win32->pos_set                 = true;
         g_win32->pos_dims                = VIDEO_SCALE_PACK(
               window_position_width  + border_thickness * 2,
               window_position_height + border_thickness * 2
                  + title_bar_height);

         if (     VIDEO_SCALE_W(g_win32->pos_dims) != 0
               && VIDEO_SCALE_H(g_win32->pos_dims) != 0)
            position_set_from_config = true;
      }

      if (position_set_from_config)
      {
         g_win32_resize_width  = *width   = VIDEO_SCALE_W(g_win32->pos_dims);
         g_win32_resize_height = *height  = VIDEO_SCALE_H(g_win32->pos_dims);
      }
      else
      {
         g_win32_resize_width  = *width   = rect->right  - rect->left;
         g_win32_resize_height = *height  = rect->bottom - rect->top;
      }
   }
}

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data)
{
   RECT *rect            = (RECT*)rect_data;

   if (!fullscreen || windowed_full)
   {
      settings_t *settings      = config_get_ptr();
      const ui_window_t *window = ui_companion_driver_get_window_ptr();
#ifdef HAVE_MENU
      bool ui_menubar_enable    = settings->bools.ui_menubar_enable;

      if (!fullscreen && ui_menubar_enable)
      {
         HMENU menuItem;
         RECT rc_temp;
         rc_temp.left   = 0;
         rc_temp.top    = 0;
         rc_temp.right  = (LONG)*height;
         rc_temp.bottom = 0x7FFF;

         /* (a window that has one keeps it) */
         if (!(menuItem = GetMenu(main_window.hwnd)))
         {
            menuItem = win32_menu_take();
            SetMenu(main_window.hwnd, menuItem);
         }

         SendMessage(main_window.hwnd, WM_NCCALCSIZE, FALSE, (LPARAM)&rc_temp);
         g_win32_resize_height = *height += rc_temp.top + rect->top;
         SetWindowPos(main_window.hwnd, NULL, 0, 0, *width, *height, SWP_NOMOVE);
      }
#endif

      ShowWindow(main_window.hwnd, SW_RESTORE);
      UpdateWindow(main_window.hwnd);
      SetForegroundWindow(main_window.hwnd);

      if (window)
         window->set_focused(&main_window);
   }

   /* the window this one replaces had the foreground */
   win32_foreground_take_back(main_window.hwnd);

   win32_show_cursor(NULL, !fullscreen);
}

/* What the video driver is told is the size the window has now.
 *
 * Setting a window up leaves g_win32_resize_width/height holding sizes
 * worked out on the way - a window's outer size, then one with the
 * menu bar added - and counts on a WM_SIZE that follows to put the
 * client's size there. That message only comes if the size changes,
 * or when a window is first shown. For a window that is already on
 * screen and ends up the size it had, or the size Windows holds it to,
 * none comes and the worked-out size stays: the driver draws for a
 * window of another size than the one on screen, and the picture sits
 * in a part of it. So for a window that was already there - restyled
 * by the fullscreen toggle, or taken back from the last driver - the
 * client's size is put there by hand. */
static void win32_window_tell_client_size(HWND hwnd)
{
   RECT client;
   if (     hwnd
         && GetClientRect(hwnd, &client)
         && client.right  > client.left
         && client.bottom > client.top)
   {
      g_win32_resize_width  = client.right  - client.left;
      g_win32_resize_height = client.bottom - client.top;
      g_win32_flags        |= WIN32_CMN_FLAG_RESIZED;
   }
}

/* The size the window's client area really is, packed as the drivers
 * keep sizes; @dims if there is no window to ask.
 *
 * It is not always the size that was asked for: Windows does not let
 * a window grow past the screen, and a menu bar takes its share. A
 * swap chain made the size asked for is then stretched over the
 * window until the first resize notice has been acted on, a frame or
 * two later - and with a window kept across a restart those frames
 * are on screen. A driver that makes its swap chain this size has
 * nothing to put right. */
unsigned win32_window_client_dims(unsigned dims)
{
   RECT client;
   HWND hwnd = main_window.hwnd;
   if (     hwnd
         && GetClientRect(hwnd, &client)
         && client.right  > client.left
         && client.bottom > client.top)
      return VIDEO_SCALE_PACK(
            (unsigned)(client.right  - client.left),
            (unsigned)(client.bottom - client.top));
   return dims;
}

bool win32_set_video_mode(void *data,
      unsigned dims,
      bool fullscreen)
{
   unsigned width  = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   DWORD style;
   MSG msg;
   RECT mon_rect;
   RECT rect;
   MONITORINFOEX current_mon;
   int res               = 0;
   unsigned mon_id       = 0;
   HMONITOR hm_to_use    = NULL;
   settings_t *settings  = config_get_ptr();
   bool windowed_full    = settings->bools.video_windowed_fullscreen;

   rect.left             = 0;
   rect.top              = 0;
   rect.right            = 0;
   rect.bottom           = 0;

   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);

   mon_rect                    = current_mon.rcMonitor;
   g_win32_resize_width        = width;
   g_win32_resize_height       = height;
   g_win32_refresh_rate        = settings->floats.video_refresh_rate;

   win32_set_style(&current_mon, &hm_to_use, &width, &height,
         fullscreen, windowed_full, &rect, &mon_rect, &style);

   if (!win32_window_create(data, style,
            &mon_rect, width, height, fullscreen))
      return false;

   win32_set_window(&width, &height,
         fullscreen, windowed_full, &rect);

   /* A window taken back from the last driver was on screen all
    * along: no WM_SIZE need have come of any of the above. */
   if (win32_wnd_taken)
      win32_window_tell_client_size(main_window.hwnd);

   /* Wait until context is created (or failed to do so ...).
    * Please don't remove the (res = ) as GetMessage can return -1. */
   while (  !(g_win32_flags & WIN32_CMN_FLAG_INITED)
         && !(g_win32_flags & WIN32_CMN_FLAG_QUIT)
         && (res = GetMessage(&msg, main_window.hwnd, 0, 0)) != 0)
   {
      if (res == -1)
      {
         RARCH_ERR("[Win32] GetMessage error code %d.\n", GetLastError());
         break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (g_win32_flags & WIN32_CMN_FLAG_QUIT)
      return false;

   /* Seed the published mask so the lock states are not reported as
    * clear until the first key event reaches the window procedure. */
   win32_update_keyboard_mods();

   return true;
}

/* For a driver that took the window the last one left up and finds it
 * cannot have it - DXGI will not make a swap chain on it: the window
 * goes and one is made as if none had been kept. The window procedure
 * and its class are as the driver set them up. */
bool win32_window_remake(void *data, unsigned dims, bool fullscreen)
{
   HWND hwnd = main_window.hwnd;

   if (hwnd)
   {
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
      if (notification_handler)
         UnregisterDeviceNotification(notification_handler);
      notification_handler = NULL;
#endif
      main_window.hwnd     = NULL;
      video_driver_window_set(0);
      win32_retiring_hwnd  = hwnd;
      DestroyWindow(hwnd);
      win32_retiring_hwnd  = NULL;
   }
   win32_wnd_taken         = false;
   g_win32_flags          &= ~WIN32_CMN_FLAG_INITED;
   RARCH_WARN("[Win32] The window taken from the last video driver could"
         " not be used: making a new one.\n");
   return win32_set_video_mode(data, dims, fullscreen);
}

bool win32_fullscreen_in_place(void)
{
   static int in_place = -1;
   if (in_place < 0)
   {
      const char *env = getenv("RETROARCH_FULLSCREEN_IN_PLACE");
      in_place        = (env && env[0] == '0') ? 0 : 1;
   }
   return in_place != 0;
}

/* Takes the window that is already there between windowed and
 * borderless fullscreen: its style, position and size change, and it
 * stays the same window.
 *
 * A fullscreen toggle used to mean a full driver restart - window,
 * video device, shaders, audio, input drivers, controllers detected
 * again - because the only way to a window of the other kind was to
 * destroy this one and create another. For a borderless fullscreen
 * window nothing but the window differs. The size it ends up with
 * reaches the video driver as any resize does, through WM_SIZE.
 *
 * Not for exclusive fullscreen, which changes the display mode: that
 * answers false and is left to the restart.
 *
 * Has to run on the thread that owns the window; a video driver's
 * set_video_mode does. */
bool win32_window_set_fullscreen(unsigned dims, bool fullscreen)
{
   DWORD style;
   RECT mon_rect;
   RECT rect;
   MONITORINFOEX current_mon;
   int x, y;
   unsigned width       = VIDEO_SCALE_W(dims);
   unsigned height      = VIDEO_SCALE_H(dims);
   unsigned mon_id      = 0;
   HMONITOR hm_to_use   = NULL;
   HWND hwnd            = main_window.hwnd;
   settings_t *settings = config_get_ptr();
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   /* where the window stood before it last went fullscreen */
   static RECT windowed_rect;
   static HWND windowed_rect_of;

   if (!hwnd || !settings->bools.video_windowed_fullscreen)
      return false;

   /* fullscreen on the monitor the window is on, as a restart has it */
   win32_monitor_last = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
   mon_rect           = current_mon.rcMonitor;

   rect.left          = 0;
   rect.top           = 0;
   rect.right         = 0;
   rect.bottom        = 0;

   if (fullscreen && !(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_POPUP))
   {
      GetWindowRect(hwnd, &windowed_rect);
      windowed_rect_of = hwnd;
   }

   win32_set_style(&current_mon, &hm_to_use, &width, &height,
         fullscreen, true, &rect, &mon_rect, &style);

   if (fullscreen)
   {
      /* the menu bar belongs to the windowed state: kept until then */
      win32_menu_put_away(hwnd);
      x = mon_rect.left;
      y = mon_rect.top;
   }
   else if (g_win32->pos_set)
   {
      x = VIDEO_POS_X(g_win32->pos);
      y = VIDEO_POS_Y(g_win32->pos);
   }
   else if (windowed_rect_of == hwnd)
   {
      x = windowed_rect.left;
      y = windowed_rect.top;
   }
   else
   {
      /* never windowed yet: the middle of its monitor */
      x = mon_rect.left + ((mon_rect.right  - mon_rect.left) - (int)width)  / 2;
      y = mon_rect.top  + ((mon_rect.bottom - mon_rect.top)  - (int)height) / 2;
      if (x < mon_rect.left)
         x = mon_rect.left;
      if (y < mon_rect.top)
         y = mon_rect.top;
   }

   SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_VISIBLE);
   SetWindowPos(hwnd, HWND_TOP, x, y, width, height,
         SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

   win32_set_window(&width, &height, fullscreen, true, &rect);

   /* what the video driver is told is the size the window has now */
   win32_window_tell_client_size(hwnd);

   return true;
}
#endif

bool win32_get_client_rect(RECT* rect)
{
   return GetClientRect(main_window.hwnd, rect);
}

void win32_window_reset(void)
{
   g_win32_flags &= ~(WIN32_CMN_FLAG_QUIT
                    | WIN32_CMN_FLAG_RESTORE_DESKTOP);
}

void win32_destroy_window(void)
{
#ifndef _XBOX
   UnregisterClass("RetroArch",
         GetModuleHandle(NULL));
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x500 /* 2K */
   UnregisterDeviceNotification(notification_handler);
#endif
#endif
   main_window.hwnd = NULL;
   /* video_st->window is a copy of this handle taken by
    * win32_window_create(). Nothing else clears it - video_driver.c
    * only resets it at the top of the next
    * video_driver_init_internal() - so without this the two disagree
    * from here until the next video driver init, and
    * video_driver_window_get() hands out a destroyed HWND in between. */
   video_driver_window_set(0);
}

/* --- HDR (scRGB) pixel format support -------------------------------
 *
 * Selecting an FP16 backbuffer needs wglChoosePixelFormatARB, which can
 * only be resolved with a live GL context, while SetPixelFormat is
 * once-per-window: the classic WGL bootstrap problem.  It is solved the
 * classic way, with a throwaway hidden window + legacy context used only
 * to resolve the ARB entry points, torn down before the real window's
 * format is set.
 *
 * Backwards compatibility is the primary constraint here: the legacy
 * ChoosePixelFormat path below is byte-for-byte untouched and remains
 * the default.  The float path only runs when (1) the caller is a GL
 * context, (2) the user enabled HDR, (3) the display is actually in HDR
 * mode (probed through the dynamically-loaded DXGI helper, so no new
 * link-time imports and no behavior on systems without it), and (4) the
 * ARB float-pixel-format extensions resolve and produce a format.  Any
 * failure at any step leaves the window untouched and falls through to
 * the legacy path. */

static bool win32_scrgb_backbuffer = false;

bool win32_backbuffer_is_scrgb(void)
{
   return win32_scrgb_backbuffer;
}

#if !defined(_XBOX) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE) || defined(HAVE_OPENGL1))

#ifndef WGL_DRAW_TO_WINDOW_ARB
#define WGL_DRAW_TO_WINDOW_ARB    0x2001
#endif
#ifndef WGL_ACCELERATION_ARB
#define WGL_ACCELERATION_ARB      0x2003
#endif
#ifndef WGL_SUPPORT_OPENGL_ARB
#define WGL_SUPPORT_OPENGL_ARB    0x2010
#endif
#ifndef WGL_DOUBLE_BUFFER_ARB
#define WGL_DOUBLE_BUFFER_ARB     0x2011
#endif
#ifndef WGL_PIXEL_TYPE_ARB
#define WGL_PIXEL_TYPE_ARB        0x2013
#endif
#ifndef WGL_RED_BITS_ARB
#define WGL_RED_BITS_ARB          0x2015
#endif
#ifndef WGL_GREEN_BITS_ARB
#define WGL_GREEN_BITS_ARB        0x2017
#endif
#ifndef WGL_BLUE_BITS_ARB
#define WGL_BLUE_BITS_ARB         0x2019
#endif
#ifndef WGL_ALPHA_BITS_ARB
#define WGL_ALPHA_BITS_ARB        0x201B
#endif
#ifndef WGL_DEPTH_BITS_ARB
#define WGL_DEPTH_BITS_ARB        0x2022
#endif
#ifndef WGL_STENCIL_BITS_ARB
#define WGL_STENCIL_BITS_ARB      0x2023
#endif
#ifndef WGL_FULL_ACCELERATION_ARB
#define WGL_FULL_ACCELERATION_ARB 0x2027
#endif
/* WGL_ARB_pixel_format_float; WGL_ATI_pixel_format_float uses the
 * same token value. */
#ifndef WGL_TYPE_RGBA_FLOAT_ARB
#define WGL_TYPE_RGBA_FLOAT_ARB   0x21A0
#endif
/* WGL_EXT_colorspace */
#ifndef WGL_COLORSPACE_EXT
#define WGL_COLORSPACE_EXT        0x309D
#endif
#ifndef WGL_COLORSPACE_LINEAR_EXT
#define WGL_COLORSPACE_LINEAR_EXT 0x308A
#endif

#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12)
/* Implemented in dxgi_common.c; declared here rather than by including
 * dxgi_common.h, so this TU pulls no COM/DXGI headers: their
 * C-interface setup (CINTERFACE / COBJMACROS before any Windows
 * include) differs from what the rest of this file establishes, and
 * MSVC against the real SDK headers rejects the mix -- MinGW's
 * headers are lenient and masked it. */
bool dxgi_display_hdr_active(HWND hwnd);
#endif

/* Display-in-HDR-mode probe.  Goes through the DXGI helper, which
 * dylib_loads dxgi.dll at runtime -- no new imports; on systems
 * without DXGI 1.6 / HDR it simply reports false and the legacy
 * pixel format is used. */
bool win32_display_hdr_active(HWND hwnd)
{
#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12)
   return dxgi_display_hdr_active(hwnd);
#else
   /* No DXGI in this build: no way to know the display is in HDR
    * mode, so never select the float format. */
   return false;
#endif
}

/* Resolve wglChoosePixelFormatARB via a throwaway window + context and
 * pick an FP16 format for target_hdc.  Returns the pixel format index,
 * or 0 on any failure (extension missing, no format, etc.); the dummy
 * window and context are always torn down. */
static int win32_try_scrgb_pixel_format(HDC target_hdc)
{
   typedef BOOL (WINAPI *choose_fmt_t)(HDC, const int*, const FLOAT*,
         UINT, int*, UINT*);
   typedef const char *(WINAPI *get_exts_t)(HDC);
   WNDCLASSEXA  wc;
   HWND         dummy_wnd = NULL;
   HDC          dummy_dc  = NULL;
   HGLRC        dummy_rc  = NULL;
   HDC          prev_dc   = NULL;
   HGLRC        prev_rc   = NULL;
   int          result    = 0;
   static const char *probe_class = "RetroArch-WGL-Probe";

   memset(&wc, 0, sizeof(wc));
   wc.cbSize        = sizeof(wc);
   wc.style         = CS_OWNDC;
   wc.lpfnWndProc   = DefWindowProcA;
   wc.hInstance     = GetModuleHandle(NULL);
   wc.lpszClassName = probe_class;

   if (!RegisterClassExA(&wc))
      return 0;

   dummy_wnd = CreateWindowExA(0, probe_class, "", WS_OVERLAPPED,
         0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
   if (dummy_wnd)
      dummy_dc = GetDC(dummy_wnd);

   if (dummy_dc)
   {
      int pf;
      PIXELFORMATDESCRIPTOR pfd = {0};
      pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
      pfd.nVersion   = 1;
      pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
                     | PFD_DOUBLEBUFFER;
      pfd.iPixelType = PFD_TYPE_RGBA;
      pfd.cColorBits = 32;
      pfd.iLayerType = PFD_MAIN_PLANE;

      pf = ChoosePixelFormat(dummy_dc, &pfd);
      if (pf && SetPixelFormat(dummy_dc, pf, &pfd))
      {
         prev_dc  = wglGetCurrentDC();
         prev_rc  = wglGetCurrentContext();
         dummy_rc = wglCreateContext(dummy_dc);
         if (dummy_rc && wglMakeCurrent(dummy_dc, dummy_rc))
         {
            get_exts_t   get_exts   = (get_exts_t)
                  wglGetProcAddress("wglGetExtensionsStringARB");
            choose_fmt_t choose_fmt = (choose_fmt_t)
                  wglGetProcAddress("wglChoosePixelFormatARB");
            const char  *exts       = get_exts
                  ? get_exts(dummy_dc) : NULL;

            if (     choose_fmt && exts
                  && strstr(exts, "WGL_ARB_pixel_format")
                  && (   strstr(exts, "WGL_ARB_pixel_format_float")
                      || strstr(exts, "WGL_ATI_pixel_format_float")))
            {
               int  fmt      = 0;
               UINT num_fmts = 0;
               int  attribs[26];
               int  n        = 0;

               attribs[n++] = WGL_DRAW_TO_WINDOW_ARB; attribs[n++] = 1;
               attribs[n++] = WGL_SUPPORT_OPENGL_ARB; attribs[n++] = 1;
               attribs[n++] = WGL_DOUBLE_BUFFER_ARB;  attribs[n++] = 1;
               attribs[n++] = WGL_ACCELERATION_ARB;
               attribs[n++] = WGL_FULL_ACCELERATION_ARB;
               attribs[n++] = WGL_PIXEL_TYPE_ARB;
               attribs[n++] = WGL_TYPE_RGBA_FLOAT_ARB;
               attribs[n++] = WGL_RED_BITS_ARB;       attribs[n++] = 16;
               attribs[n++] = WGL_GREEN_BITS_ARB;     attribs[n++] = 16;
               attribs[n++] = WGL_BLUE_BITS_ARB;      attribs[n++] = 16;
               attribs[n++] = WGL_ALPHA_BITS_ARB;     attribs[n++] = 16;
               attribs[n++] = WGL_DEPTH_BITS_ARB;     attribs[n++] = 0;
               attribs[n++] = WGL_STENCIL_BITS_ARB;   attribs[n++] = 0;
               /* Formalize the linear (scRGB) interpretation of the
                * float buffer where the driver supports saying so.
                * Linear is also the extension's documented default, so
                * this cannot change behavior on conforming drivers --
                * and it is only passed when advertised, since unknown
                * attributes can fail the choose call on others. */
               if (strstr(exts, "WGL_EXT_colorspace"))
               {
                  attribs[n++] = WGL_COLORSPACE_EXT;
                  attribs[n++] = WGL_COLORSPACE_LINEAR_EXT;
               }
               attribs[n]   = 0;

               if (     choose_fmt(target_hdc, attribs, NULL,
                              1, &fmt, &num_fmts)
                     && num_fmts >= 1)
                  result = fmt;
            }
         }
      }
   }

   /* Tear down the probe completely, restoring whatever context was
    * current before (normally none this early). */
   wglMakeCurrent(prev_dc, prev_rc);
   if (dummy_rc)
      wglDeleteContext(dummy_rc);
   if (dummy_dc)
      ReleaseDC(dummy_wnd, dummy_dc);
   if (dummy_wnd)
      DestroyWindow(dummy_wnd);
   UnregisterClassA(probe_class, wc.hInstance);

   return result;
}
#endif /* !_XBOX && GL */

void win32_setup_pixel_format(HDC hdc, bool supports_gl)
{
   int pf;
   PIXELFORMATDESCRIPTOR pfd = {0};

   win32_scrgb_backbuffer = false;

#if !defined(_XBOX) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE) || defined(HAVE_OPENGL1))
   /* HDR: try an FP16 (scRGB) backbuffer, strictly opt-in and with the
    * legacy path as the fallback for every possible failure.  Windows
    * has no WGL colorspace API; the vendor contract is that an FP16
    * backbuffer under an HDR display is composited as scRGB
    * (1.0 = 80 nits). */
   if (supports_gl)
   {
      settings_t *settings = config_get_ptr();
      if (settings && settings->uints.video_hdr_mode > 0)
      {
         if (win32_display_hdr_active(WindowFromDC(hdc)))
         {
            int fpf = win32_try_scrgb_pixel_format(hdc);
            if (fpf)
            {
               PIXELFORMATDESCRIPTOR fpfd = {0};
               fpfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
               DescribePixelFormat(hdc, fpf, sizeof(fpfd), &fpfd);
               /* (a window taken back from the last driver has it
                * already; it can be set only once) */
               if (     GetPixelFormat(hdc) == fpf
                     || SetPixelFormat(hdc, fpf, &fpfd))
               {
                  win32_scrgb_backbuffer = true;
                  RARCH_LOG("[Win32] Using FP16 scRGB backbuffer for HDR.\n");
                  if (settings->uints.video_hdr_mode == 1)
                     RARCH_LOG("[Win32] OpenGL HDR output is scRGB-only; HDR10 setting maps to scRGB.\n");
                  return;
               }
               RARCH_WARN("[Win32] FP16 SetPixelFormat failed; using SDR pixel format.\n");
            }
            else
               RARCH_LOG("[Win32] FP16 pixel format unavailable; using SDR pixel format.\n");
         }
         else
            RARCH_LOG("[Win32] HDR requested but display is not in HDR mode; using SDR pixel format.\n");
      }
   }
#endif

   pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
   pfd.nVersion     = 1;
   pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
   pfd.iPixelType   = PFD_TYPE_RGBA;
   pfd.cColorBits   = 32;
   pfd.cDepthBits   = 0;
   pfd.cStencilBits = 0;
   pfd.iLayerType   = PFD_MAIN_PLANE;

   if (supports_gl)
      pfd.dwFlags  |= PFD_SUPPORT_OPENGL;

   pf = ChoosePixelFormat(hdc, &pfd);
   if (     pf == 0
         || (   GetPixelFormat(hdc) != pf
             && !SetPixelFormat(hdc, pf, &pfd)))
      RARCH_ERR("[Win32] Failed to set pixel format.\n");
}

/* For a window that already has a pixel format - one left up by the
 * last OpenGL driver: whether it is the format
 * win32_setup_pixel_format() would give the window now. It follows
 * that function's choice step for step: the FP16 format when HDR is
 * asked for, the display is in HDR mode and the format can be had,
 * the plain one otherwise. */
static bool win32_window_pixel_format_fits(HWND hwnd)
{
   PIXELFORMATDESCRIPTOR pfd = {0};
   settings_t *settings      = config_get_ptr();
   HDC hdc                   = GetDC(hwnd);
   int cur, want             = 0;

   if (!hdc)
      return false;
   cur = GetPixelFormat(hdc);
   if (cur)
   {
#if !defined(_XBOX) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE) || defined(HAVE_OPENGL1))
      if (     settings
            && settings->uints.video_hdr_mode > 0
            && win32_display_hdr_active(hwnd))
         want          = win32_try_scrgb_pixel_format(hdc);
#endif
      if (!want)
      {
         pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
         pfd.nVersion     = 1;
         pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER
                          | PFD_SUPPORT_OPENGL;
         pfd.iPixelType   = PFD_TYPE_RGBA;
         pfd.cColorBits   = 32;
         pfd.cDepthBits   = 0;
         pfd.cStencilBits = 0;
         pfd.iLayerType   = PFD_MAIN_PLANE;
         want             = ChoosePixelFormat(hdc, &pfd);
      }
   }
   ReleaseDC(hwnd, hdc);
   /* none set yet: whatever is wanted can be */
   return !cur || want == cur;
}

#ifndef __WINRT__
unsigned short win32_get_langid_from_retro_lang(enum retro_language lang);

bool win32_window_init(WNDCLASSEX *wndclass,
      bool fullscreen, const char *class_name)
{
#if _WIN32_WINNT >= 0x0501
   /* Use the language set in the config for the menubar...
    * also changes the console language. */
   SetThreadUILanguage(win32_get_langid_from_retro_lang(
            (enum retro_language)
            *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)));
#endif
   wndclass->cbSize           = sizeof(WNDCLASSEX);
   wndclass->style            = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndclass->hInstance        = GetModuleHandle(NULL);
   wndclass->hCursor          = LoadCursor(NULL, IDC_ARROW);
   wndclass->lpszClassName    = class_name ? class_name : "RetroArch";
   wndclass->hIcon            = LoadIcon(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON));
   wndclass->hIconSm          = (HICON)LoadImage(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);

   if (GetSystemMetrics(SM_SWAPBUTTON))
      g_win32_flags          |=  WIN32_CMN_FLAG_SWAP_MOUSE_BTNS;
   else
      g_win32_flags          &= ~WIN32_CMN_FLAG_SWAP_MOUSE_BTNS;

   if (!fullscreen)
      wndclass->hbrBackground = (HBRUSH)COLOR_WINDOW;

   if (class_name)
      wndclass->style        |= CS_CLASSDC;

#ifdef HAVE_D3DKMT
   d3dkmt_init();
#endif

   /* still registered, for a window left up across the restart
    * (win32_window_keep()): one procedure serves every family, so
    * the class is the same one */
   {
      WNDCLASSEX has;
      has.cbSize = sizeof(has);
      if (GetClassInfoEx(wndclass->hInstance, wndclass->lpszClassName, &has))
         return true;
   }

   return RegisterClassEx(wndclass);
}

/* ----------------------------------------------------------------
 * PROGRAMMATIC WIN32 RESOURCES
 *
 * Replaces the menu, dialog, accelerator, and manifest resources
 * formerly in media/rarch.rc and media/rarch_ja.rc.
 *
 * The icon resource remains in rarch.rc so the executable has
 * an embedded icon visible in Explorer / taskbar / Alt+Tab.
 *
 *   IDR_MENU          → win32_resources_create_menu()  [in ui_win32.c]
 *   IDR_ACCELERATOR1  → win32_resources_get_accelerator()
 *   IDD_PICKCORE      → win32_resources_pick_core_dialog()  [in ui_win32.c]
 *   rarch.manifest    → win32_apply_dpi_awareness()
 *                       (called from the top of rarch_main, before
 *                        any window is created)
 * ---------------------------------------------------------------- */

static HACCEL s_accel_table = NULL;

/* DPI AWARENESS  (replaces media/rarch.manifest)
 * The manifest contained <dpiAware>true</dpiAware>.
 * We call the equivalent API at runtime.
 *
 * Must be called before the process creates any HWND (direct or
 * transitive, e.g. via CoInitialize or AllocConsole).  Once any
 * top-level window exists, SetProcessDpiAwareness returns
 * E_ACCESSDENIED and the process stays Unaware — meaning GetDeviceCaps
 * reports a fixed 96 DPI regardless of monitor or scaling settings.
 * See call site in retroarch.c (top of rarch_main). */
typedef HRESULT (WINAPI *pfn_SetProcessDpiAwareness)(int);
typedef BOOL    (WINAPI *pfn_SetProcessDPIAware)(void);

void win32_apply_dpi_awareness(void)
{
#ifdef HAVE_DYLIB
   dylib_t lib;

   /* Windows 8.1+: SetProcessDpiAwareness in shcore.dll. */
   if ((lib = dylib_load("shcore.dll")))
   {
      pfn_SetProcessDpiAwareness fn = (pfn_SetProcessDpiAwareness)
         dylib_proc(lib, "SetProcessDpiAwareness");
      if (fn)
      {
         fn(1); /* PROCESS_SYSTEM_DPI_AWARE */
         dylib_close(lib);
         return;
      }
      dylib_close(lib);
   }

   /* Vista / Win 7 / Win 8 fallback: SetProcessDPIAware in user32.dll. */
   if ((lib = dylib_load("user32.dll")))
   {
      pfn_SetProcessDPIAware fn = (pfn_SetProcessDPIAware)
         dylib_proc(lib, "SetProcessDPIAware");
      if (fn)
         fn();
      dylib_close(lib);
   }
   /* Older than Vista: no API available; process stays DPI-Unaware,
    * which is the correct behaviour for those systems anyway. */
#endif
}

/* ACCELERATOR TABLE  (replaces IDR_ACCELERATOR1)
 *   Ctrl+O     → ID_M_LOAD_CONTENT
 *   Alt+Enter  → ID_M_FULL_SCREEN */
static HACCEL create_accelerator_table(void)
{
   ACCEL accel[2];
   accel[0].fVirt = FCONTROL | FVIRTKEY | FNOINVERT;
   accel[0].key   = 'O';
   accel[0].cmd   = ID_M_LOAD_CONTENT;
   accel[1].fVirt = FALT | FVIRTKEY | FNOINVERT;
   accel[1].key   = VK_RETURN;
   accel[1].cmd   = ID_M_FULL_SCREEN;
   return CreateAcceleratorTableW(accel, 2);
}


void win32_resources_init(void)
{
   /* NOTE: DPI awareness is applied separately, at the very top of
    * rarch_main(), to guarantee it runs before any window is created
    * (including the hidden OLE window CoInitialize may create).
    * See win32_apply_dpi_awareness(). */
   s_accel_table = create_accelerator_table();
}

void win32_resources_free(void)
{
   if (s_accel_table)
   {
      DestroyAcceleratorTable(s_accel_table);
      s_accel_table = NULL;
   }
}

HACCEL win32_resources_get_accelerator(void)
{
   return s_accel_table;
}
#endif /* !__WINRT__ */

