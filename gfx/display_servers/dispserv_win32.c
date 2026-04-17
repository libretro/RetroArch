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

#define WIN32_LEAN_AND_MEAN

/* VC6 needs objbase included before initguid, but nothing else does */
#include <objbase.h>
#include <initguid.h>
#include <windows.h>
#include <ntverp.h>


#ifndef COBJMACROS
#define COBJMACROS
#define COBJMACROS_DEFINED
#endif
/* We really just want shobjidl.h, but there's no way to detect its existence at compile time (especially with mingw). however shlobj happens to include it for us when it's supported, which is easier. */
#include <shlobj.h>
#ifdef COBJMACROS_DEFINED
#undef COBJMACROS
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 */
#endif

#include "../video_display_server.h"
#include "../common/win32_common.h"

#ifdef __ITaskbarList3_INTERFACE_DEFINED__
#define HAS_TASKBAR_EXT

/* MSVC really doesn't want CINTERFACE to be used
 * with shobjidl for some reason, but since we use C++ mode,
 * we need a workaround... so use the names of the
 * COBJMACROS functions instead. */
#if defined(__cplusplus) && !defined(CINTERFACE)
#define ITaskbarList3_HrInit(x) (x)->HrInit()
#define ITaskbarList3_Release(x) (x)->Release()
#define ITaskbarList3_SetProgressState(a, b, c) (a)->SetProgressState(b, c)
#define ITaskbarList3_SetProgressValue(a, b, c, d) (a)->SetProgressValue(b, c, d)
#endif

/*
   NOTE: When an application displays a window,
   its taskbar button is created by the system.
   When the button is in place, the taskbar sends a
   TaskbarButtonCreated message to the window.

   Its value is computed by calling RegisterWindowMessage(
   L("TaskbarButtonCreated")).
   That message must be received by your application before
   it calls any ITaskbarList3 method.
 */
#endif

enum dispserv_win32_flags
{
   DISPSERV_WIN32_FLAG_DECORATIONS = (1 << 0)
};

typedef struct
{
#ifdef HAS_TASKBAR_EXT
   ITaskbarList3 *taskbar_list;
#endif
   int crt_center;
   unsigned orig_width;
   unsigned orig_height;
   unsigned orig_refresh;
   uint8_t flags;
} dispserv_win32_t;

/* Display configuration structs for QueryDisplayConfig */
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

static void *win32_display_server_init(void)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)calloc(1, sizeof(*dispserv));

   if (!dispserv)
      return NULL;

#ifdef HAS_TASKBAR_EXT
#ifdef __cplusplus
   /* When compiling in C++ mode, GUIDs
      are references instead of pointers */
   if (FAILED(CoCreateInstance(CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
         (void**)&dispserv->taskbar_list)))
#else
   /* Mingw GUIDs are pointers
      instead of references since we're in C mode */
   if (FAILED(CoCreateInstance(&CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, &IID_ITaskbarList3,
         (void**)&dispserv->taskbar_list)))
#endif
   {
      dispserv->taskbar_list = NULL;
   }
   else
   {
      if (FAILED(ITaskbarList3_HrInit(dispserv->taskbar_list)))
         dispserv->taskbar_list = NULL;
   }
#endif

   return dispserv;
}

static void win32_display_server_destroy(void *data)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)data;

   if (!dispserv)
      return;

   if (     dispserv->orig_width > 0
         && dispserv->orig_height > 0
         && dispserv->orig_refresh > 0)
      video_display_server_set_resolution(
            dispserv->orig_width,
            dispserv->orig_height,
            dispserv->orig_refresh,
            (float)dispserv->orig_refresh,
            dispserv->crt_center, 0, 0, 0);

#ifdef HAS_TASKBAR_EXT
   if (dispserv->taskbar_list)
   {
      ITaskbarList3_Release(dispserv->taskbar_list);
      dispserv->taskbar_list = NULL;
   }
#endif

   free(dispserv);
}

static bool win32_display_server_set_window_opacity(
      void *data, unsigned opacity)
{
#ifdef HAVE_WINDOW_TRANSP
   HWND     hwnd  = win32_get_window();
   LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
   /* Set window transparency on Windows 2000 and above */
   if (opacity < 100)
   {
      SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
      return SetLayeredWindowAttributes(hwnd, 0, (255 * opacity) / 100,
            LWA_ALPHA);
   }

   SetWindowLongPtr(hwnd, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
   return true;
#else
   return false;
#endif
}

static bool win32_display_server_set_window_progress(
      void *data, int progress, bool finished)
{
   uint8_t win32_flags    = 0;
   HWND              hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (!serv)
      return false;

#ifdef HAS_TASKBAR_EXT
   win32_flags            = win32_get_flags();
   if (!serv->taskbar_list || !(win32_flags & WIN32_CMN_FLAG_TASKBAR_CREATED))
      return false;

   if (progress == -1)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_INDETERMINATE) != S_OK)
         return false;
   }
   else if (finished)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_NOPROGRESS) != S_OK)
         return false;
   }
   else if (progress >= 0)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_NORMAL) != S_OK)
         return false;

      if (ITaskbarList3_SetProgressValue(
            serv->taskbar_list, hwnd, progress, 100) != S_OK)
         return false;
   }
#endif

   return true;
}

static bool win32_display_server_set_window_decorations(void *data, bool on)
{
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (!serv)
      return false;

   serv->flags      |= DISPSERV_WIN32_FLAG_DECORATIONS;

   /* menu_setting performs a reinit instead to properly
    * apply decoration changes */

   return true;
}

static bool win32_get_video_output(DEVMODE *dm, int mode)
{
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use        = NULL;
   unsigned mon_id           = 0;

   memset(dm, 0, sizeof(DEVMODE));
   dm->dmSize = sizeof(DEVMODE);

   win32_monitor_info(&current_mon, &hm_to_use, &mon_id);

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* Windows 2K and up*/
   return EnumDisplaySettingsEx(
         current_mon.szDevice,
         (mode == -1) ? ENUM_CURRENT_SETTINGS : (DWORD)mode,
         dm, EDS_ROTATEDMODE) != 0;
#else
   return EnumDisplaySettings(
         current_mon.szDevice,
         (mode == -1) ? ENUM_CURRENT_SETTINGS : (DWORD)mode,
         dm) != 0;
#endif
}

static bool win32_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust)
{
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use        = NULL;
   unsigned mon_id           = 0;
   DEVMODE dm                = {0};
   LONG res                  = 0;
   unsigned i                = 0;
   unsigned curr_bpp         = 0;
#if _WIN32_WINNT >= 0x0500
   unsigned curr_orientation = 0;
#endif
   dispserv_win32_t *serv    = (dispserv_win32_t*)data;

   if (!serv)
      return false;

   win32_get_video_output(&dm, -1);

   if (serv->orig_width == 0)
      serv->orig_width  = GetSystemMetrics(SM_CXSCREEN);
   if (serv->orig_height == 0)
      serv->orig_height = GetSystemMetrics(SM_CYSCREEN);
   if (serv->orig_refresh == 0)
      serv->orig_refresh = video_driver_get_refresh_rate();

   /* Used to stop super resolution bug */
   if (width == dm.dmPelsWidth)
      width = 0;
   if (width == 0)
      width = dm.dmPelsWidth;
   if (height == 0)
      height = dm.dmPelsHeight;
   if (curr_bpp == 0)
      curr_bpp = dm.dmBitsPerPel;
   if (int_hz == 0)
      int_hz = dm.dmDisplayFrequency;
#if _WIN32_WINNT >= 0x0500
   if (curr_orientation == 0)
      curr_orientation = dm.dmDisplayOrientation;
#endif

   for (i = 0; win32_get_video_output(&dm, i); i++)
   {
      if (dm.dmPelsWidth  != width)
         continue;
      if (dm.dmPelsHeight != height)
         continue;
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
      if (dm.dmDisplayFrequency != (DWORD)int_hz)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      dm.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT
                  | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
#if _WIN32_WINNT >= 0x0500
      dm.dmFields |= DM_DISPLAYORIENTATION;
#endif

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      res = win32_change_display_settings((const char*)&current_mon.szDevice, &dm, CDS_TEST);

      switch (res)
      {
         case DISP_CHANGE_SUCCESSFUL:
            res = win32_change_display_settings((const char*)&current_mon.szDevice, &dm, 0);
            switch (res)
            {
               case DISP_CHANGE_SUCCESSFUL:
               case DISP_CHANGE_NOTUPDATED:
                  return true;
               default:
                  break;
            }
            break;
         case DISP_CHANGE_RESTART:
            break;
         default:
            break;
      }
   }

   return true;
}

/* Display resolution list qsort helper function */
static int resolution_list_qsort_func(
      const video_display_config_t *a, const video_display_config_t *b)
{
   char str_a[64];
   char str_b[64];

   if (!a || !b)
      return 0;

   str_a[0] = str_b[0] = '\0';

   snprintf(str_a, sizeof(str_a), "%04dx%04d (%d Hz)",
         a->width,
         a->height,
         a->refreshrate);

   snprintf(str_b, sizeof(str_b), "%04dx%04d (%d Hz)",
         b->width,
         b->height,
         b->refreshrate);

   return strcasecmp(str_a, str_b);
}

static void *win32_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   DEVMODE dm                        = {0};
   unsigned i                        = 0;
   unsigned j                        = 0;
   unsigned count                    = 0;
   unsigned curr_width               = 0;
   unsigned curr_height              = 0;
   unsigned curr_bpp                 = 0;
   unsigned curr_refreshrate         = 0;
#if _WIN32_WINNT >= 0x0500
   unsigned curr_orientation         = 0;
#endif
   bool curr_interlaced              = false;
   struct video_display_config *conf = NULL;

   if (win32_get_video_output(&dm, -1))
   {
      curr_width                     = dm.dmPelsWidth;
      curr_height                    = dm.dmPelsHeight;
      curr_bpp                       = dm.dmBitsPerPel;
      curr_refreshrate               = dm.dmDisplayFrequency;
#if _WIN32_WINNT >= 0x0500
      curr_orientation               = dm.dmDisplayOrientation;
      curr_interlaced                = (dm.dmDisplayFlags & DM_INTERLACED) ? true : false;
#endif
   }

   for (i = 0; win32_get_video_output(&dm, i); i++)
   {
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      count++;
   }

   *len = count;
   if (!(conf = (struct video_display_config*)
      calloc(*len, sizeof(struct video_display_config))))
      return NULL;

   for (i = 0, j = 0; win32_get_video_output(&dm, i); i++)
   {
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      conf[j].width       = dm.dmPelsWidth;
      conf[j].height      = dm.dmPelsHeight;
      conf[j].bpp         = dm.dmBitsPerPel;
      conf[j].refreshrate = dm.dmDisplayFrequency;
      /* It may be possible to get exact refresh rate via different API - for now, it is integer only */
      conf[j].refreshrate_float = 0.0f;
      conf[j].idx         = j;
      conf[j].current     = false;
#if _WIN32_WINNT >= 0x0500
      conf[j].interlaced  = (dm.dmDisplayFlags & DM_INTERLACED) ? true : false;
#else
      conf[j].interlaced  = false;
#endif
      conf[j].dblscan     = false; /* no flag for doublescan on this platform */

      if (     (conf[j].width       == curr_width)
            && (conf[j].height      == curr_height)
            && (conf[j].bpp         == curr_bpp)
            && (conf[j].refreshrate == curr_refreshrate)
            && (conf[j].interlaced  == curr_interlaced)
         )
         conf[j].current  = true;

      j++;
   }

   qsort(
         conf, count,
         sizeof(video_display_config_t),
         (int (*)(const void *, const void *))
               resolution_list_qsort_func);

   return conf;
}

#if _WIN32_WINNT >= 0x0500
static enum rotation win32_display_server_get_screen_orientation(void *data)
{
   DEVMODE dm = {0};
   win32_get_video_output(&dm, -1);

   switch (dm.dmDisplayOrientation)
   {
      case DMDO_90:
         return ORIENTATION_FLIPPED_ROTATED;
      case DMDO_180:
         return ORIENTATION_FLIPPED;
      case DMDO_270:
         return ORIENTATION_VERTICAL;
      case DMDO_DEFAULT:
      default:
         break;
   }

   return ORIENTATION_NORMAL;
}

static void win32_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   DEVMODE dm = {0};

   win32_get_video_output(&dm, -1);

   switch (rotation)
   {
      case ORIENTATION_NORMAL:
      default:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_90
                     || dm.dmDisplayOrientation == DMDO_270)
                     && (width != (int)dm.dmPelsHeight))
            {
               /* Device is changing orientations, swap the aspect */
               dm.dmPelsWidth  = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_DEFAULT;
            break;
         }
      case ORIENTATION_VERTICAL:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_DEFAULT
                     || dm.dmDisplayOrientation == DMDO_180)
                     && (width != (int)dm.dmPelsHeight))
            {
               /* Device is changing orientations, swap the aspect */
               dm.dmPelsWidth  = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_270;
            break;
         }
      case ORIENTATION_FLIPPED:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_90
                     || dm.dmDisplayOrientation == DMDO_270)
                     && (width != (int)dm.dmPelsHeight))
            {
               /* Device is changing orientations, swap the aspect */
               dm.dmPelsWidth  = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_180;
            break;
         }
      case ORIENTATION_FLIPPED_ROTATED:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_DEFAULT
                     || dm.dmDisplayOrientation == DMDO_180)
                     && (width != (int)dm.dmPelsHeight))
            {
               /* Device is changing orientations, swap the aspect */
               dm.dmPelsWidth  = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_90;
            break;
         }
   }

   win32_change_display_settings(NULL, &dm, 0);
}
#endif

static float win32_display_server_get_refresh_rate(void *data)
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

static void win32_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   DEVMODE dm;
   if (win32_get_video_output(&dm, -1))
   {
      *width  = dm.dmPelsWidth;
      *height = dm.dmPelsHeight;
   }
}

static void win32_display_server_get_video_output_prev(void *data)
{
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use        = NULL;
   unsigned mon_id            = 0;
   unsigned i;
   DEVMODE dm;
   DEVMODE prev_dm;
   bool have_prev             = false;
   unsigned curr_width        = 0;
   unsigned curr_height       = 0;

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
         if (have_prev)
            break;
      }
      else
      {
         prev_dm   = dm;
         have_prev = true;
      }
   }

   if (have_prev)
   {
      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      win32_change_display_settings(
            (const char*)&current_mon.szDevice, &prev_dm, 0);
   }
}

static void win32_display_server_get_video_output_next(void *data)
{
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use        = NULL;
   unsigned mon_id            = 0;
   int i;
   DEVMODE dm;
   bool found                 = false;
   unsigned curr_width        = 0;
   unsigned curr_height       = 0;

   if (win32_get_video_output(&dm, -1))
   {
      curr_width  = dm.dmPelsWidth;
      curr_height = dm.dmPelsHeight;
   }

   for (i = 0; win32_get_video_output(&dm, i); i++)
   {
      if (found)
      {
         if (     dm.dmPelsWidth  != curr_width
               || dm.dmPelsHeight != curr_height)
         {
            win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
            win32_change_display_settings(
                  (const char*)&current_mon.szDevice, &dm, 0);
            break;
         }
      }

      if (     dm.dmPelsWidth  == curr_width
            && dm.dmPelsHeight == curr_height)
         found = true;
   }
}

static bool win32_display_server_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   HDC monitor;

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
         return false;
   }

   ReleaseDC(NULL, monitor);
   return true;
}

static uint32_t win32_display_server_get_flags(void *data)
{
   uint32_t flags   = 0;

   BIT32_SET(flags, DISPSERV_CTX_CRT_SWITCHRES);

   return flags;
}

const video_display_server_t dispserv_win32 = {
   win32_display_server_init,
   win32_display_server_destroy,
   win32_display_server_set_window_opacity,
   win32_display_server_set_window_progress,
   win32_display_server_set_window_decorations,
   win32_display_server_set_resolution,
   win32_display_server_get_resolution_list,
   NULL, /* get_output_options */
#if _WIN32_WINNT >= 0x0500
   win32_display_server_set_screen_orientation,
   win32_display_server_get_screen_orientation,
#else
   NULL,
   NULL,
#endif
   win32_display_server_get_refresh_rate,
   win32_display_server_get_video_output_size,
   win32_display_server_get_video_output_prev,
   win32_display_server_get_video_output_next,
   win32_display_server_get_metrics,
   win32_display_server_get_flags,
   "win32"
};
