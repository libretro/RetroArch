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

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "win32_common.h"

#ifdef HAVE_GDI
#include "gdi_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../driver.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include "../../core_info.h"

#if !defined(_XBOX)

#include <commdlg.h>
#include <dbt.h>
#include "../../input/input_keymaps.h"
#include "../video_thread_wrapper.h"
#include "../video_display_server.h"
#include "../../retroarch.h"
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

#ifdef LEGACY_WIN32
#define DragQueryFileR DragQueryFile
#else
#define DragQueryFileR DragQueryFileW
#endif

/* For some reason this is missing from mingw winuser.h */
#ifndef EDS_ROTATEDMODE
#define EDS_ROTATEDMODE 4
#endif

const GUID GUID_DEVINTERFACE_HID = { 0x4d1e55b2, 0xf16f, 0x11Cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x501
static HDEVNOTIFY notification_handler;
#endif

#ifdef HAVE_DINPUT
extern bool dinput_handle_message(void *dinput, UINT message,
      WPARAM wParam, LPARAM lParam);
#ifdef HAVE_GDI
extern void *dinput_gdi;
#endif
extern void *dinput_wgl;
extern void *dinput;
#endif

typedef struct DISPLAYCONFIG_RATIONAL_CUSTOM {
  UINT32 Numerator;
  UINT32 Denominator;
} DISPLAYCONFIG_RATIONAL_CUSTOM;

typedef struct DISPLAYCONFIG_2DREGION_CUSTOM {
  UINT32 cx;
  UINT32 cy;
} DISPLAYCONFIG_2DREGION_CUSTOM;

typedef struct DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM {
  UINT64                          pixelRate;
  DISPLAYCONFIG_RATIONAL_CUSTOM          hSyncFreq;
  DISPLAYCONFIG_RATIONAL_CUSTOM          vSyncFreq;
  DISPLAYCONFIG_2DREGION_CUSTOM          activeSize;
  DISPLAYCONFIG_2DREGION_CUSTOM          totalSize;
  union {
    struct {
      UINT32 videoStandard  :16;
      UINT32 vSyncFreqDivider  :6;
      UINT32 reserved  :10;
    } AdditionalSignalInfo;
    UINT32 videoStandard;
  } dummyunionname;
  UINT32 scanLineOrdering;
} DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_TARGET_MODE_CUSTOM {
  DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM targetVideoSignalInfo;
} DISPLAYCONFIG_TARGET_MODE_CUSTOM;

typedef struct DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM {
  LUID   adapterId;
  UINT32 id;
  union {
    UINT32 modeInfoIdx;
    struct {
      UINT32 cloneGroupId  :16;
      UINT32 sourceModeInfoIdx  :16;
    } dummystructname;
  } dummyunionname;
  UINT32 statusFlags;
} DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM {
  POINTL PathSourceSize;
  RECTL  DesktopImageRegion;
  RECTL  DesktopImageClip;
} DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_SOURCE_MODE_CUSTOM {
  UINT32                    width;
  UINT32                    height;
  UINT32                    pixelFormat;
  POINTL                    position;
} DISPLAYCONFIG_SOURCE_MODE_CUSTOM;

typedef struct DISPLAYCONFIG_MODE_INFO_CUSTOM {
  UINT32                       infoType;
  UINT32                       id;
  LUID                         adapterId;
  union {
    DISPLAYCONFIG_TARGET_MODE_CUSTOM        targetMode;
    DISPLAYCONFIG_SOURCE_MODE_CUSTOM        sourceMode;
    DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM desktopImageInfo;
  } dummyunionname;
} DISPLAYCONFIG_MODE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_PATH_TARGET_INFO_CUSTOM {
  LUID                                  adapterId;
  UINT32                                id;
  union {
    UINT32 modeInfoIdx;
    struct {
      UINT32 desktopModeInfoIdx  :16;
      UINT32 targetModeInfoIdx  :16;
    } dummystructname;
  } dummyunionname;
  UINT32 outputTechnology;
  UINT32 rotation;
  UINT32 scaling;
  DISPLAYCONFIG_RATIONAL_CUSTOM refreshRate;
  UINT32 scanLineOrdering;
  BOOL targetAvailable;
  UINT32 statusFlags;
} DISPLAYCONFIG_PATH_TARGET_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_PATH_INFO_CUSTOM {
  DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM sourceInfo;
  DISPLAYCONFIG_PATH_TARGET_INFO_CUSTOM targetInfo;
  UINT32                         flags;
} DISPLAYCONFIG_PATH_INFO_CUSTOM;

typedef LONG (WINAPI *QUERYDISPLAYCONFIG)(UINT32, UINT32*, DISPLAYCONFIG_PATH_INFO_CUSTOM*, UINT32*, DISPLAYCONFIG_MODE_INFO_CUSTOM*, UINT32*);
typedef LONG (WINAPI *GETDISPLAYCONFIGBUFFERSIZES)(UINT32, UINT32*, UINT32*);

static bool g_win32_resized         = false;
bool g_win32_restore_desktop        = false;
static bool doubleclick_on_titlebar = false;
static bool g_taskbar_is_created    = false;
bool g_win32_inited                 = false;
static bool g_win32_quit            = false;

static int g_win32_pos_x            = CW_USEDEFAULT;
static int g_win32_pos_y            = CW_USEDEFAULT;
static unsigned g_win32_pos_width   = 0;
static unsigned g_win32_pos_height  = 0;

unsigned g_win32_resize_width       = 0;
unsigned g_win32_resize_height      = 0;
static unsigned g_taskbar_message   = 0;
static unsigned win32_monitor_count = 0;

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

bool win32_taskbar_is_created(void)
{
   return g_taskbar_is_created;
}

bool doubleclick_on_titlebar_pressed(void)
{
   return doubleclick_on_titlebar;
}

void unset_doubleclick_on_titlebar(void)
{
   doubleclick_on_titlebar = false;
}

static INT_PTR_COMPAT CALLBACK PickCoreProc(
      HWND hDlg, UINT message,
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
                        path_set(RARCH_PATH_CORE, info->path);
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

   if (hm_to_use)
   {
      memset(mon, 0, sizeof(*mon));
      mon->cbSize = sizeof(MONITORINFOEX);

      GetMonitorInfo(*hm_to_use, (LPMONITORINFO)mon);
   }
}

bool win32_load_content_from_gui(const char *szFilename)
{
   /* poll list of current cores */
   size_t list_size;
   content_ctx_info_t content_info  = { 0 };
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info     = NULL;

   core_info_get_list(&core_info_list);

   if (!core_info_list)
      return false;

   core_info_list_get_supported_cores(core_info_list,
      (const char*)szFilename, &core_info, &list_size);

   if (!list_size)
      return false;

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

         if (string_is_equal(path_get(RARCH_PATH_CORE), info->path))
         {
            /* Our previous core supports the current rom */
            task_push_load_content_with_current_core_from_companion_ui(
               NULL,
               &content_info,
               CORE_TYPE_PLAIN,
               NULL, NULL);
            return true;
         }
      }
   }

   /* Poll for cores for current rom since none exist. */
   if (list_size == 1)
   {
      /*pick core that only exists and is bound to work. Ish. */
      const core_info_t *info = (const core_info_t*)&core_info[0];

      if (info)
      {
         task_push_load_content_with_new_core_from_companion_ui(
            info->path, NULL, NULL, &content_info, NULL, NULL);
         return true;
      }
   }
   else
   {
      bool            okay = false;
      settings_t *settings = config_get_ptr();

      /* Fullscreen: Show mouse cursor for dialog */
      if (settings->bools.video_fullscreen)
         video_driver_show_mouse();

      /* Pick one core that could be compatible, ew */
      if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PICKCORE),
         main_window.hwnd, PickCoreProc, (LPARAM)NULL) == IDOK)
      {
         task_push_load_content_with_current_core_from_companion_ui(
            NULL, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
         okay = true;
      }

      /* Fullscreen: Hide mouse cursor after dialog */
      if (settings->bools.video_fullscreen)
         video_driver_hide_mouse();
      return okay;
   }
   return false;
}

static bool win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   if (DragQueryFileR((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      bool okay        = false;
#ifdef LEGACY_WIN32
      char szFilename[1024];
      szFilename[0]    = '\0';

      DragQueryFileR((HDROP)wparam, 0, szFilename, sizeof(szFilename));
#else
      wchar_t wszFilename[4096];
      char *szFilename = NULL;
      wszFilename[0]   = L'\0';

      DragQueryFileR((HDROP)wparam, 0, wszFilename, sizeof(wszFilename));
      szFilename = utf16_to_utf8_string_alloc(wszFilename);
#endif
      okay = win32_load_content_from_gui(szFilename);
#ifndef LEGACY_WIN32
      if (szFilename)
         free(szFilename);
#endif

      return okay;
   }

   return false;
}

static void win32_set_position_from_config(void)
{
   settings_t *settings  = config_get_ptr();
   int border_thickness  = GetSystemMetrics(SM_CXSIZEFRAME);
   int title_bar_height  = GetSystemMetrics(SM_CYCAPTION);

   if (!settings->bools.video_window_save_positions)
      return;

   g_win32_pos_x         = settings->uints.window_position_x;
   g_win32_pos_y         = settings->uints.window_position_y;
   g_win32_pos_width     = settings->uints.window_position_width
      + border_thickness * 2;
   g_win32_pos_height    = settings->uints.window_position_height
      + border_thickness * 2 + title_bar_height;
}

static void win32_save_position(void)
{
   RECT rect;
   WINDOWPLACEMENT placement;
   int border_thickness     = GetSystemMetrics(SM_CXSIZEFRAME);
   int title_bar_height     = GetSystemMetrics(SM_CYCAPTION);
   int menu_bar_height      = GetSystemMetrics(SM_CYMENU);
   settings_t *settings     = config_get_ptr();

   memset(&placement, 0, sizeof(placement));

   placement.length         = sizeof(placement);

   GetWindowPlacement(main_window.hwnd, &placement);

   g_win32_pos_x = placement.rcNormalPosition.left;
   g_win32_pos_y = placement.rcNormalPosition.top;

   if (GetWindowRect(main_window.hwnd, &rect))
   {
      g_win32_pos_width  = rect.right  - rect.left;
      g_win32_pos_height = rect.bottom - rect.top;
   }
   if (settings && settings->bools.video_window_save_positions)
   {
      if (!settings->bools.video_fullscreen && !retroarch_is_forced_fullscreen() && !retroarch_is_switching_display_mode())
      {
         settings->uints.window_position_x      = g_win32_pos_x;
         settings->uints.window_position_y      = g_win32_pos_y;
         settings->uints.window_position_width  = g_win32_pos_width - border_thickness * 2;
         settings->uints.window_position_height = g_win32_pos_height - border_thickness * 2 - title_bar_height - (settings->bools.ui_menubar_enable ? menu_bar_height : 0);
      }
   }
}

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
               mod |=  RETROKMOD_CTRL;
            if (GetKeyState(VK_MENU)    & 0x80)
               mod |=  RETROKMOD_ALT;
            if (GetKeyState(VK_CAPITAL) & 0x81)
               mod |= RETROKMOD_CAPSLOCK;
            if (GetKeyState(VK_SCROLL)  & 0x81)
               mod |= RETROKMOD_SCROLLOCK;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
               mod |= RETROKMOD_META;

            /* Seems to be hard to synchronize
             * WM_CHAR and WM_KEYDOWN properly.
             */
            input_keyboard_event(true, RETROK_UNKNOWN, wparam, mod,
                  RETRO_DEVICE_KEYBOARD);
         }
         return TRUE;
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         *quit = true;
         {
            uint16_t mod          = 0;
            unsigned keycode      = 0;
            bool keydown          = true;
            unsigned keysym       = (lparam >> 16) & 0xff;
#if _WIN32_WINNT >= 0x0501 /* XP */
            settings_t *settings  = config_get_ptr();
#endif

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

#if _WIN32_WINNT >= 0x0501 /* XP */
            if (settings && string_is_equal(settings->arrays.input_driver, "raw"))
               keysym             = (unsigned)wparam;
            else
#endif
            {
#ifdef HAVE_DINPUT
               /* extended keys will map to dinput if the high bit is set */
               if (input_get_ptr() == &input_dinput && (lparam >> 24 & 0x1))
                  keysym |= 0x80;
#else
               /* fix key binding issues on winraw when DirectInput is not available */
#endif
            }

            /* Key released? */
            if (message == WM_KEYUP || message == WM_SYSKEYUP)
               keydown            = false;

            keycode = input_keymaps_translate_keysym_to_rk(keysym);

            input_keyboard_event(keydown, keycode,
                  0, mod, RETRO_DEVICE_KEYBOARD);

            if (message != WM_SYSKEYDOWN)
               return 0;

            if (
                  wparam == VK_F10  ||
                  wparam == VK_MENU ||
                  wparam == VK_RSHIFT
               )
               return 0;
         }
         return DefWindowProc(hwnd, message, wparam, lparam);
      case WM_MOVE:
         win32_save_position();
         break;
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
         win32_save_position();

         g_win32_quit  = true;
         *quit         = true;
         break;
      case WM_SIZE:
         /* Do not send resize message if we minimize. */
         if (  wparam != SIZE_MAXHIDE &&
               wparam != SIZE_MINIMIZED)
         {
            if (LOWORD(lparam) != g_win32_resize_width ||
                  HIWORD(lparam) != g_win32_resize_height)
            {
               g_win32_resize_width  = LOWORD(lparam);
               g_win32_resize_height = HIWORD(lparam);
               g_win32_resized       = true;
            }
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

#if defined(HAVE_D3D) || defined (HAVE_D3D10) || defined (HAVE_D3D11) || defined (HAVE_D3D12)
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
      case WM_MOVE:
      case WM_SIZE:
      case WM_COMMAND:
         ret = WndProcCommon(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         {
            LPCREATESTRUCT p_cs   = (LPCREATESTRUCT)lparam;
            curD3D                = p_cs->lpCreateParams;
            g_win32_inited        = true;

            if (DragAcceptFiles_func)
               DragAcceptFiles_func(hwnd, true);
         }
         return 0;
   }

#if _WIN32_WINNT >= 0x0500 /* 2K */
      if (g_taskbar_message && message == g_taskbar_message)
         g_taskbar_is_created = true;
#endif

#ifdef HAVE_DINPUT
      if (input_get_ptr() == &input_dinput)
      {
         void* input_data = input_get_data();
         if (input_data && dinput_handle_message(input_data,
               message, wparam, lparam))
            return 0;
      }
#endif
   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN)
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
      case WM_MOVE:
      case WM_SIZE:
      case WM_COMMAND:
         ret = WndProcCommon(&quit,
               hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         create_graphics_context(hwnd, &g_win32_quit);

         if (DragAcceptFiles_func)
            DragAcceptFiles_func(hwnd, true);
         return 0;
   }

#if _WIN32_WINNT >= 0x0500 /* 2K */
      if (g_taskbar_message && message == g_taskbar_message)
         g_taskbar_is_created = true;
#endif

#ifdef HAVE_DINPUT
   if (dinput_wgl && dinput_handle_message(dinput_wgl,
            message, wparam, lparam))
      return 0;
#endif
   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_GDI
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
                  StretchBlt(gdi->winDC,
                        0, 0,
                        gdi->screen_width, gdi->screen_height,
                        gdi->memDC, 0, 0, gdi->video_width, gdi->video_height, SRCCOPY);
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
      case WM_MOVE:
      case WM_SIZE:
      case WM_COMMAND:
         ret = WndProcCommon(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
         break;
      case WM_CREATE:
         create_gdi_context(hwnd, &g_win32_quit);

         if (DragAcceptFiles_func)
            DragAcceptFiles_func(hwnd, true);
         return 0;
   }

#if _WIN32_WINNT >= 0x0500 /* 2K */
      if (g_taskbar_message && message == g_taskbar_message)
         g_taskbar_is_created = true;
#endif

#ifdef HAVE_DINPUT
   if (dinput_gdi && dinput_handle_message(dinput_gdi,
            message, wparam, lparam))
      return 0;
#endif
   return DefWindowProc(hwnd, message, wparam, lparam);
}
#endif

bool win32_window_create(void *data, unsigned style,
      RECT *mon_rect, unsigned width,
      unsigned height, bool fullscreen)
{
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
   DEV_BROADCAST_DEVICEINTERFACE notification_filter;
#endif
   settings_t *settings  = config_get_ptr();
#ifndef _XBOX
   unsigned user_width   = width;
   unsigned user_height  = height;

   if (settings->bools.video_window_save_positions
         && !fullscreen)
   {
      user_width = g_win32_pos_width;
      user_height= g_win32_pos_height;
   }
   main_window.hwnd = CreateWindowEx(0,
         msg_hash_to_str(MSG_PROGRAM), msg_hash_to_str(MSG_PROGRAM),
         style,
         fullscreen ? mon_rect->left : g_win32_pos_x,
         fullscreen ? mon_rect->top  : g_win32_pos_y,
         user_width,
         user_height,
         NULL, NULL, NULL, data);
   if (!main_window.hwnd)
      return false;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
   g_taskbar_message = RegisterWindowMessage("TaskbarButtonCreated");

   ZeroMemory(&notification_filter, sizeof(notification_filter) );
   notification_filter.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
   notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
   notification_filter.dbcc_classguid  = GUID_DEVINTERFACE_HID;
   notification_handler                = RegisterDeviceNotification(
      main_window.hwnd, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE);

   if (!notification_handler)
      RARCH_ERR("Error registering for notifications\n");
#endif

   video_driver_display_type_set(RARCH_DISPLAY_WIN32);
   video_driver_display_set(0);
   video_driver_display_userdata_set((uintptr_t)&main_window);
   video_driver_window_set((uintptr_t)main_window.hwnd);

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
   if (!settings->bools.video_window_show_decorations)
      SetWindowLongPtr(main_window.hwnd, GWL_STYLE, WS_POPUP);

   /* Windows 2000 and above use layered windows to enable transparency */
   if (settings->uints.video_window_opacity < 100)
   {
      SetWindowLongPtr(main_window.hwnd,
           GWL_EXSTYLE,
           GetWindowLongPtr(main_window.hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
      SetLayeredWindowAttributes(main_window.hwnd, 0, (255 *
               settings->uints.video_window_opacity) / 100, LWA_ALPHA);
   }
#endif
#endif
   return true;
}
#endif

bool win32_get_metrics(void *data,
   enum display_metric_types type, float *value)
{
#if !defined(_XBOX)
   HDC monitor            = GetDC(NULL);
   int pixels_x           = GetDeviceCaps(monitor, HORZRES);
   int pixels_y           = GetDeviceCaps(monitor, VERTRES);
   int physical_width     = GetDeviceCaps(monitor, HORZSIZE);
   int physical_height    = GetDeviceCaps(monitor, VERTSIZE);

   ReleaseDC(NULL, monitor);

   switch (type)
   {
      case DISPLAY_METRIC_PIXEL_WIDTH:
         *value = pixels_x;
         return true;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         *value = pixels_y;
         return true;
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         return true;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         return true;
      case DISPLAY_METRIC_DPI:
         /* 25.4 mm in an inch. */
         *value = 254 * pixels_x / physical_width / 10;
         return true;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         break;
   }
#endif

   return false;
}

void win32_monitor_init(void)
{
#if !defined(_XBOX)
   win32_monitor_count = 0;
   EnumDisplayMonitors(NULL, NULL,
         win32_monitor_enum_proc, 0);
#endif
   g_win32_quit              = false;
}

static bool win32_monitor_set_fullscreen(
      unsigned width, unsigned height,
      unsigned refresh, char *dev_name)
{
#if !defined(_XBOX)
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

   return win32_change_display_settings(dev_name, &devmode,
         CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
#endif
}

void win32_show_cursor(void *data, bool state)
{
#if !defined(_XBOX)
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
#endif
}

void win32_check_window(bool *quit, bool *resize,
      unsigned *width, unsigned *height)
{
#if !defined(_XBOX)
   bool video_is_threaded = video_driver_is_threaded();
   if (video_is_threaded)
      ui_companion_win32.application->process_events();
   *quit                  = g_win32_quit;

   if (g_win32_resized)
   {
      *resize             = true;
      *width              = g_win32_resize_width;
      *height             = g_win32_resize_height;
      g_win32_resized     = false;
   }
#endif
}

bool win32_suppress_screensaver(void *data, bool enable)
{
#if !defined(_XBOX)
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

               powerSetRequest( Request, PowerRequestDisplayRequired);
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
#endif

   return false;
}

void win32_set_style(MONITORINFOEX *current_mon, HMONITOR *hm_to_use,
   unsigned *width, unsigned *height, bool fullscreen, bool windowed_full,
   RECT *rect, RECT *mon_rect, DWORD *style)
{
#if !defined(_XBOX)
   bool position_set_from_config = false;
   settings_t *settings          = config_get_ptr();
   if (fullscreen)
   {
      /* Windows only reports the refresh rates for modelines as
       * an integer, so video_refresh_rate needs to be rounded. Also, account
       * for black frame insertion using video_refresh_rate set to half
       * of the display refresh rate, as well as higher vsync swap intervals. */
      float refresh_mod    = settings->bools.video_black_frame_insertion ? 2.0f : 1.0f;
      unsigned refresh     = roundf(settings->floats.video_refresh_rate
            * refresh_mod * settings->uints.video_swap_interval);

      if (windowed_full)
      {
         *style                = WS_EX_TOPMOST | WS_POPUP;
         g_win32_resize_width  = *width  = mon_rect->right  - mon_rect->left;
         g_win32_resize_height = *height = mon_rect->bottom - mon_rect->top;
      }
      else
      {
         *style          = WS_POPUP | WS_VISIBLE;

         if (!win32_monitor_set_fullscreen(*width, *height,
                  refresh, current_mon->szDevice)) { }

         /* Display settings might have changed, get new coordinates. */
         GetMonitorInfo(*hm_to_use, (LPMONITORINFO)current_mon);
         *mon_rect = current_mon->rcMonitor;
      }
   }
   else
   {
      *style          = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      rect->right     = *width;
      rect->bottom    = *height;

      AdjustWindowRect(rect, *style, FALSE);

      if (settings->bools.video_window_save_positions)
      {
         win32_set_position_from_config();
         if (g_win32_pos_width != 0 && g_win32_pos_height != 0)
            position_set_from_config = true;
      }

      if (position_set_from_config)
      {
         g_win32_resize_width  = *width   = g_win32_pos_width;
         g_win32_resize_height = *height  = g_win32_pos_height;
      }
      else
      {
         g_win32_resize_width  = *width   = rect->right  - rect->left;
         g_win32_resize_height = *height  = rect->bottom - rect->top;
      }
   }
#endif
}

void win32_set_window(unsigned *width, unsigned *height,
      bool fullscreen, bool windowed_full, void *rect_data)
{
#if !defined(_XBOX)
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
         g_win32_resize_height = *height += rc_temp.top + rect->top;
         SetWindowPos(main_window.hwnd, NULL, 0, 0, *width, *height, SWP_NOMOVE);
      }

      ShowWindow(main_window.hwnd, SW_RESTORE);
      UpdateWindow(main_window.hwnd);
      SetForegroundWindow(main_window.hwnd);

      if (window)
         window->set_focused(&main_window);
   }

   win32_show_cursor(NULL, !fullscreen);
#endif
}

bool win32_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#if !defined(_XBOX)
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

   mon_rect                    = current_mon.rcMonitor;
   g_win32_resize_width        = width;
   g_win32_resize_height       = height;

   windowed_full               = settings->bools.video_windowed_fullscreen;

   win32_set_style(&current_mon, &hm_to_use, &width, &height,
         fullscreen, windowed_full, &rect, &mon_rect, &style);

   if (!win32_window_create(data, style,
            &mon_rect, width, height, fullscreen))
      return false;

   win32_set_window(&width, &height,
         fullscreen, windowed_full, &rect);

   /* Wait until context is created (or failed to do so ...).
    * Please don't remove the (res = ) as GetMessage can return -1. */
   while (!g_win32_inited && !g_win32_quit
         && (res = GetMessage(&msg, main_window.hwnd, 0, 0)) != 0)
   {
      if (res == -1)
      {
         RARCH_ERR("GetMessage error code %d\n", GetLastError());
         break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (g_win32_quit)
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

bool win32_has_focus(void *data)
{
   if (g_win32_inited)
      if (GetForegroundWindow() == main_window.hwnd)
         return true;

   return false;
}

HWND win32_get_window(void)
{
#ifdef _XBOX
   return NULL;
#else
   return main_window.hwnd;
#endif
}

void win32_window_reset(void)
{
   g_win32_quit            = false;
   g_win32_restore_desktop = false;
}

void win32_destroy_window(void)
{
#ifndef _XBOX
   UnregisterClass(msg_hash_to_str(MSG_PROGRAM), 
         GetModuleHandle(NULL));
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x500 /* 2K */
   UnregisterDeviceNotification(notification_handler);
#endif
#endif
   main_window.hwnd = NULL;
}

void win32_get_video_output_prev(
      unsigned *width, unsigned *height)
{
   DEVMODE dm;
   unsigned i;
   bool found           = false;
   unsigned prev_width  = 0;
   unsigned prev_height = 0;
   unsigned curr_width  = 0;
   unsigned curr_height = 0;

   if (win32_get_video_output(&dm, -1, sizeof(dm)))
   {
      curr_width  = dm.dmPelsWidth;
      curr_height = dm.dmPelsHeight;
   }

   for (i = 0; win32_get_video_output(&dm, i, sizeof(dm)); i++)
   {
      if (     dm.dmPelsWidth  == curr_width
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

float win32_get_refresh_rate(void *data)
{
   float refresh_rate                      = 0.0f;
#if _WIN32_WINNT >= 0x0601 || _WIN32_WINDOWS >= 0x0601 /* Win 7 */
   OSVERSIONINFO version_info;
   UINT32 TopologyID;
   unsigned int NumPathArrayElements       = 0;
   unsigned int NumModeInfoArrayElements   = 0;
   DISPLAYCONFIG_PATH_INFO_CUSTOM *PathInfoArray  = NULL;
   DISPLAYCONFIG_MODE_INFO_CUSTOM *ModeInfoArray  = NULL;
   int result                              = 0;
#ifdef HAVE_DYNAMIC
    static QUERYDISPLAYCONFIG pQueryDisplayConfig;
    static GETDISPLAYCONFIGBUFFERSIZES pGetDisplayConfigBufferSizes;
    if (!pQueryDisplayConfig)
        pQueryDisplayConfig = (QUERYDISPLAYCONFIG)GetProcAddress(GetModuleHandle("user32.dll"), "QueryDisplayConfig");

    if (!pGetDisplayConfigBufferSizes)
        pGetDisplayConfigBufferSizes = (GETDISPLAYCONFIGBUFFERSIZES)GetProcAddress(GetModuleHandle("user32.dll"), "GetDisplayConfigBufferSizes");
#else
    static QUERYDISPLAYCONFIG pQueryDisplayConfig                   = QueryDisplayConfig;
    static GETDISPLAYCONFIGBUFFERSIZES pGetDisplayConfigBufferSizes = GetDisplayConfigBufferSizes;
#endif

   version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   if (!GetVersionEx(&version_info))
      return refresh_rate;

   if (version_info.dwMajorVersion < 6 ||
       (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion < 1))
       return refresh_rate;

   result = pGetDisplayConfigBufferSizes(QDC_DATABASE_CURRENT,
                                        &NumPathArrayElements,
                                        &NumModeInfoArrayElements);

   if (result != ERROR_SUCCESS)
      return refresh_rate;

   PathInfoArray = (DISPLAYCONFIG_PATH_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_PATH_INFO_CUSTOM) * NumPathArrayElements);
   ModeInfoArray = (DISPLAYCONFIG_MODE_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_MODE_INFO_CUSTOM) * NumModeInfoArrayElements);

   result = pQueryDisplayConfig(QDC_DATABASE_CURRENT,
                               &NumPathArrayElements,
                               PathInfoArray,
                               &NumModeInfoArrayElements,
                               ModeInfoArray,
                               &TopologyID);

   if (result == ERROR_SUCCESS && NumPathArrayElements >= 1)
      refresh_rate = (float) PathInfoArray[0].targetInfo.refreshRate.Numerator /
                             PathInfoArray[0].targetInfo.refreshRate.Denominator;

   free(ModeInfoArray);
   free(PathInfoArray);

#endif
   return refresh_rate;
}

void win32_get_video_output_next(
      unsigned *width, unsigned *height)
{
   DEVMODE dm;
   int i;
   bool found           = false;
   unsigned curr_width  = 0;
   unsigned curr_height = 0;

   if (win32_get_video_output(&dm, -1, sizeof(dm)))
   {
      curr_width  = dm.dmPelsWidth;
      curr_height = dm.dmPelsHeight;
   }

   for (i = 0; win32_get_video_output(&dm, i, sizeof(dm)); i++)
   {
      if (found)
      {
         *width     = dm.dmPelsWidth;
         *height    = dm.dmPelsHeight;
         break;
      }

      if (     dm.dmPelsWidth  == curr_width
            && dm.dmPelsHeight == curr_height)
         found = true;
   }
}

static BOOL win32_internal_get_video_output(DWORD iModeNum, DEVMODE *dm)
{
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
   return EnumDisplaySettingsEx(NULL, iModeNum, dm, EDS_ROTATEDMODE);
#else
   return EnumDisplaySettings(NULL, iModeNum, dm);
#endif
}

bool win32_get_video_output(DEVMODE *dm, int mode, size_t len)
{
   memset(dm, 0, len);
   dm->dmSize  = len;

   if (win32_internal_get_video_output((mode == -1) ? ENUM_CURRENT_SETTINGS : mode,
            dm) == 0)
      return false;
   return true;
}

void win32_get_video_output_size(unsigned *width, unsigned *height)
{
   DEVMODE dm;

   if (win32_get_video_output(&dm, -1, sizeof(dm)))
   {
      *width  = dm.dmPelsWidth;
      *height = dm.dmPelsHeight;
   }
}

void win32_setup_pixel_format(HDC hdc, bool supports_gl)
{
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

   SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
}
