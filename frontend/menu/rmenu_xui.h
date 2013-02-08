/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef _RMENU_XUI_H_
#define _RMENU_XUI_H_

#include <xui.h>
#include <xuiapp.h>

enum
{
   SETTING_EMU_REWIND_ENABLED = 0,
   SETTING_EMU_REWIND_GRANULARITY,
   SETTING_EMU_SHOW_INFO_MSG,
   SETTING_EMU_SHOW_DEBUG_INFO_MSG,
   SETTING_AUDIO_RESAMPLER_TYPE,
   SETTING_GAMMA_CORRECTION_ENABLED,
   SETTING_SHADER,
   SETTING_SHADER_2,
   SETTING_HW_TEXTURE_FILTER,
   SETTING_HW_TEXTURE_FILTER_2,
   SETTING_SCALE_ENABLED,
   SETTING_SCALE_FACTOR,
   SETTING_ENABLE_SRAM_PATH,
   SETTING_ENABLE_STATE_PATH,
};

enum
{
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B = 0,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3,
   SETTING_CONTROLS_DPAD_EMULATION,
   SETTING_CONTROLS_DEFAULT_ALL
};

enum
{
   INPUT_LOOP_NONE = 0,
   INPUT_LOOP_MENU,
   INPUT_LOOP_RESIZE_MODE,
   INPUT_LOOP_FILEBROWSER
};

class CRetroArch : public CXuiModule
{
   public:
      HXUIOBJ hMainScene;
      HXUIOBJ hControlsMenu;
      HXUIOBJ hFileBrowser;
      HXUIOBJ hCoreBrowser;
      HXUIOBJ hShaderBrowser;
      HXUIOBJ hQuickMenu;
      HXUIOBJ hRetroArchSettings;
   protected:
      virtual HRESULT RegisterXuiClasses();
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
      CXuiControl m_logoimage;
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
      CXuiControl m_back;
      CXuiControl m_dir_game;
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
      CXuiControl m_back;
   public:
      HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
      HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

      XUI_BEGIN_MSG_MAP()
         XUI_ON_XM_INIT( OnInit)
         XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
         XUI_END_MSG_MAP();

      XUI_IMPLEMENT_CLASS(CRetroArchCoreBrowser, L"RetroArchCoreBrowser", XUI_CLASS_SCENE)
};

class CRetroArchShaderBrowser: public CXuiSceneImpl
{
   protected:
      CXuiControl m_back;
   public:
      HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
      HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );

      XUI_BEGIN_MSG_MAP()
         XUI_ON_XM_INIT( OnInit)
         XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
         XUI_END_MSG_MAP();

      XUI_IMPLEMENT_CLASS(CRetroArchShaderBrowser, L"RetroArchShaderBrowser", XUI_CLASS_SCENE)
};

class CRetroArchQuickMenu: public CXuiSceneImpl
{
   protected:
      CXuiList m_quickmenulist;
      CXuiControl m_back;
   public:
      HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
      HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );
      HRESULT OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled);

      XUI_BEGIN_MSG_MAP()
         XUI_ON_XM_INIT( OnInit)
         XUI_ON_XM_CONTROL_NAVIGATE( OnControlNavigate )
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
      HRESULT OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled);

      XUI_BEGIN_MSG_MAP()
         XUI_ON_XM_INIT( OnInit)
         XUI_ON_XM_CONTROL_NAVIGATE( OnControlNavigate )
         XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
         XUI_END_MSG_MAP();

      XUI_IMPLEMENT_CLASS(CRetroArchSettings, L"RetroArchSettings", XUI_CLASS_SCENE)
};

class CRetroArchControls: public CXuiSceneImpl
{
   protected:
      CXuiList m_controlslist;
      CXuiControl m_back;
      CXuiSlider m_controlnoslider;
   public:
      HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled );
      HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled );
      HRESULT OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled);

      XUI_BEGIN_MSG_MAP()
         XUI_ON_XM_INIT( OnInit)
         XUI_ON_XM_CONTROL_NAVIGATE( OnControlNavigate )
         XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
         XUI_END_MSG_MAP();

      XUI_IMPLEMENT_CLASS(CRetroArchControls, L"RetroArchControls", XUI_CLASS_SCENE)
};

#endif
