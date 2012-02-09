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

#include <stdint.h>
#include <crtdefs.h>
#include <tchar.h>
#include <xtl.h>
#include "file_browser.h"
#include "../console/rom_ext.h"
#include "xdk360_video.h"
#include "menu.h"
#include "shared.h"

#include "../general.h"

CSSNES		app;
filebrowser_t browser;
char		strbuffer[1024];

/* Register custom classes */
HRESULT CSSNES::RegisterXuiClasses (void)
{
	CSSNESMain::Register();
	CSSNESFileBrowser::Register();
	CSSNESSettings::Register();
	return S_OK;
}

/* Unregister custom classes */
HRESULT CSSNES::UnregisterXuiClasses (void)
{
	CSSNESMain::Unregister();
	CSSNESFileBrowser::Unregister();
	CSSNESSettings::Unregister();
	return S_OK;
}

static void filebrowser_fetch_directory_entries(const char *path, CXuiList * romlist, 
	CXuiTextElement * rompath_title)
{
	filebrowser_parse_directory(&browser, path, ssnes_console_get_rom_ext());

	DWORD dwNum_rompath = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	wchar_t * rompath_name = new wchar_t[dwNum_rompath];
	MultiByteToWideChar(CP_ACP, 0, path, -1, rompath_name, dwNum_rompath);
	rompath_title->SetText(rompath_name);

	romlist->DeleteItems(0, romlist->GetItemCount());
	romlist->InsertItems(0, browser.file_count);
	for(unsigned i = 0; i < browser.file_count; i++)
	{
		DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, browser.cur[i].d_name, -1, NULL, 0);
		wchar_t * entry_name = new wchar_t[dwNum];
		MultiByteToWideChar(CP_ACP, 0, browser.cur[i].d_name, -1, entry_name, dwNum);
		romlist->SetText(i, entry_name);
		delete []entry_name;
	}
}

HRESULT CSSNESFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
	GetChildById(L"XuiRomList", &m_romlist);
	GetChildById(L"XuiBackButton1", &m_back);
	GetChildById(L"XuiTxtRomPath", &m_rompathtitle);

	filebrowser_fetch_directory_entries(g_console.default_rom_startup_dir, &m_romlist, &m_rompathtitle);

	return S_OK;
}

HRESULT CSSNESSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
	GetChildById(L"XuiBtnRewind", &m_rewind);
	GetChildById(L"XuiCheckbox1", &m_rewind_cb);
	GetChildById(L"XuiBackButton1", &m_back);

	m_rewind_cb.SetCheck(g_settings.rewind_enable);
	return S_OK;
}

HRESULT CSSNESMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
	GetChildById(L"XuiBtnRomBrowser", &m_filebrowser);
	GetChildById(L"XuiBtnSettings", &m_settings);
	GetChildById(L"XuiBtnQuit", &m_quit);
	GetChildById(L"XuiTxtTitle", &m_title);
	GetChildById(L"XuiTxtCoreText", &m_core);
	const char * core_text = snes_library_id();
	char package_version[32];
	sprintf(package_version, "SSNES %s", PACKAGE_VERSION);
	DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, core_text, -1, NULL, 0);
	DWORD dwNum_package = MultiByteToWideChar(CP_ACP, 0, package_version, -1, NULL, 0);
	wchar_t * core_text_utf = new wchar_t[dwNum];
	wchar_t * package_version_utf = new wchar_t[dwNum_package];
	MultiByteToWideChar(CP_ACP, 0, core_text, -1, core_text_utf, dwNum);
	MultiByteToWideChar(CP_ACP, 0, package_version, -1, package_version_utf, dwNum_package);
	m_core.SetText(core_text_utf);
	m_title.SetText(package_version_utf);
	delete []core_text_utf;
	delete []package_version_utf;

	return S_OK;
}

HRESULT CSSNESFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
	if(hObjPressed == m_romlist)
	{
		int index = m_romlist.GetCurSel();
		if(browser.cur[index].d_type != FILE_ATTRIBUTE_DIRECTORY)
		{
			memset(strbuffer, 0, sizeof(strbuffer));
			wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
			memset(g_console.rom_path, 0, sizeof(g_console.rom_path));
			sprintf(g_console.rom_path, "%s%s", g_console.default_rom_startup_dir, strbuffer);
			g_console.menu_enable = false;
			g_console.mode_switch = MODE_EMULATION;
			g_console.initialize_ssnes_enable = 1;
		}
		else if(browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
		{
			memset(strbuffer, 0, sizeof(strbuffer));
			wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
			char strbuf[512];
			snprintf(strbuf, sizeof(strbuf), "%s%s", g_console.default_rom_startup_dir, strbuffer);
			filebrowser_fetch_directory_entries(strbuf, &m_romlist, &m_rompathtitle);
		}
	}
	else if(hObjPressed == m_back)
	{	
		NavigateBack(app.hMainScene);
	}

	bHandled = TRUE;
	return S_OK;
}

HRESULT CSSNESSettings::OnNotifyPress( HXUIOBJ hObjPressed,  BOOL& bHandled )
{
	if ( hObjPressed == m_rewind)
	{
		g_settings.rewind_enable = !g_settings.rewind_enable;
		m_rewind_cb.SetCheck(g_settings.rewind_enable);
	}
	else if ( hObjPressed == m_back )
	{
		HRESULT hr = XuiSceneNavigateBack(app.hSSNESSettings, app.hMainScene, XUSER_INDEX_FOCUS);
		
		if (FAILED(hr))
		{
			SSNES_ERR("Failed to load scene.\n");
		}
		
		NavigateBack(app.hMainScene);
	}
	bHandled = TRUE;
	return S_OK;
}

HRESULT CSSNESMain::OnNotifyPress( HXUIOBJ hObjPressed,  BOOL& bHandled )
{
	HRESULT hr;

	if ( hObjPressed == m_filebrowser )
	{
		hr = XuiSceneCreate(L"file://game:/media/", L"ssnes_filebrowser.xur", NULL, &app.hFileBrowser);
		
		if (FAILED(hr))
		{
			SSNES_ERR("Failed to load scene.\n");
		}

		NavigateForward(app.hFileBrowser);
	}
	else if ( hObjPressed == m_settings )
	{
		hr = XuiSceneCreate(L"file://game:/media/", L"ssnes_settings.xur", NULL, &app.hSSNESSettings);
		
		if (FAILED(hr))
		{
			SSNES_ERR("Failed to load scene.\n");
		}

		NavigateForward(app.hSSNESSettings);
	}
	else if ( hObjPressed == m_quit )
	{
		g_console.menu_enable = false;
		g_console.mode_switch = MODE_EXIT;
		g_console.initialize_ssnes_enable = false;
	}

	bHandled = TRUE;
	return S_OK;
}

int menu_init (void)
{
	HRESULT hr;

	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	
	hr = app.InitShared(vid->xdk360_render_device, &vid->d3dpp, XuiPNGTextureLoader);

	if (FAILED(hr))
	{
		SSNES_ERR("Failed initializing XUI application.\n");
		return 1;
	}

	/* Register font */
	hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game:/media/ssnes.ttf" );
	if (FAILED(hr))
	{
		SSNES_ERR("Failed to register default typeface.\n");
		return 1;
	}

	hr = app.LoadSkin( L"file://game:/media/ssnes_scene_skin.xur");
	if (FAILED(hr))
	{
		SSNES_ERR("Failed to load skin.\n");
		return 1;
	}

	hr = XuiSceneCreate(L"file://game:/media/", L"ssnes_main.xur", NULL, &app.hMainScene);
	if (FAILED(hr))
	{
		SSNES_ERR("Failed to create scene 'ssnes_main.xur'.\n");
		return 1;
	}

	XuiSceneNavigateFirst(app.GetRootObj(), app.hMainScene, XUSER_INDEX_FOCUS);

	return 0;
}

void menu_loop(void)
{
	g_console.menu_enable = true;

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
	}while(g_console.menu_enable);
}
