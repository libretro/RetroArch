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
#endif /* !defined(_XBOX) */
#include <math.h>
#include <wchar.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "win32_common.h"


#ifdef HAVE_GDI
#include "gdi_defines.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../paths.h"
#include "../../retroarch.h"
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


typedef struct win32_common_state
{
   int pos_x;
   int pos_y;
   unsigned pos_width;
   unsigned pos_height;
#ifdef HAVE_TASKBAR
   unsigned taskbar_message;
#endif
   unsigned monitor_count;
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
   CW_USEDEFAULT,       /* pos_x */
   CW_USEDEFAULT,       /* pos_y */
   0,                   /* pos_width */
   0,                   /* pos_height */
#ifdef HAVE_TASKBAR
   0,                   /* taskbar_message */
#endif
   0,                   /* monitor_count */
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
void win32_monitor_from_window(void)
{
   ui_window_t *window       = NULL;

   win32_monitor_last        =
      MonitorFromWindow(main_window.hwnd, MONITOR_DEFAULTTONEAREST);

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
   unsigned fs_monitor   = settings->uints.video_monitor_index;
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
      unsigned *width, unsigned *height)
{
   HWND         window     = win32_get_window();

   if (window)
   {
      *width               = g_win32_resize_width;
      *height              = g_win32_resize_height;
   }
   else
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id      = 0;
      HMONITOR hm_to_use   = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect             = current_mon.rcMonitor;
      *width               = mon_rect.right - mon_rect.left;
      *height              = mon_rect.bottom - mon_rect.top;
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
         g_win32->pos_x      = placement.rcNormalPosition.left;
         g_win32->pos_y      = placement.rcNormalPosition.top;
      }

      if (GetWindowRect(main_window.hwnd, &rect))
      {
         g_win32->pos_width  = rect.right  - rect.left;
         g_win32->pos_height = rect.bottom - rect.top;
      }
   }
   else
      settings->flags &= ~SETTINGS_FLG_SKIP_WINDOW_POSITIONS;

   if (window_save_positions)
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      uint32_t video_st_flags        = video_st->flags;
      bool video_fullscreen          = settings->bools.video_fullscreen;

      if (     !video_fullscreen
            && !(video_st_flags & VIDEO_FLAG_FORCE_FULLSCREEN)
            && !(video_st_flags & VIDEO_FLAG_IS_SWITCHING_DISPLAY_MODE))
      {
         bool ui_menubar_enable                     = settings->bools.ui_menubar_enable;
         bool window_show_decor                     = settings->bools.video_window_show_decorations;
         settings->uints.window_position_x          = g_win32->pos_x;
         settings->uints.window_position_y          = g_win32->pos_y;
         settings->uints.window_position_width      = g_win32->pos_width;
         settings->uints.window_position_height     = g_win32->pos_height;
         if (window_show_decor)
         {
            int border_thickness                    = GetSystemMetrics(SM_CXSIZEFRAME);
            int title_bar_height                    = GetSystemMetrics(SM_CYCAPTION);
            settings->uints.window_position_width  -= border_thickness * 2;
            settings->uints.window_position_height -= border_thickness * 2;
            settings->uints.window_position_height -= title_bar_height;
         }
         if (ui_menubar_enable)
         {
            int menu_bar_height   = GetSystemMetrics(SM_CYMENU);
            settings->uints.window_position_height -= menu_bar_height;
         }
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

static LRESULT CALLBACK wnd_proc_common(
      bool *quit, HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
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

            if (GetKeyState(VK_SHIFT)   & 0x80)
               mod |= RETROKMOD_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x80)
               mod |= RETROKMOD_CTRL;
            if (GetKeyState(VK_MENU)    & 0x80)
               mod |= RETROKMOD_ALT;
            if (GetKeyState(VK_CAPITAL) & 0x81)
               mod |= RETROKMOD_CAPSLOCK;
            if (GetKeyState(VK_SCROLL)  & 0x81)
               mod |= RETROKMOD_SCROLLOCK;
            if (GetKeyState(VK_NUMLOCK) & 0x81)
               mod |= RETROKMOD_NUMLOCK;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
               mod |= RETROKMOD_META;

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
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
         *quit          = true;
         /* fall-through */
      case WM_MOVE:
         win32_save_position();
         break;
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
                     task_push_load_new_core(
                           td->path, NULL,
                           &content_info,
                           CORE_TYPE_PLAIN,
                           NULL, NULL);
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

static LRESULT CALLBACK wnd_proc_common_internal(HWND hwnd,
      UINT message, WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool keydown                  = true;
   bool quit                     = false;
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   switch (message)
   {
      case WM_KEYUP:                /* Key released */
      case WM_SYSKEYUP:             /* Key released */
         keydown                  = false;
         /* fall-through */
      case WM_KEYDOWN:              /* Key pressed  */
      case WM_SYSKEYDOWN:           /* Key pressed  */
         quit                     = true;
         {
            uint16_t mod          = 0;
            unsigned keycode      = 0;
            unsigned keysym       = (lparam >> 16) & 0xff;
            bool extended         = (lparam >> 24) & 0x1;

            /* NumLock vs Pause correction */
            if (keysym == 0x45 && (wparam == VK_NUMLOCK || wparam == VK_PAUSE))
               extended = !extended;

            /* extended keys will map to dinput if the high bit is set */
            if (extended)
               keysym |= 0x80;

            keycode = input_keymaps_translate_keysym_to_rk(keysym);

            if (GetKeyState(VK_SHIFT)   & 0x80)
               mod |= RETROKMOD_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x80)
               mod |= RETROKMOD_CTRL;
            if (GetKeyState(VK_MENU)    & 0x80)
               mod |= RETROKMOD_ALT;
            if (GetKeyState(VK_CAPITAL) & 0x81)
               mod |= RETROKMOD_CAPSLOCK;
            if (GetKeyState(VK_SCROLL)  & 0x81)
               mod |= RETROKMOD_SCROLLOCK;
            if (GetKeyState(VK_NUMLOCK) & 0x81)
               mod |= RETROKMOD_NUMLOCK;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
               mod |= RETROKMOD_META;

            input_keyboard_event(keydown, keycode,
                  0, mod, RETRO_DEVICE_KEYBOARD);

            if (message != WM_SYSKEYDOWN)
               return 0;

            if (
                     wparam == VK_F10
                  || wparam == VK_MENU
                  || wparam == VK_RSHIFT
               )
               return 0;
         }
         break;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_DEVICECHANGE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
#ifdef HAVE_THREADS
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
#endif
         ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
      case WM_SETFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(true);
#endif
         break;
      case WM_KILLFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(false);
#endif
         break;
      case WM_DISPLAYCHANGE:  /* Fix size after display mode switch when using SR */
         {
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (mon)
               win32_resize_after_display_change(hwnd, mon);
         }
         break;
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}

#ifdef HAVE_WINRAWINPUT
static LRESULT CALLBACK wnd_proc_winraw_common_internal(HWND hwnd,
      UINT message, WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool quit                     = false;
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   switch (message)
   {
      case WM_KEYUP:                /* Key released */
      case WM_SYSKEYUP:             /* Key released */
         /* fall-through */
      case WM_KEYDOWN:              /* Key pressed  */
      case WM_SYSKEYDOWN:           /* Key pressed  */
         quit                     = true;
         if (message != WM_SYSKEYDOWN)
            return 0;

         /* keyboard_event in winraw_callback */

         if (
                  wparam == VK_F10
               || wparam == VK_MENU
               || wparam == VK_RSHIFT
            )
            return 0;
         break;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
#ifdef HAVE_THREADS
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
#endif
         ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
      case WM_SETFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(true);
#endif
#if !defined(_XBOX)
         if (winraw_handle_message(message, wparam, lparam))
            return 0;
#endif
         break;
      case WM_KILLFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(false);
#endif
#if !defined(_XBOX)
         if (winraw_handle_message(message, wparam, lparam))
            return 0;
#endif
         break;
      case WM_DISPLAYCHANGE:  /* Fix size after display mode switch when using SR */
         {
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (mon)
               win32_resize_after_display_change(hwnd, mon);
         }
         break;
      case WM_DEVICECHANGE:
#if !defined(_XBOX)
         if (winraw_handle_message(message, wparam, lparam))
            return 0;
#endif
         break;
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "Imm32")
#endif

#ifdef HAVE_DINPUT
static LRESULT CALLBACK wnd_proc_common_dinput_internal(HWND hwnd,
      UINT message, WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool keydown                  = true;
   bool quit                     = false;
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   switch (message)
   {
      case WM_IME_ENDCOMPOSITION:
         input_keyboard_event(true, 1, 0x80000000, 0, RETRO_DEVICE_KEYBOARD);
         break;
      case WM_IME_COMPOSITION:
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
         break;
      case WM_KEYUP:                /* Key released */
      case WM_SYSKEYUP:             /* Key released */
         keydown                  = false;
         /* fall-through */
      case WM_KEYDOWN:              /* Key pressed  */
      case WM_SYSKEYDOWN:           /* Key pressed  */
         quit                     = true;
         {
            uint16_t mod          = 0;
            unsigned keycode      = 0;
            unsigned keysym       = (lparam >> 16) & 0xff;
            bool extended         = (lparam >> 24) & 0x1;

            /* NumLock vs Pause correction */
            if (keysym == 0x45 && (wparam == VK_NUMLOCK || wparam == VK_PAUSE))
               extended = !extended;

            /* extended keys will map to dinput if the high bit is set */
            if (extended)
               keysym |= 0x80;

            /* tell the driver about shift and alt key events */
            if (        keysym == 0x2A/*DIK_LSHIFT*/
                     || keysym == 0x36/*DIK_RSHIFT*/
                     || keysym == 0x38/*DIK_LMENU*/
                     || keysym == 0xB8/*DIK_RMENU*/)
            {
               void* input_data = (void*)(LONG_PTR)GetWindowLongPtr(main_window.hwnd, GWLP_USERDATA);
               if (input_data && dinput_handle_message(input_data,
                        message, wparam, lparam))
                  return 0; /* key up already handled by the driver */
            }

            keycode = input_keymaps_translate_keysym_to_rk(keysym);

            if (GetKeyState(VK_SHIFT)   & 0x80)
               mod |= RETROKMOD_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x80)
               mod |= RETROKMOD_CTRL;
            if (GetKeyState(VK_MENU)    & 0x80)
               mod |= RETROKMOD_ALT;
            if (GetKeyState(VK_CAPITAL) & 0x81)
               mod |= RETROKMOD_CAPSLOCK;
            if (GetKeyState(VK_SCROLL)  & 0x81)
               mod |= RETROKMOD_SCROLLOCK;
            if (GetKeyState(VK_NUMLOCK) & 0x81)
               mod |= RETROKMOD_NUMLOCK;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
               mod |= RETROKMOD_META;

            input_keyboard_event(keydown, keycode,
                  0, mod, RETRO_DEVICE_KEYBOARD);

            if (message != WM_SYSKEYDOWN)
               return 0;

            if (
                     wparam == VK_F10
                  || wparam == VK_MENU
                  || wparam == VK_RSHIFT
               )
               return 0;
         }
         break;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_DEVICECHANGE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
#if !defined(_XBOX)
         {
            void* input_data = (void*)(LONG_PTR)GetWindowLongPtr(main_window.hwnd, GWLP_USERDATA);
            if (input_data && dinput_handle_message(input_data,
                     message, wparam, lparam))
               return 0;
         }
#endif
         break;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
#ifdef HAVE_THREADS
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
#endif
         ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
      case WM_SETFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(true);
#endif
#if !defined(_XBOX)
         {
            void* input_data = (void*)(LONG_PTR)GetWindowLongPtr(main_window.hwnd, GWLP_USERDATA);
            if (input_data && dinput_handle_message(input_data,
                     message, wparam, lparam))
               return 0;
         }
#endif
         break;
      case WM_KILLFOCUS:
#ifdef HAVE_CLIP_WINDOW
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(false);
#endif
#if !defined(_XBOX)
         {
            void* input_data = (void*)(LONG_PTR)GetWindowLongPtr(main_window.hwnd, GWLP_USERDATA);
            if (input_data && dinput_handle_message(input_data,
                     message, wparam, lparam))
               return 0;
         }
#endif
         break;
      case WM_DISPLAYCHANGE:  /* Fix size after display mode switch when using SR */
         {
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (mon)
               win32_resize_after_display_change(hwnd, mon);
         }
         break;
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

#if defined(HAVE_D3D) || defined(HAVE_D3D8) || defined(HAVE_D3D9) || defined (HAVE_D3D10) || defined (HAVE_D3D11) || defined (HAVE_D3D12)
LRESULT CALLBACK wnd_proc_d3d_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
   {
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);

      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
}

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_d3d_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
   {
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);

      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_d3d_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
   {
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);

      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#endif

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

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_wgl_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wgl_wm_create(hwnd);
   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_wgl_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wgl_wm_create(hwnd);
   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK wnd_proc_wgl_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wgl_wm_create(hwnd);
   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
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
            width, height, win32_vk_interval))
      g_win32_flags |= WIN32_CMN_FLAG_QUIT;
   g_win32_flags    |= WIN32_CMN_FLAG_INITED;
   if (DragAcceptFiles_func)
      DragAcceptFiles_func(hwnd, true);
   return 0;
}

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_vk_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_vk_create(hwnd);
   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_vk_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_vk_create(hwnd);
   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK wnd_proc_vk_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_vk_create(hwnd);
   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
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

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_gdi_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_gdi_create(hwnd);
   else if (message == WM_PAINT)
   {
      win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
      gdi_t *gdi                    = (gdi_t*)video_driver_get_ptr();

      if (gdi && gdi->memDC)
      {
         gdi->bmp_old    = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

         /* Draw video content */
         StretchBlt(
               gdi->winDC,
               0,
               0,
               gdi->screen_width,
               gdi->screen_height,
               gdi->memDC,
               0,
               0,
               gdi->video_width,
               gdi->video_height,
               SRCCOPY);

         SelectObject(gdi->memDC, gdi->bmp_old);
      }

#ifdef HAVE_TASKBAR
      if (     g_win32->taskbar_message
            && message == g_win32->taskbar_message)
         g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
   }

   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_gdi_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_gdi_create(hwnd);
   else if (message == WM_PAINT)
   {
      win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
      gdi_t *gdi                    = (gdi_t*)video_driver_get_ptr();

      if (gdi && gdi->memDC)
      {
         gdi->bmp_old    = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

         /* Draw video content */
         StretchBlt(
               gdi->winDC,
               0,
               0,
               gdi->screen_width,
               gdi->screen_height,
               gdi->memDC,
               0,
               0,
               gdi->video_width,
               gdi->video_height,
               SRCCOPY);

         SelectObject(gdi->memDC, gdi->bmp_old);
      }

#ifdef HAVE_TASKBAR
      if (     g_win32->taskbar_message
            && message == g_win32->taskbar_message)
         g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
   }

   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK wnd_proc_gdi_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_CREATE)
      return wnd_proc_wm_gdi_create(hwnd);
   else if (message == WM_PAINT)
   {
      win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
      gdi_t *gdi                    = (gdi_t*)video_driver_get_ptr();

      if (gdi && gdi->memDC)
      {
         gdi->bmp_old    = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

         /* Draw video content */
         StretchBlt(
               gdi->winDC,
               0,
               0,
               gdi->screen_width,
               gdi->screen_height,
               gdi->memDC,
               0,
               0,
               gdi->video_width,
               gdi->video_height,
               SRCCOPY);

         SelectObject(gdi->memDC, gdi->bmp_old);
      }

#ifdef HAVE_TASKBAR
      if (     g_win32->taskbar_message
            && message == g_win32->taskbar_message)
         g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
   }

   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
}
#endif

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

   if (window_save_positions && !fullscreen)
   {
      user_width                 = g_win32->pos_width;
      user_height                = g_win32->pos_height;
   }
#ifdef LEGACY_WIN32
   main_window.hwnd              = CreateWindowEx(0,
         "RetroArch", title_local,
#else
   main_window.hwnd              = CreateWindowExW(0,
         L"RetroArch", title_local,
#endif
         style,
         fullscreen ? mon_rect->left : g_win32->pos_x,
         fullscreen ? mon_rect->top  : g_win32->pos_y,
         user_width,
         user_height,
         NULL, NULL, NULL, data);
   free(title_local);
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
   notification_handler                = RegisterDeviceNotification(
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

#if !defined(_XBOX) && !defined(__WINRT__)
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
      unsigned *width, unsigned *height)
{
   bool video_is_threaded = video_driver_is_threaded();
   if (video_is_threaded)
      ui_companion_win32.application->process_events();
   *quit                  = (g_win32_flags & WIN32_CMN_FLAG_QUIT) ? true : false;

   if (g_win32_flags & WIN32_CMN_FLAG_RESIZED)
   {
      *resize             = true;
      *width              = g_win32_resize_width;
      *height             = g_win32_resize_height;
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
         unsigned window_position_x       = settings->uints.window_position_x;
         unsigned window_position_y       = settings->uints.window_position_y;
         unsigned window_position_width   = settings->uints.window_position_width;
         unsigned window_position_height  = settings->uints.window_position_height;

         g_win32->pos_x                   = window_position_x;
         g_win32->pos_y                   = window_position_y;
         g_win32->pos_width               = window_position_width
            + border_thickness * 2;
         g_win32->pos_height              = window_position_height
            + border_thickness * 2 + title_bar_height;

         if (g_win32->pos_width != 0 && g_win32->pos_height != 0)
            position_set_from_config = true;
      }

      if (position_set_from_config)
      {
         g_win32_resize_width  = *width   = g_win32->pos_width;
         g_win32_resize_height = *height  = g_win32->pos_height;
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

         menuItem = win32_resources_create_menu();
         win32_localize_menu(menuItem);
         SetMenu(main_window.hwnd, menuItem);

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

   win32_show_cursor(NULL, !fullscreen);
}

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
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
}

void win32_setup_pixel_format(HDC hdc, bool supports_gl)
{
   int pf;
   PIXELFORMATDESCRIPTOR pfd = {0};
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
   if (pf == 0 || !SetPixelFormat(hdc, pf, &pfd))
      RARCH_ERR("[Win32] Failed to set pixel format.\n");
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

void win32_apply_dpi_awareness(void)
{
   HMODULE shcore = LoadLibraryW(L"shcore.dll");
   if (shcore)
   {
      union {
         FARPROC proc;
         pfn_SetProcessDpiAwareness func;
      } u;
      u.proc = GetProcAddress(shcore, "SetProcessDpiAwareness");
      if (u.func)
      {
         u.func(1); /* PROCESS_SYSTEM_DPI_AWARE */
         FreeLibrary(shcore);
         return;
      }
      FreeLibrary(shcore);
   }
   /* Fallback for Vista / Win7 without shcore.
    * Load dynamically so we still link on XP / MSVC 2005. */
   {
      HMODULE user32 = GetModuleHandleW(L"user32.dll");
      if (user32)
      {
         typedef BOOL (WINAPI *pfn_SetProcessDPIAware)(void);
         union {
            FARPROC proc;
            pfn_SetProcessDPIAware func;
         } u;
         u.proc = GetProcAddress(user32, "SetProcessDPIAware");
         if (u.func)
            u.func();
      }
   }
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
