/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xtl.h>
#include "xdk360_video.h"
#include "menu.h"

CSSNES		app;
HXUIOBJ		hMainScene;

/* Register custom classes */
HRESULT CSSNES::RegisterXuiClasses (void)
{
	CMyMainScene::Register();
	return S_OK;
}

/* Unregister custom classes */
HRESULT CSSNES::UnregisterXuiClasses (void)
{
	CMyMainScene::Unregister();
	return S_OK;
}

int menu_init (void)
{
	HRESULT hr;

	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	
	hr = app.InitShared(vid->xdk360_render_device, &vid->d3dpp, XuiPNGTextureLoader);

	if (FAILED(hr))
	{
		OutputDebugString("Failed initializing XUI application.\n");
		return 1;
	}

	/* Register font */
	hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game/media/ssnes.ttf" );
	if (FAILED(hr))
	{
		OutputDebugString("Failed to register default typeface.\n");
		return 1;
	}

	hr = app.LoadSkin( L"file://game:/media/ssnes_scene_skin.xur");
	if (FAILED(hr))
	{
		OutputDebugString("Failed to load skin.\n");
	}

	hr = app.LoadFirstScene( L"file://game:/media/ssnes_main.xur", NULL);
	if (FAILED(hr))
	{
		OutputDebugString("Failed to load first scene.\n");
	}

	return 0;
}

void menu_loop(void)
{
	HRESULT hr;
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

	do
	{
		vid->xdk360_render_device->Clear(0, NULL,
			D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER,
			D3DCOLOR_ARGB(255, 32, 32, 64), 1.0, 0);

		app.RunFrame();			/* Update XUI */
		hr = app.Render();		/* Render XUI */
		hr = XuiTimersRun();	/* Update XUI timers */

		/* Present the frame */
		vid->xdk360_render_device->Present(NULL, NULL, NULL, NULL);
		
	}while(1);
}