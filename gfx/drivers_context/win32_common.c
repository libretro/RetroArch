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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include "../../driver.h"
#include "../gfx_common.h"
#include "win32_common.h"
#include <windows.h>
#include <commdlg.h>
#include <string.h>

#if !defined(_XBOX) && defined(_WIN32)
#include "../../retroarch.h"

static bool win32_browser(HWND owner, char *filename, const char *extensions,
	const char *title, const char *initial_dir)
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
    WPARAM mode      = wparam & 0xffff;
	unsigned cmd     = RARCH_CMD_NONE;
	bool do_wm_close = false;

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
					initial_dir = g_settings.libretro_directory;
				}
				else if (mode == ID_M_LOAD_CONTENT)
				{
					extensions  = "All Files\0*.*\0\0";
					title       = "Load Content";
					initial_dir = g_settings.menu_content_directory;
				}

				if (win32_browser(owner, win32_file, extensions, title, initial_dir))
				{
					switch (mode)
					{
					case ID_M_LOAD_CORE:
						strlcpy(g_settings.libretro, win32_file, sizeof(g_settings.libretro));
						cmd = RARCH_CMD_LOAD_CORE;
						break;
					case ID_M_LOAD_CONTENT:
						strlcpy(g_extern.fullpath, win32_file, sizeof(g_extern.fullpath));
						cmd = RARCH_CMD_LOAD_CONTENT;
						do_wm_close = true;
						break;
					}
				}
			}
			break;
		case ID_M_RESET:
			cmd = RARCH_CMD_RESET;
			break;
		case ID_M_MENU_TOGGLE:
			cmd = RARCH_CMD_MENU_TOGGLE;
			break;
		case ID_M_PAUSE_TOGGLE:
			cmd = RARCH_CMD_PAUSE_TOGGLE;
			break;
		case ID_M_LOAD_STATE:
			cmd = RARCH_CMD_LOAD_STATE;
			break;
		case ID_M_SAVE_STATE:
			cmd = RARCH_CMD_SAVE_STATE;
			break;
		case ID_M_DISK_CYCLE:
			cmd = RARCH_CMD_DISK_EJECT_TOGGLE;
			break;
		case ID_M_DISK_NEXT:
			cmd = RARCH_CMD_DISK_NEXT;
			break;
		case ID_M_DISK_PREV:
			cmd = RARCH_CMD_DISK_PREV;
			break;
		case ID_M_FULL_SCREEN:
			cmd = RARCH_CMD_FULLSCREEN_TOGGLE;
			break;
		case ID_M_MOUSE_GRAB:
			cmd = RARCH_CMD_GRAB_MOUSE_TOGGLE;
			break;
		case ID_M_TAKE_SCREENSHOT:
			cmd = RARCH_CMD_TAKE_SCREENSHOT;
			break;
		case ID_M_QUIT:
			do_wm_close = true;
			break;
	}

	if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
	{
		unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
		g_extern.pending.windowed_scale = idx;
		cmd = RARCH_CMD_RESIZE_WINDOWED_SCALE;
	}
	if (mode >= ID_M_STATE_INDEX_AUTO && mode <= (ID_M_STATE_INDEX_AUTO+10))
	{
		signed idx = (mode - (ID_M_STATE_INDEX_AUTO));
		g_settings.state_slot = idx;
	}

	if (cmd != RARCH_CMD_NONE)
		rarch_main_command(cmd);

	if (do_wm_close)
		PostMessage(owner, WM_CLOSE, 0, 0);
	
	return 0L;
}
#endif
