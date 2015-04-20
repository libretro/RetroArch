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

#if !defined(_XBOX) && defined(_WIN32)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <windows.h>
#include <commdlg.h>
#include "../../retroarch.h"

#ifdef HAVE_OPENGL
#include "../drivers_wm/win32_shader_dlg.h"
#endif

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
                     strlcpy(global->fullpath, win32_file, sizeof(global->fullpath));
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

void win32_show_cursor(bool state)
{
#ifndef _XBOX
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
#endif
}

void win32_check_window(void)
{
#ifndef _XBOX
   MSG msg;

   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
#endif
}
