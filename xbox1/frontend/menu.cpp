/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "RetroLaunch/IoSupport.h"
#include "RetroLaunch/MenuManager.h"

#include "../../console/fileio/file_browser.h"

filebrowser_t browser;

int menu_init(void)
{
   RARCH_LOG("Starting RetroLaunch.\n");

   // Set file cache size
   XSetFileCacheSize(8 * 1024 * 1024);

   // Mount drives
   g_IOSupport.Mount("A:", "cdrom0");
   g_IOSupport.Mount("E:", "Harddisk0\\Partition1");
   g_IOSupport.Mount("Z:", "Harddisk0\\Partition2");
   g_IOSupport.Mount("F:", "Harddisk0\\Partition6");
   g_IOSupport.Mount("G:", "Harddisk0\\Partition7");

	strlcpy(browser.extensions, rarch_console_get_rom_ext(), sizeof(browser.extensions));
   filebrowser_set_root(&browser, g_console.default_rom_startup_dir);
   filebrowser_iterate(&browser, FILEBROWSER_ACTION_RESET);

   // Build menu here (Menu state -> Main Menu)
   g_menuManager.Create();

   g_console.mode_switch = MODE_MENU;

   return 0;
}

void menu_free(void)
{
   filebrowser_free(&browser);
}

void menu_loop(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   g_console.menu_enable = true;

   do
   {
      d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         D3DCOLOR_ARGB(0, 0, 0, 0),
         1.0f, 0);
      
      d3d->d3d_render_device->BeginScene();
      d3d->d3d_render_device->SetFlickerFilter(1);
      d3d->d3d_render_device->SetSoftDisplayFilter(1);

      //g_input.GetInput();
      g_menuManager.Update();
      
      d3d->d3d_render_device->EndScene();
      d3d->d3d_render_device->Present(NULL, NULL, NULL, NULL);
   }while(g_console.menu_enable);

   g_console.ingame_menu_enable = false;
}
