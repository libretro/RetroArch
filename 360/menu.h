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

#ifndef _MENU_XUI_H_
#define _MENU_XUI_H_

#include <xui.h>
#include <xuiapp.h>

class CSSNES : public CXuiModule
{
public:
	HXUIOBJ		hMainScene;
	HXUIOBJ		hFileBrowser;
	HXUIOBJ		hCoreBrowser;
	HXUIOBJ		hQuickMenu;
	HXUIOBJ		hSSNESSettings;
protected:
	/* Override so that Cssnes can register classes */
	virtual HRESULT RegisterXuiClasses();
	/* Override so that Cssnes can unregister classes */
	virtual HRESULT UnregisterXuiClasses();
};

class CSSNESMain: public CXuiSceneImpl
{
protected:
	CXuiControl m_filebrowser;
	CXuiControl m_quick_menu;
	CXuiControl m_controls;
	CXuiControl m_settings;
	CXuiControl m_change_libsnes_core;
	CXuiControl m_quit;
	CXuiTextElement m_title;
	CXuiTextElement m_core;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESMain, L"SSNESMain", XUI_CLASS_SCENE)
};

class CSSNESFileBrowser: public CXuiSceneImpl
{
protected:
	CXuiList m_romlist;
	CXuiControl m_back;
	CXuiControl m_dir_game;
	CXuiControl m_dir_cache;
	CXuiTextElement m_rompathtitle;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESFileBrowser, L"SSNESFileBrowser", XUI_CLASS_SCENE)
};

class CSSNESCoreBrowser: public CXuiSceneImpl
{
protected:
	CXuiList m_romlist;
	CXuiControl m_back;
	CXuiTextElement m_rompathtitle;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESCoreBrowser, L"SSNESCoreBrowser", XUI_CLASS_SCENE)
};

class CSSNESQuickMenu: public CXuiSceneImpl
{
protected:
	CXuiList m_quickmenulist;
	CXuiControl m_back;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESQuickMenu, L"SSNESQuickMenu", XUI_CLASS_SCENE)
};

class CSSNESSettings: public CXuiSceneImpl
{
protected:
	CXuiControl m_rewind;
	CXuiCheckbox m_rewind_cb;
	CXuiControl m_hw_filter;
	CXuiControl m_back;
public:
	HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit)
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
	XUI_END_MSG_MAP();

	XUI_IMPLEMENT_CLASS(CSSNESSettings, L"SSNESSettings", XUI_CLASS_TABSCENE)
};

int menu_init (void);
void menu_loop (void);

extern CSSNES app;

#endif
