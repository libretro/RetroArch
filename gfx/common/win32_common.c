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
enum
{
   IDR_MENU          = 101,
   IDR_PICKCORE      = 103,
   IDR_ACCELERATOR1,

   ID_M_LOAD_CONTENT = 40001,
   ID_CORELISTBOX,
   ID_M_RESET,
   ID_M_QUIT,
   ID_M_MENU_TOGGLE,
   ID_M_PAUSE_TOGGLE,
   ID_M_LOAD_CORE,
   ID_M_LOAD_STATE,
   ID_M_SAVE_STATE,
   ID_M_DISK_CYCLE,
   ID_M_DISK_NEXT,
   ID_M_DISK_PREV,
   ID_M_WINDOW_SCALE_1X,
   ID_M_WINDOW_SCALE_2X,
   ID_M_WINDOW_SCALE_3X,
   ID_M_WINDOW_SCALE_4X,
   ID_M_WINDOW_SCALE_5X,
   ID_M_WINDOW_SCALE_6X,
   ID_M_WINDOW_SCALE_7X,
   ID_M_WINDOW_SCALE_8X,
   ID_M_WINDOW_SCALE_9X,
   ID_M_WINDOW_SCALE_10X,
   ID_M_FULL_SCREEN,
   ID_M_MOUSE_GRAB,
   ID_M_STATE_INDEX_AUTO,
   ID_M_STATE_INDEX_0,
   ID_M_STATE_INDEX_1,
   ID_M_STATE_INDEX_2,
   ID_M_STATE_INDEX_3,
   ID_M_STATE_INDEX_4,
   ID_M_STATE_INDEX_5,
   ID_M_STATE_INDEX_6,
   ID_M_STATE_INDEX_7,
   ID_M_STATE_INDEX_8,
   ID_M_STATE_INDEX_9,
   ID_M_TAKE_SCREENSHOT,
   ID_M_MUTE_TOGGLE,
   ID_M_TOGGLE_DESKTOP
};

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

typedef struct DISPLAYCONFIG_RATIONAL_CUSTOM
{
   UINT32 Numerator;
   UINT32 Denominator;
} DISPLAYCONFIG_RATIONAL_CUSTOM;

typedef struct DISPLAYCONFIG_2DREGION_CUSTOM
{
   UINT32 cx;
   UINT32 cy;
} DISPLAYCONFIG_2DREGION_CUSTOM;

typedef struct DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM
{
   UINT64                          pixelRate;
   DISPLAYCONFIG_RATIONAL_CUSTOM          hSyncFreq;
   DISPLAYCONFIG_RATIONAL_CUSTOM          vSyncFreq;
   DISPLAYCONFIG_2DREGION_CUSTOM          activeSize;
   DISPLAYCONFIG_2DREGION_CUSTOM          totalSize;
   union
   {
      struct
      {
         UINT32 videoStandard  :16;
         UINT32 vSyncFreqDivider  :6;
         UINT32 reserved  :10;
      } AdditionalSignalInfo;
      UINT32 videoStandard;
   } dummyunionname;
   UINT32 scanLineOrdering;
} DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_TARGET_MODE_CUSTOM
{
   DISPLAYCONFIG_VIDEO_SIGNAL_INFO_CUSTOM targetVideoSignalInfo;
} DISPLAYCONFIG_TARGET_MODE_CUSTOM;

typedef struct DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM
{
   LUID   adapterId;
   UINT32 id;
   union
   {
      UINT32 modeInfoIdx;
      struct
      {
         UINT32 cloneGroupId  :16;
         UINT32 sourceModeInfoIdx  :16;
      } dummystructname;
   } dummyunionname;
   UINT32 statusFlags;
} DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM
{
   POINTL PathSourceSize;
   RECTL  DesktopImageRegion;
   RECTL  DesktopImageClip;
} DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_SOURCE_MODE_CUSTOM
{
   UINT32                    width;
   UINT32                    height;
   UINT32                    pixelFormat;
   POINTL                    position;
} DISPLAYCONFIG_SOURCE_MODE_CUSTOM;

typedef struct DISPLAYCONFIG_MODE_INFO_CUSTOM
{
   UINT32                       infoType;
   UINT32                       id;
   LUID                         adapterId;
   union
   {
      DISPLAYCONFIG_TARGET_MODE_CUSTOM        targetMode;
      DISPLAYCONFIG_SOURCE_MODE_CUSTOM        sourceMode;
      DISPLAYCONFIG_DESKTOP_IMAGE_INFO_CUSTOM desktopImageInfo;
   } dummyunionname;
} DISPLAYCONFIG_MODE_INFO_CUSTOM;

typedef struct DISPLAYCONFIG_PATH_TARGET_INFO_CUSTOM
{
   LUID                                  adapterId;
   UINT32                                id;
   union
   {
      UINT32 modeInfoIdx;
      struct
      {
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

typedef struct DISPLAYCONFIG_PATH_INFO_CUSTOM
{
   DISPLAYCONFIG_PATH_SOURCE_INFO_CUSTOM sourceInfo;
   DISPLAYCONFIG_PATH_TARGET_INFO_CUSTOM targetInfo;
   UINT32                         flags;
} DISPLAYCONFIG_PATH_INFO_CUSTOM;

typedef LONG (WINAPI *QUERYDISPLAYCONFIG)(UINT32, UINT32*, DISPLAYCONFIG_PATH_INFO_CUSTOM*, UINT32*, DISPLAYCONFIG_MODE_INFO_CUSTOM*, UINT32*);
typedef LONG (WINAPI *GETDISPLAYCONFIGBUFFERSIZES)(UINT32, UINT32*, UINT32*);

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

#if defined(_MSC_VER) && _MSC_VER <= 1200
#define INT_PTR_COMPAT int
#else
#define INT_PTR_COMPAT INT_PTR
#endif

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

static INT_PTR_COMPAT CALLBACK pick_core_proc(
      HWND hDlg, UINT message,
      WPARAM wParam, LPARAM lParam)
{
   size_t list_size;

   switch (message)
   {
      case WM_INITDIALOG:
         {
            const core_info_t *core_info     = NULL;
            core_info_list_t *core_info_list = NULL;
            /* Add items to list. */
            core_info_get_list(&core_info_list);
            core_info_list_get_supported_cores(core_info_list,
                  path_get(RARCH_PATH_CONTENT), &core_info, &list_size);
            if (list_size != 0)
            {
               size_t i;
               HWND hwndList = GetDlgItem(hDlg, ID_CORELISTBOX);
               for (i = 0; i < list_size; i++)
                  SendMessage(hwndList, LB_ADDSTRING, 0,
                        (LPARAM)core_info[i].display_name);
               /* Select the first item in the list */
               SendMessage(hwndList, LB_SETCURSEL, 0, 0);
               path_set(RARCH_PATH_CORE, core_info[0].path);
               SetFocus(hwndList);
            }
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
                        const core_info_t *core_info     = NULL;
                        core_info_list_t *core_info_list = NULL;
                        HWND hwndList = GetDlgItem(hDlg, ID_CORELISTBOX);
                        int lbItem    = (int)
                           SendMessage(hwndList, LB_GETCURSEL, 0, 0);

                        core_info_get_list(&core_info_list);
                        core_info_list_get_supported_cores(core_info_list,
                              path_get(RARCH_PATH_CONTENT), &core_info,
                              &list_size);
                        if (lbItem < 0 || (size_t)lbItem >= list_size)
                           break;
                        path_set(RARCH_PATH_CORE, core_info[lbItem].path);
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

static bool win32_load_content_from_gui(const char *szFilename)
{
   /* poll list of current cores */
   core_info_list_t *core_info_list = NULL;

   core_info_get_list(&core_info_list);

   if (core_info_list)
   {
      size_t list_size;
      content_ctx_info_t content_info  = { 0 };
      const core_info_t *core_info     = NULL;
      core_info_list_get_supported_cores(core_info_list,
            (const char*)szFilename, &core_info, &list_size);

      if (list_size)
      {
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
                     info->path, NULL, NULL, NULL, NULL, &content_info, NULL, NULL);
               return true;
            }
         }
         else
         {
            bool            okay              = false;
            settings_t *settings              = config_get_ptr();
            bool video_is_fs                  = settings->bools.video_fullscreen;
            video_driver_state_t *video_st    = video_state_get_ptr();
            bool needs_cursor                 =    video_is_fs
                                               && video_st->poke
                                               && video_st->poke->show_mouse;

            if (needs_cursor)
               video_st->poke->show_mouse(video_st->data, true);

            /* Pick one core that could be compatible. */
            if (win32_resources_pick_core_dialog(
                     main_window.hwnd, pick_core_proc) == IDOK)
            {
               task_push_load_content_with_current_core_from_companion_ui(
                     NULL, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
               okay = true;
            }

            if (needs_cursor)
               video_st->poke->show_mouse(video_st->data, false);

            return okay;
         }
      }
   }
   return false;
}

#ifdef LEGACY_WIN32
static bool win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   if (DragQueryFile((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      char szFilename[1024];
      szFilename[0]    = '\0';
      DragQueryFile((HDROP)wparam, 0, szFilename, sizeof(szFilename));
      return win32_load_content_from_gui(szFilename);
   }
   return false;
}
#else
static bool win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   if (DragQueryFileW((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      wchar_t wszFilename[4096];
      bool ret        = false;
      char *szFilename = NULL;
      wszFilename[0]   = L'\0';

      DragQueryFileW((HDROP)wparam, 0, wszFilename,
            sizeof(wszFilename) / sizeof(wszFilename[0]));
      szFilename = utf16_to_utf8_string_alloc(wszFilename);
      ret        = win32_load_content_from_gui(szFilename);
      if (szFilename)
         free(szFilename);
      if (ret)
         return true;
   }
   return false;
}
#endif

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

#ifdef HAVE_THREADS
/* Set by win32_browser() before calling browser->open() so
 * the threaded dialog knows whether the result should trigger a
 * core load or a content load. Only touched on the main thread.
 * Declared extern in ui_win32.c. */
enum win32_browser_mode g_win32_browser_mode =
   WIN32_BROWSER_MODE_LOAD_CONTENT;

static bool win32_browser(
      HWND owner,
      char *filename,
      size_t filename_size,
      const char *extensions,
      const char *title,
      const char *initial_dir,
      enum win32_browser_mode mode)
{
   bool result = false;
   const ui_browser_window_t *browser =
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t browser_state;

      /* These need to be big enough to hold the
       * path/name of any file the user may select. */
      char new_title[PATH_MAX];
      char new_file[PATH_MAX_LENGTH]; /* MAX_PATH-length path buffer */
      char new_dir[DIR_MAX_LENGTH];

      new_title[0] = '\0';
      new_file[0]  = '\0';
      new_dir[0]   = '\0';

      if (title && *title)
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      if (initial_dir && *initial_dir)
         strlcpy(new_dir, initial_dir, sizeof(new_dir));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = new_dir;
      browser_state.path     = new_file;
      browser_state.window   = owner;

      /* Stash the mode so the threaded dialog can tag its result. */
      g_win32_browser_mode   = mode;

      result = browser->open(&browser_state);

      /* With the threaded dialog, browser->open() spawns the thread
       * and returns false immediately.  The real result arrives via
       * WM_BROWSER_OPEN_RESULT.  The copy below is harmless but
       * will not contain anything useful in the threaded path. */
      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);
   }

   return result;
}
#else
/* Non-threaded fallback: the dialog blocks the main thread and the
 * result is returned synchronously to the caller (old behavior). */
static bool win32_browser(
      HWND owner,
      char *filename,
      size_t filename_size,
      const char *extensions,
      const char *title,
      const char *initial_dir)
{
   bool result = false;
   const ui_browser_window_t *browser =
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t browser_state;

      /* These need to be big enough to hold the
       * path/name of any file the user may select. */
      char new_title[PATH_MAX];
      char new_file[PATH_MAX_LENGTH];
      char new_dir[DIR_MAX_LENGTH];

      new_title[0] = '\0';
      new_file[0]  = '\0';
      new_dir[0]   = '\0';

      if (title && *title)
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      if (initial_dir && *initial_dir)
         strlcpy(new_dir, initial_dir, sizeof(new_dir));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = new_dir;
      browser_state.path     = new_file;
      browser_state.window   = owner;

      result = browser->open(&browser_state);

      /* browser->open() may update browser_state.path in-place;
       * copy the final path back to the caller's buffer. */
      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);
   }

   return result;
}
#endif /* HAVE_THREADS */

static LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode            = wparam & 0xffff;

   switch (mode)
   {
      case ID_M_LOAD_CORE:
         {
#ifndef HAVE_THREADS
            content_ctx_info_t content_info;
#endif
            char win32_file[PATH_MAX_LENGTH] = {0};
            settings_t *settings    = config_get_ptr();
            char    *title_cp       = NULL;
            const char *extensions  = "Libretro core (.dll)\0*.dll\0All Files\0*.*\0\0";
            const char *title       = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST);
            const char *initial_dir = settings->paths.directory_libretro;

            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

#ifdef HAVE_THREADS
            /* Fire-and-forget: the dialog runs on a worker thread.
             * The actual core-load happens in WM_BROWSER_OPEN_RESULT. */
            win32_browser(owner, win32_file, sizeof(win32_file),
                     extensions, title_cp, initial_dir,
                     WIN32_BROWSER_MODE_LOAD_CORE);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
#else
            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            if (!win32_browser(owner, win32_file, sizeof(win32_file),
                     extensions, title_cp, initial_dir))
            {
               if (title_wide)
                  free(title_wide);
               if (title_cp)
                  free(title_cp);
               break;
            }

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
            content_info.argc        = 0;
            content_info.argv        = NULL;
            content_info.args        = NULL;
            content_info.environ_get = NULL;
            task_push_load_new_core(
                     win32_file, NULL,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL);
#endif
         }
         break;
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            char *title_cp          = NULL;
            wchar_t *title_wide     = NULL;
            const char *extensions  = "All Files (*.*)\0*.*\0\0";
            const char *title       = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST);
            settings_t *settings    = config_get_ptr();
            const char *initial_dir = settings->paths.directory_menu_content;
#ifndef HAVE_THREADS
            bool browser            = true;
#endif

            /* Menubar accelerator hotkey is hijacked always, therefore must
             * press the keyboard event manually when blocking the accelerator. */
            if (     !settings->bools.ui_menubar_enable
                  || (!settings->bools.video_windowed_fullscreen && settings->bools.video_fullscreen))
            {
               input_keyboard_event(true, RETROK_o,
                     0, RETROK_LCTRL, RETRO_DEVICE_KEYBOARD);
               break;
            }

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            title_wide = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp = utf16_to_utf8_string_alloc(title_wide);

#ifdef HAVE_THREADS
            /* Fire-and-forget: the dialog runs on a worker thread.
             * The actual content-load happens in WM_BROWSER_OPEN_RESULT. */
            win32_browser(owner, win32_file, sizeof(win32_file),
                  extensions, title_cp, initial_dir,
                  WIN32_BROWSER_MODE_LOAD_CONTENT);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
#else
            browser = win32_browser(owner, win32_file, sizeof(win32_file),
                  extensions, title_cp, initial_dir);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);

            if (browser)
               win32_load_content_from_gui(win32_file);
#endif
         }
         break;
      case ID_M_RESET:
         command_event(CMD_EVENT_RESET, NULL);
         break;
      case ID_M_MUTE_TOGGLE:
         command_event(CMD_EVENT_AUDIO_MUTE_TOGGLE, NULL);
         break;
      case ID_M_MENU_TOGGLE:
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         break;
      case ID_M_PAUSE_TOGGLE:
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
         break;
      case ID_M_LOAD_STATE:
         command_event(CMD_EVENT_LOAD_STATE, NULL);
         break;
      case ID_M_SAVE_STATE:
         command_event(CMD_EVENT_SAVE_STATE, NULL);
         break;
      case ID_M_DISK_CYCLE:
         command_event(CMD_EVENT_DISK_EJECT_TOGGLE, NULL);
         break;
      case ID_M_DISK_NEXT:
         command_event(CMD_EVENT_DISK_NEXT, NULL);
         break;
      case ID_M_DISK_PREV:
         command_event(CMD_EVENT_DISK_PREV, NULL);
         break;
      case ID_M_FULL_SCREEN:
         {
            /* Menubar accelerator hotkey is hijacked always, therefore must
             * press the keyboard event manually when blocking the accelerator. */
            settings_t *settings    = config_get_ptr();
            if (     !settings->bools.ui_menubar_enable
                  || (!settings->bools.video_windowed_fullscreen && settings->bools.video_fullscreen))
            {
               input_keyboard_event(true, RETROK_RETURN,
                     0, RETROK_LALT, RETRO_DEVICE_KEYBOARD);
               break;
            }
         }
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
         break;
      case ID_M_MOUSE_GRAB:
         command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
         break;
      case ID_M_TAKE_SCREENSHOT:
         command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
         break;
      case ID_M_QUIT:
         PostMessage(owner, WM_CLOSE, 0, 0);
         break;
      case ID_M_TOGGLE_DESKTOP:
         command_event(CMD_EVENT_UI_COMPANION_TOGGLE, NULL);
         break;
      default:
         if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
         {
            unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
            retroarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);
            command_event(CMD_EVENT_RESIZE_WINDOWED_SCALE, NULL);
         }
         else if (mode == ID_M_STATE_INDEX_AUTO)
         {
            signed           idx = -1;
            settings_t *settings = config_get_ptr();
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         else if (mode >= (ID_M_STATE_INDEX_AUTO+1)
               && mode <= (ID_M_STATE_INDEX_AUTO+10))
         {
            signed           idx = (mode - (ID_M_STATE_INDEX_AUTO+1));
            settings_t *settings = config_get_ptr();
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         break;
   }

   return 0L;
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

bool win32_window_create(void *data, unsigned style,
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
bool win32_get_metrics(void *data,
   enum display_metric_types type, float *value)
{
   HDC monitor;
   bool ret = true;

   if (type == DISPLAY_METRIC_NONE)
   {
      *value = 0;
      return false;
   }

   monitor = GetDC(NULL);
   if (!monitor)
   {
      *value = 0;
      return false;
   }

   switch (type)
   {
      case DISPLAY_METRIC_PIXEL_WIDTH:
         *value = (float)GetDeviceCaps(monitor, HORZRES);
         break;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         *value = (float)GetDeviceCaps(monitor, VERTRES);
         break;
      case DISPLAY_METRIC_MM_WIDTH:
         *value = (float)GetDeviceCaps(monitor, HORZSIZE);
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = (float)GetDeviceCaps(monitor, VERTSIZE);
         break;
      case DISPLAY_METRIC_DPI:
         /* 25.4 mm in an inch. */
         {
            int pixels_x       = GetDeviceCaps(monitor, HORZRES);
            int physical_width = GetDeviceCaps(monitor, HORZSIZE);
            *value = (physical_width > 0)
               ? (float)(254 * pixels_x) / (float)(physical_width * 10)
               : 0.0f;
         }
         break;
      default:
         *value = 0;
         ret    = false;
         break;
   }

   ReleaseDC(NULL, monitor);
   return ret;
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

#ifdef HAVE_MENU
/* Given a Win32 Resource ID, return a RetroArch menu ID (for renaming the menu item) */
static enum msg_hash_enums menu_id_to_label_enum(unsigned int menuId)
{
   switch (menuId)
   {
      case ID_M_LOAD_CONTENT:
         return MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST;
      case ID_M_RESET:
         return MENU_ENUM_LABEL_VALUE_RESTART_CONTENT;
      case ID_M_QUIT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY;
      case ID_M_MENU_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE;
      case ID_M_PAUSE_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE;
      case ID_M_LOAD_CORE:
         return MENU_ENUM_LABEL_VALUE_CORE_LIST;
      case ID_M_LOAD_STATE:
         return MENU_ENUM_LABEL_VALUE_LOAD_STATE;
      case ID_M_SAVE_STATE:
         return MENU_ENUM_LABEL_VALUE_SAVE_STATE;
      case ID_M_DISK_CYCLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE;
      case ID_M_DISK_NEXT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT;
      case ID_M_DISK_PREV:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV;
      case ID_M_FULL_SCREEN:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY;
      case ID_M_MOUSE_GRAB:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE;
      case ID_M_TAKE_SCREENSHOT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT;
      case ID_M_MUTE_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE;
      default:
         break;
   }

   return MSG_UNKNOWN;
}

/* Given a RetroArch menu ID, get its shortcut key (meta key) */
static unsigned int menu_id_to_meta_key(unsigned int menu_id)
{
   switch (menu_id)
   {
      case ID_M_RESET:
         return RARCH_RESET;
      case ID_M_QUIT:
         return RARCH_QUIT_KEY;
      case ID_M_MENU_TOGGLE:
         return RARCH_MENU_TOGGLE;
      case ID_M_PAUSE_TOGGLE:
         return RARCH_PAUSE_TOGGLE;
      case ID_M_LOAD_STATE:
         return RARCH_LOAD_STATE_KEY;
      case ID_M_SAVE_STATE:
         return RARCH_SAVE_STATE_KEY;
      case ID_M_DISK_CYCLE:
         return RARCH_DISK_EJECT_TOGGLE;
      case ID_M_DISK_NEXT:
         return RARCH_DISK_NEXT;
      case ID_M_DISK_PREV:
         return RARCH_DISK_PREV;
      case ID_M_FULL_SCREEN:
         return RARCH_FULLSCREEN_TOGGLE_KEY;
      case ID_M_MOUSE_GRAB:
         return RARCH_GRAB_MOUSE_TOGGLE;
      case ID_M_TAKE_SCREENSHOT:
         return RARCH_SCREENSHOT;
      case ID_M_MUTE_TOGGLE:
         return RARCH_MUTE;
      default:
         break;
   }

   return 0;
}

/* Given a short key (meta key), get its name as a string.
 * For named keys the return value points into the global
 * input_config_key_map table.  For single printable-ASCII
 * characters the name is written into the caller-supplied
 * buffer (buf, buf_size) and the return value points there.
 * Returns NULL when no name can be determined. */
static const char *win32_meta_key_to_name(unsigned int meta_key,
      char *buf, size_t buf_size)
{
   int i = 0;
   const struct retro_keybind* key = &input_config_binds[0][meta_key];
   int key_code                    = key->key;

   for (;;)
   {
      const struct input_key_map* entry = &input_config_key_map[i];
      if (!entry->str)
         break;
      if (entry->key == (enum retro_key)key_code)
         return entry->str;
      i++;
   }

   if (key_code >= 32 && key_code < 127 && buf_size >= 2)
   {
      buf[0] = (char)key_code;
      buf[1] = '\0';
      return buf;
   }
   return NULL;
}

/* Replaces Menu Item text with localized menu text,
 * and displays the current shortcut key */
static void win32_localize_menu(HMENU menu)
{
#ifndef LEGACY_WIN32
   MENUITEMINFOW menu_item_info;
#else
   MENUITEMINFOA menu_item_info;
#endif
   int index = 0;

   for (;;)
   {
      enum msg_hash_enums label_enum;
      memset(&menu_item_info, 0, sizeof(menu_item_info));
      menu_item_info.cbSize     = sizeof(menu_item_info);
      menu_item_info.dwTypeData = NULL;
#if (WINVER >= 0x0500)
      menu_item_info.fMask      = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#else
      menu_item_info.fMask      =                            MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#endif

#ifndef LEGACY_WIN32
      if (!GetMenuItemInfoW(menu, index, true, &menu_item_info))
         break;
#else
      if (!GetMenuItemInfoA(menu, index, true, &menu_item_info))
         break;
#endif

      /* Recursion - call this on submenu items too */
      if (menu_item_info.hSubMenu)
         win32_localize_menu(menu_item_info.hSubMenu);

      label_enum = menu_id_to_label_enum(menu_item_info.wID);
      if (label_enum != MSG_UNKNOWN)
      {
         size_t final_len;
         size_t key_name_len;
#ifndef LEGACY_WIN32
         wchar_t* new_label_unicode = NULL;
#else
         char* new_label_ansi       = NULL;
#endif
         const char* new_label      = msg_hash_to_str(label_enum);
         unsigned int meta_key      = menu_id_to_meta_key(menu_item_info.wID);
         const char* new_label2     = new_label;
         const char* meta_key_name  = NULL;
         char* new_label_text       = NULL;
         char key_name_buf[2]       = {0};

         /* specific replacements:
            Load Content = "Ctrl+O"
            Fullscreen = "Alt+Enter" */
         if (label_enum ==
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST)
         {
            meta_key_name = "Ctrl+O";
            key_name_len  = STRLEN_CONST("Ctrl+O");
         }
         else if (label_enum ==
               MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY)
         {
            meta_key_name = "Alt+Enter";
            key_name_len  = STRLEN_CONST("Alt+Enter");
         }
         else if (meta_key != 0)
         {
            meta_key_name = win32_meta_key_to_name(meta_key,
                  key_name_buf, sizeof(key_name_buf));
            key_name_len  = meta_key_name ? strlen(meta_key_name) : 0;
         }

         /* Append localized name, tab character, and Shortcut Key */
         if (meta_key_name && string_is_not_equal(meta_key_name, "nul"))
         {
            size_t label_len = strlen(new_label);
            size_t buf_size  = label_len + key_name_len + 2;
            new_label_text   = (char*)malloc(buf_size);

            if (new_label_text)
            {
               size_t copy_len;
               new_label2              = new_label_text;
               copy_len                = strlcpy(new_label_text, new_label,
                     buf_size);
               new_label_text[  copy_len] = '\t';
               new_label_text[++copy_len] = '\0';
               strlcpy(new_label_text + copy_len, meta_key_name, buf_size - copy_len);
               /* Make first character of shortcut name uppercase */
               new_label_text[label_len + 1] = toupper(new_label_text[label_len + 1]);
            }
         }

#ifndef LEGACY_WIN32
         /* Convert string from UTF-8, then assign menu text */
         new_label_unicode         = utf8_to_utf16_string_alloc(new_label2);
         final_len                 = wcslen(new_label_unicode);
         menu_item_info.cch        = final_len;
         menu_item_info.dwTypeData = new_label_unicode;
         SetMenuItemInfoW(menu, index, true, &menu_item_info);
         free(new_label_unicode);
#else
         new_label_ansi            = utf8_to_local_string_alloc(new_label2);
         final_len                 = strlen(new_label_ansi);
         menu_item_info.cch        = final_len;
         menu_item_info.dwTypeData = new_label_ansi;
         SetMenuItemInfoA(menu, index, true, &menu_item_info);
         free(new_label_ansi);
#endif
         if (new_label_text)
            free(new_label_text);
      }
      index++;
   }
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

   if (win32_get_video_output(&dm, -1))
   {
      curr_width  = dm.dmPelsWidth;
      curr_height = dm.dmPelsHeight;
   }

   for (i = 0; win32_get_video_output(&dm, i); i++)
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
#if _WIN32_WINNT >= 0x0601 || _WIN32_WINDOWS >= 0x0601 /* Win 7 */
   UINT32 TopologyID;
   float refresh_rate                            = 0.0f;
   unsigned int NumPathArrayElements             = 0;
   unsigned int NumModeInfoArrayElements         = 0;
   DISPLAYCONFIG_PATH_INFO_CUSTOM *PathInfoArray = NULL;
   DISPLAYCONFIG_MODE_INFO_CUSTOM *ModeInfoArray = NULL;
#ifdef HAVE_DYLIB
   static QUERYDISPLAYCONFIG pQueryDisplayConfig;
   static GETDISPLAYCONFIGBUFFERSIZES pGetDisplayConfigBufferSizes;
   if (!pQueryDisplayConfig || !pGetDisplayConfigBufferSizes)
   {
      HMODULE user32 = GetModuleHandle("user32.dll");
      if (!pQueryDisplayConfig)
         pQueryDisplayConfig        = (QUERYDISPLAYCONFIG)
         GetProcAddress(user32, "QueryDisplayConfig");
      if (!pGetDisplayConfigBufferSizes)
         pGetDisplayConfigBufferSizes = (GETDISPLAYCONFIGBUFFERSIZES)
         GetProcAddress(user32, "GetDisplayConfigBufferSizes");
   }
#else
   static QUERYDISPLAYCONFIG pQueryDisplayConfig = QueryDisplayConfig;
   static GETDISPLAYCONFIGBUFFERSIZES pGetDisplayConfigBufferSizes = GetDisplayConfigBufferSizes;
#endif

   /* Both function pointers must be valid before proceeding. */
   if (!pQueryDisplayConfig || !pGetDisplayConfigBufferSizes)
      return 0.0f;

   if (pGetDisplayConfigBufferSizes(
            QDC_DATABASE_CURRENT,
            &NumPathArrayElements,
            &NumModeInfoArrayElements) != ERROR_SUCCESS)
      return 0.0f;

   PathInfoArray = (DISPLAYCONFIG_PATH_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_PATH_INFO_CUSTOM) * NumPathArrayElements);
   if (!PathInfoArray)
      return 0.0f;
   ModeInfoArray = (DISPLAYCONFIG_MODE_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_MODE_INFO_CUSTOM) * NumModeInfoArrayElements);
   if (!ModeInfoArray)
   {
      free(PathInfoArray);
      return 0.0f;
   }

   if (pQueryDisplayConfig(QDC_DATABASE_CURRENT,
            &NumPathArrayElements,
            PathInfoArray,
            &NumModeInfoArrayElements,
            ModeInfoArray,
            &TopologyID) == ERROR_SUCCESS
         && NumPathArrayElements >= 1
         && PathInfoArray[0].targetInfo.refreshRate.Denominator != 0)
      refresh_rate = (float)PathInfoArray[0].targetInfo.refreshRate.Numerator
         / PathInfoArray[0].targetInfo.refreshRate.Denominator;

   free(ModeInfoArray);
   free(PathInfoArray);
   return refresh_rate;
#else
   return 0.0f;
#endif
}

void win32_get_video_output_next(
      unsigned *width, unsigned *height)
{
   DEVMODE dm;
   int i;
   bool found           = false;
   unsigned curr_width  = 0;
   unsigned curr_height = 0;

   if (win32_get_video_output(&dm, -1))
   {
      curr_width  = dm.dmPelsWidth;
      curr_height = dm.dmPelsHeight;
   }

   for (i = 0; win32_get_video_output(&dm, i); i++)
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

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
#define WIN32_GET_VIDEO_OUTPUT(devName, iModeNum, dm) EnumDisplaySettingsEx(devName, iModeNum, dm, EDS_ROTATEDMODE)
#else
#define WIN32_GET_VIDEO_OUTPUT(devName, iModeNum, dm) EnumDisplaySettings(devName, iModeNum, dm)
#endif

bool win32_get_video_output(DEVMODE *dm, int mode)
{
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use        = NULL;
   unsigned mon_id           = 0;

   memset(dm, 0, sizeof(DEVMODE));
   dm->dmSize = sizeof(DEVMODE);

   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);

   return WIN32_GET_VIDEO_OUTPUT(
         current_mon.szDevice,
         (mode == -1) ? ENUM_CURRENT_SETTINGS : (DWORD)mode,
         dm) != 0;
}

void win32_get_video_output_size(void *data, unsigned *width,
   unsigned *height, char *desc, size_t len)
{
   DEVMODE dm;
   if (win32_get_video_output(&dm, -1))
   {
      *width  = dm.dmPelsWidth;
      *height = dm.dmPelsHeight;
   }
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
 *   IDR_MENU          → win32_resources_create_menu()
 *   IDR_ACCELERATOR1  → win32_resources_get_accelerator()
 *   IDD_PICKCORE      → win32_resources_pick_core_dialog()
 *   rarch.manifest    → apply_dpi_awareness()  (called from _init)
 * ---------------------------------------------------------------- */

static HACCEL s_accel_table = NULL;

/* DPI AWARENESS  (replaces media/rarch.manifest)
 * The manifest contained <dpiAware>true</dpiAware>.
 * We call the equivalent API at runtime. */
typedef HRESULT (WINAPI *pfn_SetProcessDpiAwareness)(int);

static void apply_dpi_awareness(void)
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
   /* Fallback for Vista / Win7 without shcore */
   SetProcessDPIAware();
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

/* MENU BAR  (replaces IDR_MENU from rarch.rc)
 * Always created with English labels; win32_localize_menu()
 * re-labels every item for the active language. */
HMENU win32_resources_create_menu(void)
{
   HMENU menu_bar, file_menu, command_menu, window_menu;
   HMENU audio_menu, disk_menu, savestate_menu, stateindex_menu;
   HMENU scale_menu;

   menu_bar = CreateMenu();
   if (!menu_bar)
      return NULL;

   /* ---- File ---- */
   file_menu = CreatePopupMenu();
   AppendMenuA(file_menu, MF_STRING, ID_M_LOAD_CORE,    "Load Core...");
   AppendMenuA(file_menu, MF_STRING, ID_M_LOAD_CONTENT, "Load Content...");
   AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
   AppendMenuA(file_menu, MF_STRING, ID_M_QUIT,         "Close");
   AppendMenuA(menu_bar,  MF_POPUP, (UINT_PTR)file_menu, "File");

   /* ---- Command ---- */
   command_menu = CreatePopupMenu();

   audio_menu = CreatePopupMenu();
   AppendMenuA(audio_menu, MF_STRING, ID_M_MUTE_TOGGLE, "Audio Mute Toggle");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)audio_menu, "Audio Options");

   disk_menu = CreatePopupMenu();
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_CYCLE, "Disk Eject Toggle");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_PREV,  "Disk Previous");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_NEXT,  "Disk Next");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)disk_menu, "Disk Options");

   savestate_menu = CreatePopupMenu();

   stateindex_menu = CreatePopupMenu();
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_AUTO, "Auto");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_0,    "0");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_1,    "1");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_2,    "2");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_3,    "3");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_4,    "4");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_5,    "5");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_6,    "6");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_7,    "7");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_8,    "8");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_9,    "9");
   AppendMenuA(savestate_menu, MF_POPUP,
         (UINT_PTR)stateindex_menu, "State Index");

   AppendMenuA(savestate_menu, MF_STRING, ID_M_LOAD_STATE, "Load State");
   AppendMenuA(savestate_menu, MF_STRING, ID_M_SAVE_STATE, "Save State");
   AppendMenuA(command_menu, MF_POPUP,
         (UINT_PTR)savestate_menu, "Save State Options");

   AppendMenuA(command_menu, MF_STRING, ID_M_RESET,           "Reset");
   AppendMenuA(command_menu, MF_STRING, ID_M_PAUSE_TOGGLE,    "Pause Toggle");
   AppendMenuA(command_menu, MF_STRING, ID_M_MENU_TOGGLE,     "Menu Toggle");
   AppendMenuA(command_menu, MF_STRING, ID_M_TAKE_SCREENSHOT, "Take Screenshot");
   AppendMenuA(command_menu, MF_STRING, ID_M_MOUSE_GRAB,      "Mouse Grab Toggle");
   AppendMenuA(menu_bar, MF_POPUP, (UINT_PTR)command_menu, "Command");

   /* ---- Window ---- */
   window_menu = CreatePopupMenu();

   scale_menu = CreatePopupMenu();
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_1X,  "1x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_2X,  "2x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_3X,  "3x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_4X,  "4x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_5X,  "5x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_6X,  "6x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_7X,  "7x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_8X,  "8x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_9X,  "9x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_10X, "10x");
   AppendMenuA(window_menu, MF_POPUP, (UINT_PTR)scale_menu, "Windowed Scale");

#ifdef HAVE_QT
   AppendMenuA(window_menu, MF_STRING, ID_M_TOGGLE_DESKTOP,
         "Toggle Desktop Menu");
#endif
   AppendMenuA(window_menu, MF_STRING, ID_M_FULL_SCREEN,
         "Toggle Exclusive Full Screen");
   AppendMenuA(menu_bar, MF_POPUP, (UINT_PTR)window_menu, "Window");

   return menu_bar;
}

/* "PICK CORE" DIALOG  (replaces IDD_PICKCORE)
 * Builds DLGTEMPLATE + DLGITEMTEMPLATE in memory. */

static LPWORD align_dword(LPWORD ptr)
{
   ULONG_PTR ul = (ULONG_PTR)ptr;
   ul  = (ul + 3) & ~(ULONG_PTR)3;
   return (LPWORD)ul;
}

static LPWORD append_wstr(LPWORD ptr, const WCHAR *str)
{
   int len = (int)wcslen(str) + 1;
   memcpy(ptr, str, len * sizeof(WCHAR));
   return ptr + len;
}

int win32_resources_pick_core_dialog(HWND parent, DLGPROC dlg_proc)
{
   BYTE buf[2048];
   DLGTEMPLATE *dlg;
   LPWORD p;
   DLGITEMTEMPLATE *item;

   memset(buf, 0, sizeof(buf));
   dlg = (DLGTEMPLATE *)buf;

   dlg->style = DS_3DLOOK | DS_CENTER | DS_MODALFRAME | DS_SHELLFONT
              | WS_CAPTION | WS_VISIBLE | WS_POPUP | WS_SYSMENU;
   dlg->dwExtendedStyle = 0;
   dlg->cdit  = 4;
   dlg->x     = 0;
   dlg->y     = 0;
   dlg->cx    = 225;
   dlg->cy    = 118;

   p = (LPWORD)(dlg + 1);
   *p++ = 0;                              /* no menu       */
   *p++ = 0;                              /* default class */
   p = append_wstr(p, L"Pick Core");      /* caption       */
   *p++ = 8;                              /* font size     */
   p = append_wstr(p, L"Ms Shell Dlg");   /* font name     */

   /* Control 1: LTEXT (static label) */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | SS_LEFT;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 9;   item->y  = 12;
   item->cx = 160; item->cy = 17;
   item->id = 0;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0082;          /* STATIC class  */
   p = append_wstr(p,
         L"Please select a core to use for the content loaded.\n"
         L"Otherwise, press 'Cancel' to cancel loading.");
   *p++ = 0;                              /* no creation data */

   /* Control 2: DEFPUSHBUTTON "OK" */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 170; item->y  = 15;
   item->cx = 50;  item->cy = 14;
   item->id = IDOK;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0080;          /* BUTTON class  */
   p = append_wstr(p, L"OK");
   *p++ = 0;

   /* Control 3: PUSHBUTTON "Cancel" */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 170; item->y  = 32;
   item->cx = 50;  item->cy = 14;
   item->id = IDCANCEL;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0080;          /* BUTTON class  */
   p = append_wstr(p, L"Cancel");
   *p++ = 0;

   /* Control 4: LISTBOX (core list) */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                         | LBS_NOINTEGRALHEIGHT | LBS_SORT | LBS_NOTIFY;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 5;   item->y  = 55;
   item->cx = 214; item->cy = 60;
   item->id = ID_CORELISTBOX;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0083;          /* LISTBOX class */
   *p++ = 0;                              /* empty title   */
   *p++ = 0;                              /* no creation data */

   return (int)DialogBoxIndirectParamW(
         GetModuleHandleW(NULL),
         dlg, parent, dlg_proc, 0);
}

void win32_resources_init(void)
{
   apply_dpi_awareness();
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
