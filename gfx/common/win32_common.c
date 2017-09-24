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

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "win32_common.h"
#include "gdi_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../driver.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../core_info.h"

#if !defined(_XBOX)

#define IDI_ICON 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /* _WIN32_WINNT_WIN2K */
#endif

#include <windows.h>
#include <commdlg.h>
#include "../../retroarch.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../video_thread_wrapper.h"
#include <shellapi.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <encodings/utf.h>

extern LRESULT win32_menu_loop(HWND owner, WPARAM wparam);

#if defined(HAVE_D3D9) || defined(HAVE_D3D8)
extern bool dinput_handle_message(void *dinput, UINT message,
      WPARAM wParam, LPARAM lParam);
extern void *dinput_gdi;
extern void *dinput_wgl;
extern void *dinput;
#endif

unsigned g_resize_width             = 0;
unsigned g_resize_height            = 0;
static bool g_resized               = false;
bool g_restore_desktop              = false;
static bool doubleclick_on_titlebar = false;
bool g_inited                       = false;
static bool g_quit                  = false;
static unsigned g_pos_x             = CW_USEDEFAULT;
static unsigned g_pos_y             = CW_USEDEFAULT;
static void *curD3D                 = NULL;

ui_window_win32_t main_window;

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

#if defined(_MSC_VER) && _MSC_VER <= 1200
#define INT_PTR_COMPAT int
#else
#define INT_PTR_COMPAT INT_PTR
#endif

static HMONITOR win32_monitor_last;
static HMONITOR win32_monitor_all[MAX_MONITORS];
static unsigned win32_monitor_count              = 0;

bool doubleclick_on_titlebar_pressed(void)
{
   return doubleclick_on_titlebar;
}

void unset_doubleclick_on_titlebar(void)
{
   doubleclick_on_titlebar = false;
}

INT_PTR_COMPAT CALLBACK PickCoreProc(HWND hDlg, UINT message,
        WPARAM wParam, LPARAM lParam)
{
   size_t list_size;
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info     = NULL;

   switch (message)
   {
      case WM_INITDIALOG:
         {
            HWND hwndList;
            unsigned i;
            /* Add items to list.  */

            core_info_get_list(&core_info_list);
            core_info_list_get_supported_cores(core_info_list,
                  path_get(RARCH_PATH_CONTENT), &core_info, &list_size);

            hwndList = GetDlgItem(hDlg, ID_CORELISTBOX);

            for (i = 0; i < list_size; i++)
            {
               const core_info_t *info = (const core_info_t*)&core_info[i];
               SendMessage(hwndList, LB_ADDSTRING, 0,
                     (LPARAM)info->display_name);
            }
            SetFocus(hwndList);
            return TRUE;
         }

      case WM_COMMAND:
         switch (LOWORD(wParam))
         {
            case IDOK:
            case IDCANCEL:
               EndDialog(hDlg, LOWORD(wParam));
               break;
            case ID_CORELISTBOX:
               switch (HIWORD(wParam))
               {
                  case LBN_SELCHANGE:
                     {
                        const core_info_t *info = NULL;
                        HWND hwndList           = GetDlgItem(
                              hDlg, ID_CORELISTBOX);
                        int lbItem              = (int)
                           SendMessage(hwndList, LB_GETCURSEL, 0, 0);

                        core_info_get_list(&core_info_list);
                        core_info_list_get_supported_cores(core_info_list,
                              path_get(RARCH_PATH_CONTENT), &core_info, &list_size);
                        info = (const core_info_t*)&core_info[lbItem];
                        rarch_ctl(RARCH_CTL_SET_LIBRETRO_PATH,info->path);
                     }
                     break;
               }
               return TRUE;
         }
   }
   return FALSE;
}


static BOOL CALLBACK win32_monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   win32_monitor_all[win32_monitor_count++] = hMonitor;
   return TRUE;
}


void win32_monitor_from_window(void)
{
#ifndef _XBOX
   ui_window_t *window       = NULL;

   win32_monitor_last        =
      MonitorFromWindow(main_window.hwnd, MONITOR_DEFAULTTONEAREST);

   window = (ui_window_t*)ui_companion_driver_get_window_ptr();

   if (window)
      window->destroy(&main_window);
#endif
}

void win32_monitor_get_info(void)
{
   MONITORINFOEX current_mon;

   memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);

   GetMonitorInfo(win32_monitor_last, (LPMONITORINFO)&current_mon);

#if _WIN32_WINDOWS >= 0x0410 || _WIN32_WINNT >= 0x0410
   /* Windows 98 and later codepath */
   ChangeDisplaySettingsEx(current_mon.szDevice, NULL, NULL, 0, NULL);
#else
   /* Windows 95 / NT codepath */
   ChangeDisplaySettings(NULL, 0);
#endif
}

void win32_monitor_info(void *data, void *hm_data, unsigned *mon_id)
{
   unsigned i;
   settings_t *settings  = config_get_ptr();
   MONITORINFOEX *mon    = (MONITORINFOEX*)data;
   HMONITOR *hm_to_use   = (HMONITOR*)hm_data;
   unsigned fs_monitor   = settings->uints.video_monitor_index;

   if (!win32_monitor_last)
      win32_monitor_last = MonitorFromWindow(GetDesktopWindow(),
            MONITOR_DEFAULTTONEAREST);

   *hm_to_use            = win32_monitor_last;

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
   GetMonitorInfo(*hm_to_use, (LPMONITORINFO)mon);
}

/* Get the count of the files dropped */
static int win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   char szFilename[1024];

   szFilename[0] = '\0';

   if (DragQueryFile((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      /*poll list of current cores */
      size_t list_size;
      content_ctx_info_t content_info  = {0};
      core_info_list_t *core_info_list = NULL;
      const core_info_t *core_info     = NULL;

      DragQueryFile((HDROP)wparam, 0, szFilename, sizeof(szFilename));

      core_info_get_list(&core_info_list);

      if (!core_info_list)
         return 0;

      core_info_list_get_supported_cores(core_info_list,
            (const char*)szFilename, &core_info, &list_size);

      if (!list_size)
         return 0;

      path_set(RARCH_PATH_CONTENT, szFilename);

      if (!path_is_empty(RARCH_PATH_CONTENT))
      {
         unsigned i;
         core_info_t *current_core = NULL;
         core_info_get_current_core(&current_core);

         /*we already have path for libretro core */
         for (i = 0; i < list_size; i++)
         {
            const core_info_t *info = (const core_info_t*)&core_info[i];

            if(!string_is_equal(info->systemname, current_core->systemname))
               break;

            if(string_is_equal(path_get(RARCH_PATH_CORE), info->path))
            {
               /* Our previous core supports the current rom */
               content_ctx_info_t content_info = {0};
               task_push_load_content_with_current_core_from_companion_ui(
                     NULL,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL);
               return 0;
            }
         }
      }

      /* Poll for cores for current rom since none exist. */
      if(list_size ==1)
      {
         /*pick core that only exists and is bound to work. Ish. */
         const core_info_t *info = (const core_info_t*)&core_info[0];

         if (info)
            task_push_load_content_with_new_core_from_companion_ui(
                  info->path, NULL,
                  &content_info,
                  NULL, NULL);
      }
      else
      {
         /* Pick one core that could be compatible, ew */
         if(DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_PICKCORE),
                  hwnd,PickCoreProc,(LPARAM)NULL)==IDOK)
         {
            task_push_load_content_with_current_core_from_companion_ui(
                  NULL,
                  &content_info,
                  CORE_TYPE_PLAIN,
                  NULL, NULL);
         }
      }
   }

   return 0;
}

#ifndef _XBOX
static LRESULT win32_handle_keyboard_event(HWND hwnd, UINT message,
		WPARAM wparam, LPARAM lparam)
{
   unsigned keycode;
   uint16_t mod     = 0;
   bool keydown     = true;

   if (GetKeyState(VK_SHIFT)   & 0x80)
      mod |= RETROKMOD_SHIFT;
   if (GetKeyState(VK_CONTROL) & 0x80)
      mod |=  RETROKMOD_CTRL;
   if (GetKeyState(VK_MENU)    & 0x80)
      mod |=  RETROKMOD_ALT;
   if (GetKeyState(VK_CAPITAL) & 0x81)
      mod |= RETROKMOD_CAPSLOCK;
   if (GetKeyState(VK_SCROLL)  & 0x81)
      mod |= RETROKMOD_SCROLLOCK;
   if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
      mod |= RETROKMOD_META;

   switch (message)
   {
      /* Seems to be hard to synchronize
       * WM_CHAR and WM_KEYDOWN properly.
       */
      case WM_CHAR:
         input_keyboard_event(keydown, RETROK_UNKNOWN, wparam, mod,
               RETRO_DEVICE_KEYBOARD);
         return TRUE;

      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         /* Key released? */
         if (message == WM_KEYUP || message == WM_SYSKEYUP)
            keydown = false;

#if _WIN32_WINNT >= 0x0501
         if (string_is_equal_fast(config_get_ptr()->arrays.input_driver, "raw", 4))
            keycode = input_keymaps_translate_keysym_to_rk((unsigned)(wparam));
         else
#endif
            keycode = input_keymaps_translate_keysym_to_rk((lparam >> 16) & 0xff);

         input_keyboard_event(keydown, keycode, 0, mod, RETRO_DEVICE_KEYBOARD);

         if (message == WM_SYSKEYDOWN)
         {
            switch (wparam)
            {
               case VK_F10:
               case VK_MENU:
               case VK_RSHIFT:
                  return 0;
               default:
                  break;
            }
         }
         else
            return 0;

         break;
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

static LRESULT CALLBACK WndProcCommon(bool *quit, HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   if (message == WM_NCLBUTTONDBLCLK)
      doubleclick_on_titlebar = true;

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
         {
            int ret = win32_drag_query_file(hwnd, wparam);
            DragFinish((HDROP)wparam);
            if (ret != 0)
               return 0;
         }
         break;
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSKEYDOWN:
         *quit = true;
         return win32_handle_keyboard_event(hwnd, message, wparam, lparam);

      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
         {
            WINDOWPLACEMENT placement;
            GetWindowPlacement(main_window.hwnd, &placement);
            g_pos_x = placement.rcNormalPosition.left;
            g_pos_y = placement.rcNormalPosition.top;
            g_quit  = true;
            *quit   = true;
         }
         break;
      case WM_SIZE:
         /* Do not send resize message if we minimize. */
         if (wparam != SIZE_MAXHIDE && wparam != SIZE_MINIMIZED)
         {
            g_resize_width  = LOWORD(lparam);
            g_resize_height = HIWORD(lparam);
            g_resized       = true;
         }
         *quit = true;
         break;
     case WM_COMMAND:
         {
            settings_t *settings     = config_get_ptr();
            if (settings && settings->bools.ui_menubar_enable)
               win32_menu_loop(main_window.hwnd, wparam);
         }
         break;
   }
   return 0;
}

extern VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

static void win32_set_droppable(ui_window_win32_t *window, bool droppable)
{
   if (DragAcceptFiles_func != NULL)
      DragAcceptFiles_func(window->hwnd, droppable);
}

#if defined(HAVE_D3D9) || defined(HAVE_D3D8)
LRESULT CALLBACK WndProcD3D(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool quit = false;

   if (message == WM_NCLBUTTONDBLCLK)
      doubleclick_on_titlebar = true;

   switch (message)
   {
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSKEYDOWN:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_COMMAND:
         ret = WndProcCommon(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         {
            ui_window_win32_t win32_window;
            LPCREATESTRUCT p_cs   = (LPCREATESTRUCT)lparam;
            curD3D                = p_cs->lpCreateParams;
            g_inited              = true;

            win32_window.hwnd     = hwnd;

            win32_set_droppable(&win32_window, true);
         }
         return 0;
   }

   if (dinput && dinput_handle_message(dinput,
            message, wparam, lparam))
      return 0;
   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_VULKAN)
LRESULT CALLBACK WndProcGL(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool quit = false;

   if (message == WM_NCLBUTTONDBLCLK)
      doubleclick_on_titlebar = true;

   switch (message)
   {
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSKEYDOWN:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_SIZE:
      case WM_COMMAND:
         ret = WndProcCommon(&quit,
               hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         {
            ui_window_win32_t win32_window;
            win32_window.hwnd           = hwnd;

            create_graphics_context(hwnd, &g_quit);

            win32_set_droppable(&win32_window, true);
         }
         return 0;
   }

#if defined(HAVE_D3D9) || defined(HAVE_D3D8)
   if (dinput_wgl && dinput_handle_message(dinput_wgl,
            message, wparam, lparam))
      return 0;
#endif
   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK WndProcGDI(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   bool quit = false;

   if (message == WM_NCLBUTTONDBLCLK)
      doubleclick_on_titlebar = true;

   switch (message)
   {
      case WM_PAINT:
      {
         gdi_t *gdi = (gdi_t*)video_driver_get_ptr(false);

         if (gdi && gdi->memDC)
         {
            gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

#ifdef HAVE_MENU
            if (menu_driver_is_alive() && !gdi_has_menu_frame())
            {
               /* draw menu contents behind a gradient background */
               if (gdi && gdi->memDC)
               {
                  RECT rect;
                  HBRUSH brush = CreateSolidBrush(RGB(1,81,127));

                  GetClientRect(hwnd, &rect);

                  StretchBlt(gdi->winDC,
                        0, 0,
                        gdi->screen_width, gdi->screen_height,
                        gdi->memDC, 0, 0, gdi->video_width, gdi->video_height, SRCCOPY);

                  FillRect(gdi->memDC, &rect, brush);
                  DeleteObject(brush);
               }
           }
           else
#endif
           {
              /* draw video content */
              gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

              StretchBlt(gdi->winDC,
                    0, 0,
                    gdi->screen_width, gdi->screen_height,
                    gdi->memDC, 0, 0, gdi->video_width, gdi->video_height, SRCCOPY);
           }

           SelectObject(gdi->memDC, gdi->bmp_old);
        }

        break;
      }
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSKEYDOWN:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_SIZE:
      case WM_COMMAND:
         ret = WndProcCommon(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         {
            ui_window_win32_t win32_window;
            win32_window.hwnd = hwnd;

            create_gdi_context(hwnd, &g_quit);

            win32_set_droppable(&win32_window, true);
         }
         return 0;
   }

#if defined(HAVE_D3D9) || defined(HAVE_D3D8)
   if (dinput_gdi && dinput_handle_message(dinput_gdi,
            message, wparam, lparam))
      return 0;
#endif
   return DefWindowProc(hwnd, message, wparam, lparam);
}

bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen)
{
#ifndef _XBOX
   main_window.hwnd = CreateWindowEx(0,
         "RetroArch", "RetroArch",
         style,
         fullscreen ? mon_rect->left : g_pos_x,
         fullscreen ? mon_rect->top  : g_pos_y,
         width, height,
         NULL, NULL, NULL, data);
   if (!main_window.hwnd)
      return false;

   video_driver_display_type_set(RARCH_DISPLAY_WIN32);
   video_driver_display_set(0);
   video_driver_window_set((uintptr_t)main_window.hwnd);
#endif
   return true;
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
   EnumDisplayMonitors(NULL, NULL,
         win32_monitor_enum_proc, 0);
#endif

   g_quit              = false;
}

static bool win32_monitor_set_fullscreen(
      unsigned width, unsigned height,
      unsigned refresh, char *dev_name)
{
#ifndef _XBOX
   DEVMODE devmode;

   memset(&devmode, 0, sizeof(devmode));
   devmode.dmSize             = sizeof(DEVMODE);
   devmode.dmPelsWidth        = width;
   devmode.dmPelsHeight       = height;
   devmode.dmDisplayFrequency = refresh;
   devmode.dmFields           = DM_PELSWIDTH
      | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

   RARCH_LOG("Setting fullscreen to %ux%u @ %uHz on device %s.\n",
         width, height, refresh, dev_name);

#if _WIN32_WINDOWS >= 0x0410 || _WIN32_WINNT >= 0x0410
   /* Windows 98 and later codepath */
   return ChangeDisplaySettingsEx(dev_name, &devmode,
         NULL, CDS_FULLSCREEN, NULL) == DISP_CHANGE_SUCCESSFUL;
#else
   /* Windows 95 / NT codepath */
   return ChangeDisplaySettings(&devmode, CDS_FULLSCREEN);
#endif
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

void win32_check_window(bool *quit, bool *resize,
      unsigned *width, unsigned *height)
{
#ifndef _XBOX
   const ui_application_t *application =
      ui_companion_driver_get_application_ptr();
   if (application)
      application->process_events();
#endif
   *quit            = g_quit;

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
#ifndef _XBOX
   if(enable)
   {
      int major, minor;
      char tmp[PATH_MAX_LENGTH];
      const frontend_ctx_driver_t *frontend = frontend_get_ptr();

      if (!frontend)
         return false;

      if (frontend->get_os)
         frontend->get_os(tmp, sizeof(tmp), &major, &minor);

      if (major*100+minor >= 601)
      {
#if _WIN32_WINNT >= 0x0601
         /* Windows 7, 8, 10 codepath */
         typedef HANDLE (WINAPI * PowerCreateRequestPtr)(REASON_CONTEXT *context);
         typedef BOOL   (WINAPI * PowerSetRequestPtr)(HANDLE PowerRequest,
               POWER_REQUEST_TYPE RequestType);
         HMODULE kernel32 = GetModuleHandle("kernel32.dll");
         PowerCreateRequestPtr powerCreateRequest =
            (PowerCreateRequestPtr)GetProcAddress(kernel32, "PowerCreateRequest");
         PowerSetRequestPtr    powerSetRequest =
            (PowerSetRequestPtr)GetProcAddress(kernel32, "PowerSetRequest");

         if(powerCreateRequest && powerSetRequest)
         {
            POWER_REQUEST_CONTEXT RequestContext;
            HANDLE Request;

            RequestContext.Version = POWER_REQUEST_CONTEXT_VERSION;
            RequestContext.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
            RequestContext.Reason.SimpleReasonString = (LPWSTR)L"RetroArch running";

            Request = powerCreateRequest(&RequestContext);

            powerSetRequest( Request, PowerRequestDisplayRequired);
            return true;
         }
#endif
      }
      else if (major*100+minor >= 410)
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
#endif

   return false;
}

void win32_set_style(MONITORINFOEX *current_mon, HMONITOR *hm_to_use,
   unsigned *width, unsigned *height, bool fullscreen, bool windowed_full,
   RECT *rect, RECT *mon_rect, DWORD *style)
{
#ifndef _XBOX
   settings_t *settings = config_get_ptr();

   /* Windows only reports the refresh rates for modelines as
    * an integer, so video_refresh_rate needs to be rounded. Also, account
    * for black frame insertion using video_refresh_rate set to half
    * of the display refresh rate, as well as higher vsync swap intervals. */
   float refresh_mod    = settings->bools.video_black_frame_insertion ? 2.0f : 1.0f;
   unsigned refresh     = roundf(settings->floats.video_refresh_rate
         * refresh_mod * settings->uints.video_swap_interval);

   if (fullscreen)
   {
      if (windowed_full)
      {
         *style          = WS_EX_TOPMOST | WS_POPUP;
         g_resize_width  = *width  = mon_rect->right  - mon_rect->left;
         g_resize_height = *height = mon_rect->bottom - mon_rect->top;
      }
      else
      {
         *style          = WS_POPUP | WS_VISIBLE;

         if (!win32_monitor_set_fullscreen(*width, *height,
                  refresh, current_mon->szDevice))
          {}

         /* Display settings might have changed, get new coordinates. */
         GetMonitorInfo(*hm_to_use, (LPMONITORINFO)current_mon);
         *mon_rect = current_mon->rcMonitor;
      }
   }
   else
   {
      *style       = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      rect->right  = *width;
      rect->bottom = *height;
      AdjustWindowRect(rect, *style, FALSE);
      g_resize_width  = *width   = rect->right  - rect->left;
      g_resize_height = *height  = rect->bottom - rect->top;
   }
#endif
}

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data)
{
#ifndef _XBOX
   RECT *rect            = (RECT*)rect_data;

   if (!fullscreen || windowed_full)
   {
      settings_t *settings      = config_get_ptr();
      const ui_window_t *window = ui_companion_driver_get_window_ptr();

      if (!fullscreen && settings->bools.ui_menubar_enable)
      {
         RECT rc_temp;
         rc_temp.left   = 0;
         rc_temp.top    = 0;
         rc_temp.right  = (LONG)*height;
         rc_temp.bottom = 0x7FFF;

         SetMenu(main_window.hwnd,
               LoadMenu(GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MENU)));
         SendMessage(main_window.hwnd, WM_NCCALCSIZE, FALSE, (LPARAM)&rc_temp);
         g_resize_height = *height += rc_temp.top + rect->top;
         SetWindowPos(main_window.hwnd, NULL, 0, 0, *width, *height, SWP_NOMOVE);
      }

      ShowWindow(main_window.hwnd, SW_RESTORE);
      UpdateWindow(main_window.hwnd);
      SetForegroundWindow(main_window.hwnd);

      if (window)
         window->set_focused(&main_window);
   }

   win32_show_cursor(!fullscreen);
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
   RECT rect;
   MONITORINFOEX current_mon;
   unsigned mon_id       = 0;
   bool windowed_full    = false;
   HMONITOR hm_to_use    = NULL;
   settings_t *settings  = config_get_ptr();
   int res               = 0;

   rect.left             = 0;
   rect.top              = 0;
   rect.right            = 0;
   rect.bottom           = 0;

   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_resize_width        = width;
   g_resize_height       = height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   win32_set_style(&current_mon, &hm_to_use, &width, &height,
         fullscreen, windowed_full, &rect, &mon_rect, &style);

   if (!win32_window_create(data, style,
            &mon_rect, width, height, fullscreen))
      return false;

   win32_set_window(&width, &height,
         fullscreen, windowed_full, &rect);

   /* Wait until context is created (or failed to do so ...).
    * Please don't remove the (res = ) as GetMessage can return -1. */
   while (!g_inited && !g_quit
         && (res = GetMessage(&msg, main_window.hwnd, 0, 0)) != 0)
   {
      if (res == -1)
      {
         RARCH_ERR("GetMessage error code %d\n", GetLastError());
         break;
      }
      else
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }

   if (g_quit)
      return false;
#endif

   return true;
}

#ifdef _XBOX
static HANDLE GetFocus(void)
{
   return main_window.hwnd;
}

static HWND GetForegroundWindow(void)
{
   return main_window.hwnd;
}

BOOL IsIconic(HWND hwnd)
{
   return FALSE;
}
#endif

bool win32_has_focus(void)
{
   if (g_inited)
   {
#ifdef _XBOX
      if (GetForegroundWindow() == main_window.hwnd)
         return true;
#else
      const ui_window_t *window =
         ui_companion_driver_get_window_ptr();
      if (window)
         return window->focused(&main_window);
#endif
   }

   return false;
}

HWND win32_get_window(void)
{
   return main_window.hwnd;
}

void win32_window_reset(void)
{
   g_quit              = false;
   g_restore_desktop   = false;
}

void win32_destroy_window(void)
{
#ifndef _XBOX
   UnregisterClass("RetroArch", GetModuleHandle(NULL));
#endif
   main_window.hwnd = NULL;
}

void win32_get_video_output_prev(
      unsigned *width, unsigned *height)
{
   DEVMODE dm;
   int iModeNum;
   bool found           = false;
   unsigned prev_width  = 0;
   unsigned prev_height = 0;
   unsigned curr_width  = 0;
   unsigned curr_height = 0;

   memset(&dm, 0, sizeof(dm));

   dm.dmSize            = sizeof(dm);

   win32_get_video_output_size(&curr_width, &curr_height);

   for (iModeNum = 0;
         EnumDisplaySettings(NULL, iModeNum, &dm) != 0;
         iModeNum++)
   {
      if (     dm.dmPelsWidth == curr_width
            && dm.dmPelsHeight == curr_height)
      {
         if (     prev_width  != curr_width
               && prev_height != curr_height)
         {
            found        = true;
            break;
         }
      }

      prev_width     = dm.dmPelsWidth;
      prev_height    = dm.dmPelsHeight;
   }

   if (found)
   {
      *width       = prev_width;
      *height      = prev_height;
   }
}

void win32_get_video_output_next(
      unsigned *width, unsigned *height)
{
   DEVMODE dm;
   int iModeNum;
   bool found           = false;
   unsigned curr_width  = 0;
   unsigned curr_height = 0;

   memset(&dm, 0, sizeof(dm));
   dm.dmSize = sizeof(dm);

   win32_get_video_output_size(&curr_width, &curr_height);

   for (iModeNum = 0;
         EnumDisplaySettings(NULL, iModeNum, &dm) != 0;
         iModeNum++)
   {
      if (found)
      {
         *width     = dm.dmPelsWidth;
         *height    = dm.dmPelsHeight;
         break;
      }

      if (     dm.dmPelsWidth == curr_width
            && dm.dmPelsHeight == curr_height)
         found = true;
   }
}

void win32_get_video_output_size(unsigned *width, unsigned *height)
{
   DEVMODE dm;
   memset(&dm, 0, sizeof(dm));
   dm.dmSize = sizeof(dm);

   if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm) != 0)
   {
      *width  = dm.dmPelsWidth;
      *height = dm.dmPelsHeight;
   }
}
