/**
 * RetroLaunch 2012
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. To contact the
 * authors: Surreal64 CE Team (http://www.emuxtras.net)
 */

#include "Global.h"
#include "Video.h"
#include "IniFile.h"
#include "IoSupport.h"
#include "Input.h"
#include "Debug.h"
#include "Font.h"
#include "MenuManager.h"
#include "RomList.h"

bool g_bExit = false;

void __cdecl main()
{
	g_debug.Print("Starting RetroLaunch\n");

	// Set file cache size
	XSetFileCacheSize(8 * 1024 * 1024);

	// Mount drives
	g_IOSupport.Mount("A:", "cdrom0");
	g_IOSupport.Mount("E:", "Harddisk0\\Partition1");
	g_IOSupport.Mount("Z:", "Harddisk0\\Partition2");
	g_IOSupport.Mount("F:", "Harddisk0\\Partition6");
	g_IOSupport.Mount("G:", "Harddisk0\\Partition7");


	// Initialize Direct3D
	if (!g_video.Create(NULL, false))
		return;

	// Parse ini file for settings
	g_iniFile.CheckForIniEntry();

	// Load the rom list if it isn't already loaded
	if (!g_romList.IsLoaded()) {
		g_romList.Load();
	}

	// Init input here
	g_input.Create();

	// Load the font here
	g_font.Create();

	// Build menu here (Menu state -> Main Menu)
	g_menuManager.Create();

	// Loop the app
	while (!g_bExit)
	{
		g_video.BeginRender();
		g_input.GetInput();
		g_menuManager.Update();
		g_video.EndRender();
	}
}
