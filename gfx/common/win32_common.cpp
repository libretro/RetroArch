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

#include "../../general.h"
#include "win32_common.h"

#if !defined(_XBOX)

#define IDI_ICON 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <windows.h>
#include <commdlg.h>
#include "../../retroarch.h"
#include "../video_thread_wrapper.h"

#ifdef HAVE_OPENGL
#include "../drivers_wm/win32_shader_dlg.h"
#endif

#ifdef HAVE_D3D
#include "../d3d/d3d.h"
#endif

#ifdef __cplusplus
extern "C"
#endif
bool dinput_handle_message(void *dinput, UINT message, WPARAM wParam, LPARAM lParam);

unsigned g_resize_width;
unsigned g_resize_height;
bool g_restore_desktop;
static unsigned g_pos_x = CW_USEDEFAULT;
static unsigned g_pos_y = CW_USEDEFAULT;
static bool g_resized;
bool g_inited;
bool g_quit;
HWND g_hwnd;

extern void *dinput_wgl;
extern void *curD3D;
extern void *dinput;

/* Power Request APIs */

typedef REASON_CONTEXT POWER_REQUEST_CONTEXT, *PPOWER_REQUEST_CONTEXT, *LPPOWER_REQUEST_CONTEXT;

extern "C" WINBASEAPI
HANDLE
WINAPI
PowerCreateRequest (
    PREASON_CONTEXT Context
    );

WINBASEAPI
BOOL
WINAPI
PowerSetRequest (
    HANDLE PowerRequest,
    POWER_REQUEST_TYPE RequestType
    );

WINBASEAPI
BOOL
WINAPI
PowerClearRequest (
    HANDLE PowerRequest,
    POWER_REQUEST_TYPE RequestType
    );

#ifndef MAX_MONITORS
#define MAX_MONITORS 9
#endif

static HMONITOR win32_monitor_last;
static unsigned win32_monitor_count;
static HMONITOR win32_monitor_all[MAX_MONITORS];

static BOOL CALLBACK win32_monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   win32_monitor_all[win32_monitor_count++] = hMonitor;
   return TRUE;
}


void win32_monitor_from_window(HWND data, bool destroy)
{
   win32_monitor_last = MonitorFromWindow(data, MONITOR_DEFAULTTONEAREST);
   if (destroy)
      DestroyWindow(data);
}

void win32_monitor_get_info(void)
{
   MONITORINFOEX current_mon;

   memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);

   GetMonitorInfo(win32_monitor_last, (MONITORINFO*)&current_mon);
   ChangeDisplaySettingsEx(current_mon.szDevice, NULL, NULL, 0, NULL);
}

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id)
{
   unsigned i, fs_monitor;
   settings_t *settings = config_get_ptr();
   MONITORINFOEX *mon   = (MONITORINFOEX*)data;
   HMONITOR *hm_to_use  = (HMONITOR*)hm_data;

   if (!win32_monitor_last)
      win32_monitor_from_window(GetDesktopWindow(), false);

   *hm_to_use = win32_monitor_last;
   fs_monitor = settings->video.monitor_index;

   if (fs_monitor && fs_monitor <= win32_monitor_count
         && win32_monitor_all[fs_monitor - 1])
   {
      *hm_to_use = win32_monitor_all[fs_monitor - 1];
      *mon_id    = fs_monitor - 1;
   }
   else
   {
      for (i = 0; i < win32_monitor_count; i++)
      {
         if (win32_monitor_all[i] != *hm_to_use)
            continue;

         *mon_id = i;
         break;
      }
   }

   memset(mon, 0, sizeof(*mon));
   mon->cbSize = sizeof(MONITORINFOEX);
   GetMonitorInfo(*hm_to_use, (MONITORINFO*)mon);
}

static const char *win32_video_get_ident(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
      return rarch_threaded_video_get_ident();
#endif
   return video_driver_get_ident();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   settings_t *settings     = config_get_ptr();
   driver_t   *driver       = driver_get_ptr();
   const char *video_driver = win32_video_get_ident();

   switch (message)
   {
      case WM_SYSCOMMAND:
         /* Prevent screensavers, etc, while running. */
         switch (wparam)
         {
            case SC_SCREENSAVE:
            case SC_MONITORPOWER:
               return 0;
         }
         break;

      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSKEYDOWN:
         return win32_handle_keyboard_event(hwnd, message, wparam, lparam);

      case WM_CREATE:
         if (!strcmp(video_driver, "gl"))
            create_gl_context(hwnd);
         else if (!strcmp(video_driver, "d3d"))
         {
            LPCREATESTRUCT p_cs   = (LPCREATESTRUCT)lparam;
            curD3D                = p_cs->lpCreateParams;
         }
         return 0;

      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      {
         WINDOWPLACEMENT placement;
         GetWindowPlacement(g_hwnd, &placement);
         g_pos_x = placement.rcNormalPosition.left;
         g_pos_y = placement.rcNormalPosition.top;
         g_quit = true;
         return 0;
      }
      case WM_SIZE:
         /* Do not send resize message if we minimize. */
         if (wparam != SIZE_MAXHIDE && wparam != SIZE_MINIMIZED)
         {
            g_resize_width  = LOWORD(lparam);
            g_resize_height = HIWORD(lparam);
            g_resized = true;
         }
         return 0;
	  case WM_COMMAND:
         if (settings->ui.menubar_enable)
         {
            HWND d3dr = g_hwnd;
            if (!strcmp(video_driver, "d3d"))
            {
               d3d_video_t *d3d = (d3d_video_t*)driver->video_data;
               d3dr = g_hwnd;
            }
            LRESULT ret = win32_menu_loop(d3dr, wparam);
            (void)ret;
         }
         break;
   }

   if (dinput_handle_message((!strcmp(video_driver, "gl")) ? dinput_wgl : dinput, message, wparam, lparam))
      return 0;
   return DefWindowProc(hwnd, message, wparam, lparam);
}

bool win32_window_init(WNDCLASSEX *wndclass, bool fullscreen)
{
#ifndef _XBOX
   wndclass->cbSize        = sizeof(WNDCLASSEX);
   wndclass->style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndclass->lpfnWndProc   = WndProc;
   wndclass->hInstance     = GetModuleHandle(NULL);
   wndclass->hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass->lpszClassName = "RetroArch";
   wndclass->hIcon         = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
   wndclass->hIconSm       = (HICON)LoadImage(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!fullscreen)
      wndclass->hbrBackground = (HBRUSH)COLOR_WINDOW;

   if (!RegisterClassEx(wndclass))
      return false;
#endif
   return true;
}

bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen)
{
#ifndef _XBOX
   driver_t   *driver       = driver_get_ptr();
   g_hwnd = CreateWindowEx(0, "RetroArch", "RetroArch",
         style,
         fullscreen ? mon_rect->left : g_pos_x,
         fullscreen ? mon_rect->top  : g_pos_y,
         width, height,
         NULL, NULL, NULL, data);
   if (!g_hwnd)
      return false;

   driver->display_type  = RARCH_DISPLAY_WIN32;
   driver->video_display = 0;
   driver->video_window  = (uintptr_t)g_hwnd;
#endif
   return true;
}

static bool win32_browser(
      HWND owner,
      char *filename,
      const char *extensions,
      const char *title,
      const char *initial_dir)
{
	OPENFILENAME ofn;

	memset((void*)&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = owner;
	ofn.lpstrFilter     = extensions;
	ofn.lpstrFile       = filename;
	ofn.lpstrTitle      = title;
	ofn.lpstrInitialDir = TEXT(initial_dir);
	ofn.lpstrDefExt     = "";
	ofn.nMaxFile        = PATH_MAX;
	ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (!GetOpenFileName(&ofn))
		return false;

	return true;
}

LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode         = wparam & 0xffff;
   enum event_command cmd         = EVENT_CMD_NONE;
   bool do_wm_close     = false;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   (void)global;

	switch (mode)
   {
      case ID_M_LOAD_CORE:
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            const char *extensions  = NULL;
            const char *title       = NULL;
            const char *initial_dir = NULL;

            if      (mode == ID_M_LOAD_CORE)
            {
               extensions  = "All Files\0*.*\0 Libretro core(.dll)\0*.dll\0";
               title       = "Load Core";
               initial_dir = settings->libretro_directory;
            }
            else if (mode == ID_M_LOAD_CONTENT)
            {
               extensions  = "All Files\0*.*\0\0";
               title       = "Load Content";
               initial_dir = settings->menu_content_directory;
            }

            if (win32_browser(owner, win32_file, extensions, title, initial_dir))
            {
               switch (mode)
               {
                  case ID_M_LOAD_CORE:
                     strlcpy(settings->libretro, win32_file, sizeof(settings->libretro));
                     cmd = EVENT_CMD_LOAD_CORE;
                     break;
                  case ID_M_LOAD_CONTENT:
                     strlcpy(global->path.fullpath, win32_file, sizeof(global->path.fullpath));
                     cmd = EVENT_CMD_LOAD_CONTENT;
                     do_wm_close = true;
                     break;
               }
            }
         }
         break;
      case ID_M_RESET:
         cmd = EVENT_CMD_RESET;
         break;
      case ID_M_MUTE_TOGGLE:
         cmd = EVENT_CMD_AUDIO_MUTE_TOGGLE;
         break;
      case ID_M_MENU_TOGGLE:
         cmd = EVENT_CMD_MENU_TOGGLE;
         break;
      case ID_M_PAUSE_TOGGLE:
         cmd = EVENT_CMD_PAUSE_TOGGLE;
         break;
      case ID_M_LOAD_STATE:
         cmd = EVENT_CMD_LOAD_STATE;
         break;
      case ID_M_SAVE_STATE:
         cmd = EVENT_CMD_SAVE_STATE;
         break;
      case ID_M_DISK_CYCLE:
         cmd = EVENT_CMD_DISK_EJECT_TOGGLE;
         break;
      case ID_M_DISK_NEXT:
         cmd = EVENT_CMD_DISK_NEXT;
         break;
      case ID_M_DISK_PREV:
         cmd = EVENT_CMD_DISK_PREV;
         break;
      case ID_M_FULL_SCREEN:
         cmd = EVENT_CMD_FULLSCREEN_TOGGLE;
         break;
#ifdef HAVE_OPENGL
      case ID_M_SHADER_PARAMETERS:
         shader_dlg_show(owner);
         break;
#endif
      case ID_M_MOUSE_GRAB:
         cmd = EVENT_CMD_GRAB_MOUSE_TOGGLE;
         break;
      case ID_M_TAKE_SCREENSHOT:
         cmd = EVENT_CMD_TAKE_SCREENSHOT;
         break;
      case ID_M_QUIT:
         do_wm_close = true;
         break;
      default:
         if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
         {
            unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
            global->pending.windowed_scale = idx;
            cmd = EVENT_CMD_RESIZE_WINDOWED_SCALE;
         }
         else if (mode == ID_M_STATE_INDEX_AUTO)
         {
            signed idx = -1;
            settings->state_slot = idx;
         }
         else if (mode >= (ID_M_STATE_INDEX_AUTO+1) && mode <= (ID_M_STATE_INDEX_AUTO+10))
         {
            signed idx = (mode - (ID_M_STATE_INDEX_AUTO+1));
            settings->state_slot = idx;
         }
         break;
   }

	if (cmd != EVENT_CMD_NONE)
		event_command(cmd);

	if (do_wm_close)
		PostMessage(owner, WM_CLOSE, 0, 0);
	
	return 0L;
}
#endif

bool win32_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
#ifdef _XBOX
   return false;
#else
   HDC monitor            = GetDC(NULL);
   int pixels_x           = GetDeviceCaps(monitor, HORZRES);
   int pixels_y           = GetDeviceCaps(monitor, VERTRES);
   int physical_width     = GetDeviceCaps(monitor, HORZSIZE);
   int physical_height    = GetDeviceCaps(monitor, VERTSIZE);

   ReleaseDC(NULL, monitor);

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         /* 25.4 mm in an inch. */
         *value = 254 * pixels_x / physical_width / 10;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
#endif
}

void win32_monitor_init(void)
{
#ifndef _XBOX
   win32_monitor_count = 0;
   EnumDisplayMonitors(NULL, NULL, win32_monitor_enum_proc, 0);
#endif

   g_quit              = false;
}

bool win32_monitor_set_fullscreen(unsigned width, unsigned height, unsigned refresh, char *dev_name)
{
#ifndef _XBOX
   DEVMODE devmode;

   memset(&devmode, 0, sizeof(devmode));
   devmode.dmSize       = sizeof(DEVMODE);
   devmode.dmPelsWidth  = width;
   devmode.dmPelsHeight = height;
   devmode.dmDisplayFrequency = refresh;
   devmode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

   RARCH_LOG("[WGL]: Setting fullscreen to %ux%u @ %uHz on device %s.\n", width, height, refresh, dev_name);
   return ChangeDisplaySettingsEx(dev_name, &devmode, NULL, CDS_FULLSCREEN, NULL) == DISP_CHANGE_SUCCESSFUL;
#endif
}

void win32_show_cursor(bool state)
{
#ifndef _XBOX
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
#endif
}

void win32_check_window(bool *quit, bool *resize, unsigned *width, unsigned *height)
{
#ifndef _XBOX
   MSG msg;

   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
#endif
   *quit = g_quit;

   if (g_resized)
   {
      *resize       = true;
      *width        = g_resize_width;
      *height       = g_resize_height;
      g_resized     = false;
   }
}

bool win32_suppress_screensaver(void *data, bool enable)
{
#ifdef _XBOX
   return false;
#else
   typedef HANDLE (WINAPI * PowerCreateRequestPtr)(REASON_CONTEXT *context);
   typedef BOOL   (WINAPI * PowerSetRequestPtr)(HANDLE PowerRequest, POWER_REQUEST_TYPE RequestType);
   HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
   PowerCreateRequestPtr powerCreateRequest =
     (PowerCreateRequestPtr)GetProcAddress(kernel32, "PowerCreateRequest");
   PowerSetRequestPtr    powerSetRequest =
     (PowerSetRequestPtr)GetProcAddress(kernel32, "PowerSetRequest");

   if(enable)
   {
      if(powerCreateRequest && powerSetRequest)
      {
         /* Windows 7, 8, 10 codepath */
         POWER_REQUEST_CONTEXT RequestContext;
        HANDLE Request;

         RequestContext.Version = POWER_REQUEST_CONTEXT_VERSION;
         RequestContext.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
         RequestContext.Reason.SimpleReasonString = L"RetroArch running";

         Request = PowerCreateRequest(&RequestContext);

         powerSetRequest( Request, PowerRequestDisplayRequired);
         return true;
      }
     else
      {
         /* XP / Vista codepath */
         SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
         return true;
      }
   }

   return false;
#endif
}

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   DWORD style;
   MSG msg;
   RECT mon_rect;
   unsigned mon_id;
   MONITORINFOEX current_mon;
   float refresh_mod;
   unsigned refresh;
   bool windowed_full;
   RECT rect   = {0};
   HMONITOR hm_to_use = NULL;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);

   mon_rect        = current_mon.rcMonitor;
   g_resize_width  = width;
   g_resize_height = height;

   /* Windows only reports the refresh rates for modelines as 
    * an integer, so video.refresh_rate needs to be rounded. Also, account 
    * for black frame insertion using video.refresh_rate set to half
    * of the display refresh rate, as well as higher vsync swap intervals. */
   refresh_mod = settings->video.black_frame_insertion ? 2.0f : 1.0f;
   refresh     = roundf(settings->video.refresh_rate * refresh_mod * settings->video.swap_interval);

   windowed_full   = settings->video.windowed_fullscreen;

   if (fullscreen)
   {
      if (windowed_full)
      {
         style = WS_EX_TOPMOST | WS_POPUP;
         g_resize_width  = width  = mon_rect.right - mon_rect.left;
         g_resize_height = height = mon_rect.bottom - mon_rect.top;
      }
      else
      {
         style = WS_POPUP | WS_VISIBLE;

         if (!win32_monitor_set_fullscreen(width, height, refresh, current_mon.szDevice))
            return false;

         /* Display settings might have changed, get new coordinates. */
         GetMonitorInfo(hm_to_use, (MONITORINFO*)&current_mon);
         mon_rect = current_mon.rcMonitor;
         g_restore_desktop = true;
      }
   }
   else
   {
      style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      rect.right  = width;
      rect.bottom = height;
      AdjustWindowRect(&rect, style, FALSE);
      g_resize_width  = width  = rect.right - rect.left;
      g_resize_height = height = rect.bottom - rect.top;
   }

   if (!win32_window_create(NULL, style, &mon_rect, width, height, fullscreen))
      return false;

   if (!fullscreen || windowed_full)
   {
      if (!fullscreen && settings->ui.menubar_enable)
      {
         RECT rc_temp = {0, 0, (LONG)height, 0x7FFF};
         SetMenu(g_hwnd, LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
         SendMessage(g_hwnd, WM_NCCALCSIZE, FALSE, (LPARAM)&rc_temp);
         g_resize_height = height += rc_temp.top + rect.top;
         SetWindowPos(g_hwnd, NULL, 0, 0, width, height, SWP_NOMOVE);
      }

      ShowWindow(g_hwnd, SW_RESTORE);
      UpdateWindow(g_hwnd);
      SetForegroundWindow(g_hwnd);
      SetFocus(g_hwnd);
   }

   win32_show_cursor(!fullscreen);

   /* Wait until context is created (or failed to do so ...) */
   while (!g_inited && !g_quit && GetMessage(&msg, g_hwnd, 0, 0))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (g_quit)
      return false;
#endif

   return true;
}

#ifdef _XBOX
static HANDLE GetFocus(void)
{
   return g_hwnd;
}
#endif

bool win32_has_focus(void)
{
   if (!g_inited)
      return false;

   return GetFocus() == g_hwnd;
}
