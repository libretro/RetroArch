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

#ifndef _MENU_XUI_H_
#define _MENU_XUI_H_

#include <xui.h>
#include <xuiapp.h>

enum
{
   SETTING_EMU_REWIND_ENABLED = 0,
   SETTING_GAMMA_CORRECTION_ENABLED,
   SETTING_HARDWARE_FILTERING,
   SETTING_SCALE_ENABLED
};

class CRetroArch : public CXuiModule
{
public:
   HXUIOBJ		hMainScene;
   HXUIOBJ		hFileBrowser;
   HXUIOBJ		hCoreBrowser;
   HXUIOBJ		hQuickMenu;
   HXUIOBJ		hRetroArchSettings;
protected:
   /* Override so that Cssnes can register classes */
   virtual HRESULT RegisterXuiClasses();
   /* Override so that Cssnes can unregister classes */
   virtual HRESULT UnregisterXuiClasses();
};

class CRetroArchMain: public CXuiSceneImpl
{
protected:
   CXuiControl m_filebrowser;
   CXuiControl m_quick_menu;
   CXuiControl m_controls;
   CXuiControl m_settings;
   CXuiControl m_change_libretro_core;
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

   XUI_IMPLEMENT_CLASS(CRetroArchMain, L"RetroArchMain", XUI_CLASS_SCENE)
};

class CRetroArchFileBrowser: public CXuiSceneImpl
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

   XUI_IMPLEMENT_CLASS(CRetroArchFileBrowser, L"RetroArchFileBrowser", XUI_CLASS_SCENE)
};

class CRetroArchCoreBrowser: public CXuiSceneImpl
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

   XUI_IMPLEMENT_CLASS(CRetroArchCoreBrowser, L"RetroArchCoreBrowser", XUI_CLASS_SCENE)
};

class CRetroArchQuickMenu: public CXuiSceneImpl
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

   XUI_IMPLEMENT_CLASS(CRetroArchQuickMenu, L"RetroArchQuickMenu", XUI_CLASS_SCENE)
};

class CRetroArchSettings: public CXuiSceneImpl
{
protected:
   CXuiList m_settingslist;
   CXuiControl m_back;
public:
   HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
   HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

   XUI_BEGIN_MSG_MAP()
      XUI_ON_XM_INIT( OnInit)
      XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
   XUI_END_MSG_MAP();

   XUI_IMPLEMENT_CLASS(CRetroArchSettings, L"RetroArchSettings", XUI_CLASS_SCENE)
};

int menu_init (void);
void menu_deinit (void);
void menu_loop (void);

extern CRetroArch app;

#endif
