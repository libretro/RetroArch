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
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <crtdefs.h>
#include <tchar.h>
#include <xtl.h>
#include "../console/fileio/file_browser.h"
#include "../console/console_ext.h"
#include "xdk360_video.h"
#include "menu.h"
#include "shared.h"

#include "../general.h"

CRetroArch        app;
filebrowser_t browser;
filebrowser_t tmp_browser;
char          strbuffer[1024];

static void return_to_game (void)
{
   g_console.frame_advance_enable = false;
   g_console.menu_enable = false;
   g_console.mode_switch = MODE_EMULATION;
}

static void return_to_dashboard (void)
{
   g_console.menu_enable = false;
   g_console.mode_switch = MODE_EXIT;
   g_console.initialize_rarch_enable = false;
}

/* Register custom classes */
HRESULT CSSNES::RegisterXuiClasses (void)
{
   CSSNESMain::Register();
   CSSNESFileBrowser::Register();
   CSSNESCoreBrowser::Register();
   CSSNESQuickMenu::Register();
   CSSNESSettings::Register();

   return S_OK;
}

/* Unregister custom classes */
HRESULT CSSNES::UnregisterXuiClasses (void)
{
   CSSNESMain::Unregister();
   CSSNESCoreBrowser::Unregister();
   CSSNESFileBrowser::Unregister();
   CSSNESQuickMenu::Register();
   CSSNESSettings::Unregister();

   return S_OK;
}

static void filebrowser_fetch_directory_entries(const char *path, filebrowser_t * browser, CXuiList * romlist, 
   CXuiTextElement * rompath_title)
{
   filebrowser_push_directory(browser, path, true);

   unsigned long dwNum_rompath = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
   wchar_t * rompath_name = new wchar_t[dwNum_rompath];
   MultiByteToWideChar(CP_ACP, 0, path, -1, rompath_name, dwNum_rompath);
   rompath_title->SetText(rompath_name);

   romlist->DeleteItems(0, romlist->GetItemCount());
   romlist->InsertItems(0, browser->file_count);
   for(unsigned i = 0; i < browser->file_count; i++)
   {
      unsigned long dwNum = MultiByteToWideChar(CP_ACP, 0, browser->cur[i].d_name, -1, NULL, 0);
      wchar_t * entry_name = new wchar_t[dwNum];
      MultiByteToWideChar(CP_ACP, 0, browser->cur[i].d_name, -1, entry_name, dwNum);
      romlist->SetText(i, entry_name);
      delete []entry_name;
   }
}

HRESULT CSSNESFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);
   GetChildById(L"XuiBtnGameDir", &m_dir_game);
   GetChildById(L"XuiBtnCacheDir", &m_dir_cache);

   filebrowser_fetch_directory_entries(g_console.default_rom_startup_dir, &browser, &m_romlist, &m_rompathtitle);

   return S_OK;
}

HRESULT CSSNESCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);

   filebrowser_new(&tmp_browser, "game:", "xex|XEX");
   filebrowser_fetch_directory_entries("game:", &tmp_browser, &m_romlist, &m_rompathtitle);

   return S_OK;
}

static const wchar_t * set_filter_element(int index)
{
   switch(index)
   {
      case FALSE:
         return L"Hardware filtering: Point filtering";
      case TRUE:
	 return L"Hardware filtering: Linear interpolation";
   }

   return L"";
}

HRESULT CSSNESSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiSettingsList", &m_settingslist);
   GetChildById(L"XuiBackButton", &m_back);

   m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
   m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
   m_settingslist.SetText(SETTING_HARDWARE_FILTERING, set_filter_element(g_settings.video.smooth));

   return S_OK;
}

HRESULT CSSNESQuickMenu::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiQuickMenuList", &m_quickmenulist);
   GetChildById(L"XuiBackButton", &m_back);

   m_quickmenulist.SetText(MENU_ITEM_HARDWARE_FILTERING, set_filter_element(g_settings.video.smooth));
   switch(g_console.screen_orientation)
   {
      case ORIENTATION_NORMAL:
         m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Normal");
	 break;
      case ORIENTATION_VERTICAL:
	 m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Vertical");
	 break;
      case ORIENTATION_FLIPPED:
	 m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Flipped");
	 break;
      case ORIENTATION_FLIPPED_ROTATED:
	 m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Flipped Rotated");
	 break;
   }
   char aspectratio_label[32];
   sprintf(aspectratio_label, "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);
   unsigned long dwNum = MultiByteToWideChar(CP_ACP, 0, aspectratio_label, -1, NULL, 0);
   wchar_t * aspectratio_label_w = new wchar_t[dwNum];
   MultiByteToWideChar(CP_ACP, 0, aspectratio_label, -1, aspectratio_label_w, dwNum);
   m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, aspectratio_label_w);
   delete[] aspectratio_label_w;

   return S_OK;
}

HRESULT CSSNESQuickMenu::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   int current_index;

   if ( hObjPressed == m_quickmenulist)
   {
      current_index = m_quickmenulist.GetCurSel();

      switch(current_index)
      {
         case MENU_ITEM_LOAD_STATE:
            if (g_console.emulator_initialized)
	    {
               rarch_load_state();
	       return_to_game();
	    }
	    break;
	 case MENU_ITEM_SAVE_STATE:
	    if (g_console.emulator_initialized)
	    {
               rarch_save_state();
	       return_to_game();
	    }
	    break;
	 case MENU_ITEM_HARDWARE_FILTERING:
	    g_settings.video.smooth = !g_settings.video.smooth;
	    m_quickmenulist.SetText(MENU_ITEM_HARDWARE_FILTERING, set_filter_element(g_settings.video.smooth));
	    break;
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
	    {
               if(g_console.aspect_ratio_index < ASPECT_RATIO_END)
                  g_console.aspect_ratio_index++;
	       else
                  g_console.aspect_ratio_index = 0;

	       video_xdk360.set_aspect_ratio(NULL, g_console.aspect_ratio_index);
	       char aspectratio_label[32];
	       sprintf(aspectratio_label, "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);
	       unsigned long dwNum = MultiByteToWideChar(CP_ACP, 0, aspectratio_label, -1, NULL, 0);
	       wchar_t * aspectratio_label_w = new wchar_t[dwNum];
	       MultiByteToWideChar(CP_ACP, 0, aspectratio_label, -1, aspectratio_label_w, dwNum);
	       m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, aspectratio_label_w);
	       delete[] aspectratio_label_w;
	    }
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
	    break;
	 case MENU_ITEM_ORIENTATION:
	    switch(g_console.screen_orientation)
	    {
               case ORIENTATION_NORMAL:
                  g_console.screen_orientation = ORIENTATION_VERTICAL;
		  m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Vertical");
		  break;
	       case ORIENTATION_VERTICAL:
		  g_console.screen_orientation = ORIENTATION_FLIPPED;
		  m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Flipped");
		  break;
	       case ORIENTATION_FLIPPED:
		  g_console.screen_orientation = ORIENTATION_FLIPPED_ROTATED;
		  m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Flipped Rotated");
		  break;
	       case ORIENTATION_FLIPPED_ROTATED:
		  g_console.screen_orientation = ORIENTATION_NORMAL;
		  m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, L"Orientation: Normal");
		  break;
	    }
	    video_xdk360.set_rotation(NULL, g_console.screen_orientation);
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    break;
	 case MENU_ITEM_FRAME_ADVANCE:
	    if (g_console.emulator_initialized)
	    {
               g_console.frame_advance_enable = true;
	       g_console.menu_enable = false;
	       g_console.mode_switch = MODE_EMULATION;
	    }
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    break;
	 case MENU_ITEM_RESET:
	    if (g_console.emulator_initialized)
	    {
               return_to_game();
	       rarch_game_reset();
	    }
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if (g_console.emulator_initialized)
               return_to_game();
	    break;
	 case MENU_ITEM_RETURN_TO_DASHBOARD:
	    return_to_dashboard();
	    break;
      }
   }
	
   if ( hObjPressed == m_back )
      NavigateBack(app.hMainScene);

   bHandled = TRUE;

   return S_OK;
}

HRESULT CSSNESMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   struct retro_system_info info;
   retro_get_system_info(&info);
   const char *id = info.library_name ? info.library_name : "Unknown";

   GetChildById(L"XuiBtnRomBrowser", &m_filebrowser);
   GetChildById(L"XuiBtnSettings", &m_settings);
   GetChildById(L"XuiBtnQuickMenu", &m_quick_menu);
   GetChildById(L"XuiBtnControls", &m_controls);
   GetChildById(L"XuiBtnQuit", &m_quit);
   GetChildById(L"XuiTxtTitle", &m_title);
   GetChildById(L"XuiTxtCoreText", &m_core);
   GetChildById(L"XuiBtnLibsnesCore", &m_change_libsnes_core);

   char core_text[256];
   sprintf(core_text, "%s (v%s)", id, info.library_version);
   char package_version[32];
   sprintf(package_version, "RetroArch %s", PACKAGE_VERSION);
   unsigned long dwNum = MultiByteToWideChar(CP_ACP, 0, core_text, -1, NULL, 0);
   unsigned long dwNum_package = MultiByteToWideChar(CP_ACP, 0, package_version, -1, NULL, 0);
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
   char path[MAX_PATH_LENGTH];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      if(browser.cur[index].d_type != FILE_ATTRIBUTE_DIRECTORY)
      {
         struct retro_system_info info;
	 retro_get_system_info(&info);
	 bool block_zip_extract  = info.block_extract;
         memset(strbuffer, 0, sizeof(strbuffer));
	 wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
	 if((strstr(strbuffer, ".zip") || strstr(strbuffer, ".ZIP")) && !block_zip_extract)
	 {
            char path_tmp[1024];
	    sprintf(path_tmp, "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), strbuffer);
	    rarch_extract_zipfile(path_tmp);
	 }
	 else
	 {
            memset(g_console.rom_path, 0, sizeof(g_console.rom_path));
	    sprintf(g_console.rom_path, "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), strbuffer);
	    return_to_game();
	    g_console.initialize_rarch_enable = 1;
	 }
      }
      else if(browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
      {
         memset(strbuffer, 0, sizeof(strbuffer));
	 wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
	 snprintf(path, sizeof(path), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), strbuffer);
	 filebrowser_fetch_directory_entries(path, &browser, &m_romlist, &m_rompathtitle);
      }
   }
   else if (hObjPressed == m_dir_game)
   {
      filebrowser_new(&browser, g_console.default_rom_startup_dir, rarch_console_get_rom_ext());
      filebrowser_fetch_directory_entries(g_console.default_rom_startup_dir, &browser, &m_romlist, &m_rompathtitle);
   }
   else if (hObjPressed == m_dir_cache)
   {
      filebrowser_new(&browser, "cache:", rarch_console_get_rom_ext());
      filebrowser_fetch_directory_entries("cache:", &browser, &m_romlist, &m_rompathtitle);
   }
   else if(hObjPressed == m_back)
      NavigateBack(app.hMainScene);

   bHandled = TRUE;

   return S_OK;
}

HRESULT CSSNESCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[MAX_PATH_LENGTH];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      if(browser.cur[index].d_type != FILE_ATTRIBUTE_DIRECTORY)
      {
         memset(strbuffer, 0, sizeof(strbuffer));
	 wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
	 sprintf(g_console.launch_app_on_exit, "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
	 g_console.return_to_launcher = true;
	 g_console.menu_enable = false;
	 g_console.mode_switch = MODE_EXIT;
      }
      else if(tmp_browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
      {
         memset(strbuffer, 0, sizeof(strbuffer));
	 wcstombs(strbuffer, (const wchar_t *)m_romlist.GetText(index), sizeof(strbuffer));
	 snprintf(path, sizeof(path), "%s%s\\", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
	 filebrowser_fetch_directory_entries(path, &tmp_browser, &m_romlist, &m_rompathtitle);
      }
   }
   else if(hObjPressed == m_back)
	   NavigateBack(app.hMainScene);

   bHandled = TRUE;
   return S_OK;
}

HRESULT CSSNESSettings::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   int current_index;

   if ( hObjPressed == m_settingslist)
   {
      current_index = m_settingslist.GetCurSel();

      switch(current_index)
      {
         case SETTING_EMU_REWIND_ENABLED:
            g_settings.rewind_enable = !g_settings.rewind_enable;
	    m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
	    break;
	 case SETTING_GAMMA_CORRECTION_ENABLED:
	    g_console.gamma_correction_enable = !g_console.gamma_correction_enable;
	    m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
	    break;
	 case SETTING_HARDWARE_FILTERING:
	    g_settings.video.smooth = !g_settings.video.smooth;
	    m_settingslist.SetText(SETTING_HARDWARE_FILTERING, set_filter_element(g_settings.video.smooth));
	    break;
      }
   }
	
   if ( hObjPressed == m_back )
      NavigateBack(app.hMainScene);

   bHandled = TRUE;
   return S_OK;
}

HRESULT CSSNESMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

   bool hdmenus_allowed = vid->video_mode.fIsHiDef && (g_console.aspect_ratio_index >= ASPECT_RATIO_16_9);

   HRESULT hr;

   if ( hObjPressed == m_filebrowser )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_filebrowser.xur", NULL, &app.hFileBrowser);

      if (FAILED(hr))
      {
         RARCH_ERR("Failed to load scene.\n");
      }

      NavigateForward(app.hFileBrowser);
   }
   else if ( hObjPressed == m_quick_menu)
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_quickmenu.xur", NULL, &app.hQuickMenu);

      if (FAILED(hr))
         RARCH_ERR("Failed to load scene.\n");

      NavigateForward(app.hQuickMenu);
   }
   else if ( hObjPressed == m_change_libsnes_core )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_libsnescore_browser.xur", NULL, &app.hCoreBrowser);

      if (FAILED(hr))
      {
         RARCH_ERR("Failed to load scene.\n");
      }
      NavigateForward(app.hCoreBrowser);
   }
   else if ( hObjPressed == m_settings )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_settings.xur", NULL, &app.hSSNESSettings);

      if (FAILED(hr))
         RARCH_ERR("Failed to load scene.\n");

      NavigateForward(app.hSSNESSettings);
   }
   else if ( hObjPressed == m_quit )
      return_to_dashboard();

   bHandled = TRUE;
   return S_OK;
}

int menu_init (void)
{
   HRESULT hr;

   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

   hr = app.InitShared(vid->d3d_render_device, &vid->d3dpp, XuiPNGTextureLoader);

   if (FAILED(hr))
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      return 1;
   }

   /* Register font */
   hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game:/media/ssnes.ttf" );
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to register default typeface.\n");
      return 1;
   }

   hr = app.LoadSkin( L"file://game:/media/rarch_scene_skin.xur");
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to load skin.\n");
      return 1;
   }

   hr = XuiSceneCreate(L"file://game:/media/sd/", L"rarch_main.xur", NULL, &app.hMainScene);
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      return 1;
   }

   XuiSceneNavigateFirst(app.GetRootObj(), app.hMainScene, XUSER_INDEX_FOCUS);

   filebrowser_new(&browser, g_console.default_rom_startup_dir, rarch_console_get_rom_ext());

   return 0;
}

void menu_deinit (void)
{
	app.Uninit();
}

void menu_loop(void)
{
   g_console.menu_enable = true;

   HRESULT hr;
   xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

   if(g_console.emulator_initialized)
      video_xdk360.set_swap_block_state(NULL, true);

   do
   {
      g_frame_count++;
      if(g_console.emulator_initialized)
      {
         rarch_render_cached_frame();
      }
      else
         vid->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   D3DCOLOR_ARGB(255, 32, 32, 64), 1.0f, 0);

      XINPUT_STATE state;
      XInputGetState(0, &state);

      g_console.menu_enable = !((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) 
		      && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) && (g_console.emulator_initialized)
		      && IS_TIMER_EXPIRED());
      g_console.mode_switch = g_console.menu_enable ? MODE_MENU : MODE_EMULATION;

      app.RunFrame();			/* Update XUI */
      hr = app.Render();		/* Render XUI */
      hr = XuiTimersRun();	/* Update XUI timers */

      if(g_console.mode_switch == MODE_EMULATION && !g_console.frame_advance_enable)
      {
         SET_TIMER_EXPIRATION(30);
      }

      video_xdk360.swap(NULL);
   }while(g_console.menu_enable);

   if(g_console.emulator_initialized)
      video_xdk360.set_swap_block_state(NULL, false);

   g_console.ingame_menu_enable = false;
}
