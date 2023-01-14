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
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include "../../core_info.h"

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

/* TODO/FIXME - globals */
unsigned g_win32_resize_width       = 0;
unsigned g_win32_resize_height      = 0;
float g_win32_refresh_rate          = 0;
ui_window_win32_t main_window;

/* TODO/FIXME - static globals */
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
   win32_common_state_t 
      *g_win32           = (win32_common_state_t*)&win32_st;

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
            info->path, NULL, NULL, NULL, NULL, &content_info, NULL, NULL);
         return true;
      }
   }
   else
   {
      bool            okay = false;
      settings_t *settings = config_get_ptr();
      bool video_is_fs     = settings->bools.video_fullscreen;

      /* Fullscreen: Show mouse cursor for dialog */
      if (video_is_fs)
         video_driver_show_mouse();

      /* Pick one core that could be compatible, ew */
      if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PICKCORE),
         main_window.hwnd, pick_core_proc, (LPARAM)NULL) == IDOK)
      {
         task_push_load_content_with_current_core_from_companion_ui(
            NULL, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
         okay = true;
      }

      /* Fullscreen: Hide mouse cursor after dialog */
      if (video_is_fs)
         video_driver_hide_mouse();
      return okay;
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
      bool okay        = false;
      char *szFilename = NULL;
      wszFilename[0]   = L'\0';

      DragQueryFileW((HDROP)wparam, 0, wszFilename, sizeof(wszFilename));
      szFilename = utf16_to_utf8_string_alloc(wszFilename);
      okay = win32_load_content_from_gui(szFilename);
      if (szFilename)
         free(szFilename);
      return okay;
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
            abs(info.rcMonitor.right - info.rcMonitor.left),
            abs(info.rcMonitor.bottom - info.rcMonitor.top),
            SWP_NOMOVE);
}

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
       * path/name of any file the user may select.
       * FIXME: We should really handle the
       * error case when this isn't big enough. */
      char new_title[PATH_MAX];
      char new_file[32768];

      new_title[0] = '\0';
      new_file[0]  = '\0';

      if (!string_is_empty(title))
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = strdup(initial_dir);
      browser_state.path     = new_file;
      browser_state.window   = owner;

      result = browser->open(&browser_state);

      /* TODO/FIXME - this is weird - why is this called
       * after the browser->open call? Seems to have no effect
       * anymore here */
      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);

      free(browser_state.startdir);
   }

   return result;
}

static LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode            = wparam & 0xffff;

   switch (mode)
   {
      case ID_M_LOAD_CORE:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            settings_t *settings    = config_get_ptr();
            char    *title_cp       = NULL;
            size_t converted        = 0;
            const char *extensions  = "Libretro core (.dll)\0*.dll\0All Files\0*.*\0\0";
            const char *title       = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST);
            const char *initial_dir = settings->paths.directory_libretro;

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

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
            path_set(RARCH_PATH_CORE, win32_file);
            command_event(CMD_EVENT_LOAD_CORE, NULL);
         }
         break;
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            char *title_cp          = NULL;
            size_t converted        = 0;
            const char *extensions  = "All Files (*.*)\0*.*\0\0";
            const char *title       = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST);
            settings_t *settings    = config_get_ptr();
            const char *initial_dir = settings->paths.directory_menu_content;

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

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
            win32_load_content_from_gui(win32_file);
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

   if (window_save_positions)
   {
      uint32_t video_st_flags        = video_driver_get_st_flags();
      bool video_fullscreen          = settings->bools.video_fullscreen;

      if (     !video_fullscreen
            && !(video_st_flags & VIDEO_FLAG_FORCE_FULLSCREEN)
            && !(video_st_flags & VIDEO_FLAG_IS_SWITCHING_DISPLAY_MODE))
      {
         bool ui_menubar_enable = settings->bools.ui_menubar_enable;
         bool window_show_decor = settings->bools.video_window_show_decorations;
         settings->uints.window_position_x      = g_win32->pos_x;
         settings->uints.window_position_y      = g_win32->pos_y;
         settings->uints.window_position_width  = g_win32->pos_width;
         settings->uints.window_position_height = g_win32->pos_height;
         if (window_show_decor)
         {
            int border_thickness  = GetSystemMetrics(SM_CXSIZEFRAME);
            int title_bar_height  = GetSystemMetrics(SM_CYCAPTION);
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
      case WM_COMMAND:
         {
            settings_t *settings     = config_get_ptr();
            bool ui_menubar_enable   = settings ? settings->bools.ui_menubar_enable : false;
            if (ui_menubar_enable)
               win32_menu_loop(main_window.hwnd, wparam);
         }
         break;
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

            /* extended keys will map to dinput if the high bit is set */
            if ((lparam >> 24 & 0x1))
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
      case WM_COMMAND:
         ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
#ifdef HAVE_CLIP_WINDOW
      case WM_SETFOCUS:
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(true);
         break;
      case WM_KILLFOCUS:
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(false);
         break;
#endif
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
      case WM_COMMAND:
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
            unsigned gcs = lparam & (GCS_COMPSTR|GCS_RESULTSTR);
            if (gcs)
            {
               int i;
               wchar_t wstr[4]={0,};
               int len1 = ImmGetCompositionStringW(hIMC, gcs, wstr, 4);
               wstr[2]  = wstr[1];
               wstr[1]  = 0;
               if ((len1 <= 0) || (len1 > 4))
                  break;
               for (i = 0; i < len1; i = i + 2)
               {
                  size_t len2;
                  char *utf8   = utf16_to_utf8_string_alloc(wstr+i);
                  if (!utf8)
                     continue;
                  len2         = strlen(utf8) + 1;
                  if (len2 >= 1 && len2 <= 3)
                  {
                     if (len2 >= 2)
                        utf8[3] = (gcs) | (gcs >> 4);
                     input_keyboard_event(true, 1, *((int*)utf8), 0, RETRO_DEVICE_KEYBOARD);
                  }
                  free(utf8);
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

            /* extended keys will map to dinput if the high bit is set */
            if ((lparam >> 24 & 0x1))
               keysym |= 0x80;

            keycode = input_keymaps_translate_keysym_to_rk(keysym);
            switch (keycode)
            {
               /* L+R Shift handling done in dinput_poll */
               case RETROK_LSHIFT:
               case RETROK_RSHIFT:
                  return 0;
            }

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
      case WM_COMMAND:
         ret = wnd_proc_common(&quit, hwnd, message, wparam, lparam);
         if (quit)
            return ret;
#ifdef HAVE_TASKBAR
         if (g_win32->taskbar_message && message == g_win32->taskbar_message)
            g_win32_flags |= WIN32_CMN_FLAG_TASKBAR_CREATED;
#endif
         break;
#ifdef HAVE_CLIP_WINDOW
      case WM_SETFOCUS:
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(true);
         break;
      case WM_KILLFOCUS:
         if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
            win32_clip_window(false);
         break;
#endif
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
#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_wgl_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_wgl_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_wgl_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_wgl_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      g_win32_flags |= WIN32_CMN_FLAG_INITED;
      return 0;
   }

   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK wnd_proc_wgl_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_wgl_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }

   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_VULKAN

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_vk_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_vk_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }

   return wnd_proc_common_dinput_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_WINRAWINPUT
LRESULT CALLBACK wnd_proc_vk_winraw(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_vk_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }

   return wnd_proc_winraw_common_internal(hwnd, message, wparam, lparam);
}
#endif

LRESULT CALLBACK wnd_proc_vk_common(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;

   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_vk_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }

   return wnd_proc_common_internal(hwnd, message, wparam, lparam);
}
#endif

#ifdef HAVE_GDI

#ifdef HAVE_DINPUT
LRESULT CALLBACK wnd_proc_gdi_dinput(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   
   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_gdi_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }
   else if (message == WM_PAINT)
   {
      gdi_t *gdi = (gdi_t*)video_driver_get_ptr();

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
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   
   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_gdi_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }
   else if (message == WM_PAINT)
   {
      gdi_t *gdi = (gdi_t*)video_driver_get_ptr();

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
   win32_common_state_t *g_win32 = (win32_common_state_t*)&win32_st;
   
   if (message == WM_CREATE)
   {
      bool is_quit = false;
      create_gdi_context(hwnd, &is_quit);
      if (is_quit)
         g_win32_flags |= WIN32_CMN_FLAG_QUIT;
      if (DragAcceptFiles_func)
         DragAcceptFiles_func(hwnd, true);
      return 0;
   }
   else if (message == WM_PAINT)
   {
      gdi_t *gdi = (gdi_t*)video_driver_get_ptr();

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

   window_accelerators = LoadAcceleratorsA(GetModuleHandleA(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR1));

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
      RARCH_ERR("Error registering for notifications\n");
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
   switch (type)
   {
      case DISPLAY_METRIC_PIXEL_WIDTH:
         {
            HDC monitor        = GetDC(NULL);
            *value             = GetDeviceCaps(monitor, HORZRES);
            ReleaseDC(NULL, monitor);
         }
         return true;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         {
            HDC monitor        = GetDC(NULL);
            *value             = GetDeviceCaps(monitor, VERTRES);
            ReleaseDC(NULL, monitor);
         }
         return true;
      case DISPLAY_METRIC_MM_WIDTH:
         {
            HDC monitor        = GetDC(NULL);
            *value             = GetDeviceCaps(monitor, HORZSIZE);
            ReleaseDC(NULL, monitor);
         }
         return true;
      case DISPLAY_METRIC_MM_HEIGHT:
         {
            HDC monitor        = GetDC(NULL);
            *value             = GetDeviceCaps(monitor, VERTSIZE);
            ReleaseDC(NULL, monitor);
         }
         return true;
      case DISPLAY_METRIC_DPI:
         /* 25.4 mm in an inch. */
         {
            HDC monitor        = GetDC(NULL);
            int pixels_x       = GetDeviceCaps(monitor, HORZRES);
            int physical_width = GetDeviceCaps(monitor, HORZSIZE);
            *value = 254 * pixels_x / physical_width / 10;
            ReleaseDC(NULL, monitor);
         }
         return true;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         break;
   }
   return false;
}
#endif

void win32_monitor_init(void)
{
   win32_common_state_t 
      *g_win32            = (win32_common_state_t*)&win32_st;

#if !defined(_XBOX)
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
   win32_common_state_t 
      *g_win32            = (win32_common_state_t*)&win32_st;
   bool video_is_threaded = video_driver_is_threaded();
   if (video_is_threaded)
      ui_companion_win32.application->process_events();
   *quit                  = g_win32_flags & WIN32_CMN_FLAG_QUIT;

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
   RECT clip_rect;

   if (state && main_window.hwnd)
   {
      PWINDOWINFO info;
      info         = (PWINDOWINFO)malloc(sizeof(*info));

      if (info)
      {
         info->cbSize = sizeof(PWINDOWINFO);

         if (GetWindowInfo(main_window.hwnd, info))
            clip_rect = info->rcClient;

         free(info);
      }
      info = NULL;
   }
   else
      GetWindowRect(GetDesktopWindow(), &clip_rect);

   ClipCursor(&clip_rect);
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

/* Given a short key (meta key), get its name as a string */
/* For single character results, may return same pointer 
 * with different data inside (modifying the old result) */
static const char *meta_key_to_name(unsigned int meta_key)
{
   int i = 0;
   const struct retro_keybind* key = &input_config_binds[0][meta_key];
   int key_code                    = key->key;

   for (;;)
   {
      const struct input_key_map* entry = &input_config_key_map[i];
      if (!entry->str)
         break;
      if (entry->key == key_code)
         return entry->str;
      i++;
   }

   if (key_code >= 32 && key_code < 127)
   {
      static char single_char[2] = "A";
      single_char[0]              = key_code;
      return single_char;
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
      BOOL okay;
      enum msg_hash_enums label_enum;
      memset(&menu_item_info, 0, sizeof(menu_item_info));
      menu_item_info.cbSize     = sizeof(menu_item_info);
      menu_item_info.dwTypeData = NULL;
#if(WINVER >= 0x0500)
      menu_item_info.fMask      = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#else
      menu_item_info.fMask      =                            MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#endif

#ifndef LEGACY_WIN32
      okay                    = GetMenuItemInfoW(menu, index, true, &menu_item_info);
#else
      okay                    = GetMenuItemInfoA(menu, index, true, &menu_item_info);
#endif
      if (!okay)
         break;

      /* Recursion - call this on submenu items too */
      if (menu_item_info.hSubMenu)
         win32_localize_menu(menu_item_info.hSubMenu);

      label_enum = menu_id_to_label_enum(menu_item_info.wID);
      if (label_enum != MSG_UNKNOWN)
      {
         int len;
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

         /* specific replacements:
            Load Content = "Ctrl+O"
            Fullscreen = "Alt+Enter" */
         if (label_enum == 
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST)
            meta_key_name           = "Ctrl+O";
         else if (label_enum == 
               MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY)
            meta_key_name           = "Alt+Enter";
         else if (meta_key != 0)
            meta_key_name           = meta_key_to_name(meta_key);

         /* Append localized name, tab character, and Shortcut Key */
         if (meta_key_name && string_is_not_equal(meta_key_name, "nul"))
         {
            size_t len1     = strlen(new_label);
            size_t len2     = strlen(meta_key_name);
            size_t buf_size = len1 + len2 + 2;
            new_label_text  = (char*)malloc(buf_size);

            if (new_label_text)
            {
               size_t _len;
               new_label2              = new_label_text;
               _len                    = strlcpy(new_label_text, new_label,
                     buf_size);
               new_label_text[_len  ]  = '\t';
               new_label_text[_len+1]  = '\0';
               strlcat(new_label_text, meta_key_name, buf_size);
               /* Make first character of shortcut name uppercase */
               new_label_text[len1 + 1] = toupper(new_label_text[len1 + 1]);
            }
         }

#ifndef LEGACY_WIN32
         /* Convert string from UTF-8, then assign menu text */
         new_label_unicode         = utf8_to_utf16_string_alloc(new_label2);
         len                       = wcslen(new_label_unicode);
         menu_item_info.cch        = len;
         menu_item_info.dwTypeData = new_label_unicode;
         SetMenuItemInfoW(menu, index, true, &menu_item_info);
         free(new_label_unicode);
#else
         new_label_ansi            = utf8_to_local_string_alloc(new_label2);
         len                       = strlen(new_label_ansi);
         menu_item_info.cch        = len;
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
   if (g_win32_flags & WIN32_CMN_FLAG_INITED)
      if (GetForegroundWindow() == main_window.hwnd)
         return true;

   return false;
}

HWND win32_get_window(void) { return main_window.hwnd; }

bool win32_suppress_screensaver(void *data, bool enable)
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

   return false;
}

static bool win32_monitor_set_fullscreen(
      unsigned width, unsigned height,
      unsigned refresh, char *dev_name)
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
      float video_refresh    = settings->floats.video_refresh_rate;
      unsigned bfi           = settings->uints.video_black_frame_insertion;
      float refresh_mod      = bfi + 1.0f;
      float refresh_rate     = video_refresh * refresh_mod;

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
               (int)refresh_rate, current_mon->szDevice))
         {
            RARCH_LOG("[Video]: Fullscreen set to %ux%u @ %uHz on device %s.\n",
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
      bool window_show_decor = settings->bools.video_window_show_decorations;

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

         menuItem = LoadMenuA(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU));
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
         RARCH_ERR("GetMessage error code %d\n", GetLastError());
         break;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (g_win32_flags & WIN32_CMN_FLAG_QUIT)
      return false;
   return true;
}

void win32_update_title(void)
{
   const ui_window_t *window         = ui_companion_driver_get_window_ptr();
   if (window)
   {
      static char prev_title[128];
      char title[128];
      title[0] = '\0';
      video_driver_get_window_title(title, sizeof(title));
      if (title[0] && !string_is_equal(title, prev_title))
      {
         window->set_title(&main_window, title);
         strlcpy(prev_title, title, sizeof(prev_title));
      }
   }
}
#endif

bool win32_get_client_rect(RECT* rect)
{
   return GetWindowRect(main_window.hwnd, rect);
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
#ifdef HAVE_DYLIB
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

   if (pGetDisplayConfigBufferSizes(
            QDC_DATABASE_CURRENT,
            &NumPathArrayElements,
            &NumModeInfoArrayElements) != ERROR_SUCCESS)
      return refresh_rate;

   PathInfoArray = (DISPLAYCONFIG_PATH_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_PATH_INFO_CUSTOM) * NumPathArrayElements);
   ModeInfoArray = (DISPLAYCONFIG_MODE_INFO_CUSTOM *)
      malloc(sizeof(DISPLAYCONFIG_MODE_INFO_CUSTOM) * NumModeInfoArrayElements);

   if (pQueryDisplayConfig(QDC_DATABASE_CURRENT,
                               &NumPathArrayElements,
                               PathInfoArray,
                               &NumModeInfoArrayElements,
                               ModeInfoArray,
                               &TopologyID) == ERROR_SUCCESS
         && NumPathArrayElements >= 1)
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

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
#define WIN32_GET_VIDEO_OUTPUT(iModeNum, dm) EnumDisplaySettingsEx(NULL, iModeNum, dm, EDS_ROTATEDMODE)
#else
#define WIN32_GET_VIDEO_OUTPUT(iModeNum, dm) EnumDisplaySettings(NULL, iModeNum, dm)
#endif

bool win32_get_video_output(DEVMODE *dm, int mode, size_t len)
{
   memset(dm, 0, len);
   dm->dmSize  = len;
   if (WIN32_GET_VIDEO_OUTPUT((mode == -1) 
            ? ENUM_CURRENT_SETTINGS 
            : mode,
            dm) == 0)
      return false;
   return true;
}

void win32_get_video_output_size(unsigned *width, unsigned *height, char *desc, size_t desc_len)
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
#endif
