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

#include <stdint.h>
#include <crtdefs.h>
#include <tchar.h>
#include <xtl.h>
#include "../../console/fileio/file_browser.h"
#include "../../console/console_ext.h"
#include "../xdk360_video.h"
#include "menu.h"
#include "../../message.h"

#include "../../general.h"

CRetroArch app;
HXUIOBJ hCur;
filebrowser_t browser;
filebrowser_t tmp_browser;
uint32_t set_shader = 0;
wchar_t strw_buffer[PATH_MAX];

/* Register custom classes */
HRESULT CRetroArch::RegisterXuiClasses (void)
{
   CRetroArchMain::Register();
   CRetroArchFileBrowser::Register();
   CRetroArchCoreBrowser::Register();
   CRetroArchShaderBrowser::Register();
   CRetroArchQuickMenu::Register();
   CRetroArchControls::Register();
   CRetroArchSettings::Register();

   return 0;
}

/* Unregister custom classes */
HRESULT CRetroArch::UnregisterXuiClasses (void)
{
   CRetroArchMain::Unregister();
   CRetroArchCoreBrowser::Unregister();
   CRetroArchShaderBrowser::Unregister();
   CRetroArchFileBrowser::Unregister();
   CRetroArchQuickMenu::Register();
   CRetroArchControls::Register();
   CRetroArchSettings::Unregister();

   return 0;
}

static void filebrowser_fetch_directory_entries(const char *path, filebrowser_t * browser, CXuiList * romlist, 
   CXuiTextElement * rompath_title)
{
   filebrowser_push_directory(browser, path, true);

   rarch_convert_char_to_wchar(strw_buffer, path, sizeof(strw_buffer));
   rompath_title->SetText(strw_buffer);

   romlist->DeleteItems(0, romlist->GetItemCount());
   romlist->InsertItems(0, browser->current_dir.list->size);
   for(unsigned i = 0; i < browser->current_dir.list->size; i++)
   {
      char fname_tmp[256];
	  fill_pathname_base(fname_tmp, browser->current_dir.list->elems[i].data, sizeof(fname_tmp));
      rarch_convert_char_to_wchar(strw_buffer, fname_tmp, sizeof(strw_buffer));
      romlist->SetText(i, strw_buffer);
   }
}

HRESULT CRetroArchFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);
   GetChildById(L"XuiBtnGameDir", &m_dir_game);
   GetChildById(L"XuiBtnCacheDir", &m_dir_cache);

   filebrowser_set_root(&browser, g_console.default_rom_startup_dir);
   strlcpy(tmp_browser.extensions, rarch_console_get_rom_ext(), sizeof(tmp_browser.extensions));
   filebrowser_fetch_directory_entries(g_console.default_rom_startup_dir, &browser, &m_romlist, &m_rompathtitle);

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);

   filebrowser_set_root(&tmp_browser, "game:");
   strlcpy(tmp_browser.extensions, "xex|XEX", sizeof(tmp_browser.extensions));
   filebrowser_fetch_directory_entries("game:", &tmp_browser, &m_romlist, &m_rompathtitle);

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_shaderlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_shaderpathtitle);

   filebrowser_set_root(&tmp_browser, "game:\\media\\shaders");
   strlcpy(tmp_browser.extensions, "cg|CG", sizeof(tmp_browser.extensions));
   filebrowser_fetch_directory_entries("game:\\media\\shaders", &tmp_browser, &m_shaderlist, &m_shaderpathtitle);

   return 0;
}

HRESULT CRetroArchControls::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   unsigned i;
   int controlno;
   char buttons[RARCH_FIRST_META_KEY][128];

   GetChildById(L"XuiControlsList", &m_controlslist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiControlNoSlider", &m_controlnoslider);

   m_controlnoslider.SetValue(g_settings.input.currently_selected_controller_no);
   m_controlnoslider.GetValue(&controlno);

   for(i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_input_get_default_keybind_name(i), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      rarch_convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer)); 
      m_controlslist.SetText(i, strw_buffer);
   }
   
   return 0;
}

HRESULT CRetroArchControls::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   char button[128];
   char buttons[RARCH_FIRST_META_KEY][128];
   int controlno, i, current_index;
   
   current_index = m_controlslist.GetCurSel();
   m_controlnoslider.GetValue(&controlno);

   for(i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_input_get_default_keybind_name(i), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      rarch_convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer));
      m_controlslist.SetText(i, strw_buffer);
   }

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         if(current_index > 0 && current_index != SETTING_CONTROLS_DEFAULT_ALL)
         {
            rarch_input_set_keybind(controlno, KEYBIND_DECREMENT, current_index);
            snprintf(button, sizeof(button), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
            rarch_convert_char_to_wchar(strw_buffer, button, sizeof(strw_buffer));
            m_controlslist.SetText(current_index, strw_buffer);
         }
         break;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         if(current_index < RARCH_FIRST_META_KEY && current_index != SETTING_CONTROLS_DEFAULT_ALL)
         {
            rarch_input_set_keybind(controlno, KEYBIND_INCREMENT, current_index);
            snprintf(button, sizeof(button), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
            rarch_convert_char_to_wchar(strw_buffer, button, sizeof(strw_buffer));
            m_controlslist.SetText(current_index, strw_buffer);
         }
         break;
      case XUI_CONTROL_NAVIGATE_UP:
      case XUI_CONTROL_NAVIGATE_DOWN:
         break;
   }

	return 0;
}

HRESULT CRetroArchControls::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   int current_index, i, controlno;
   char buttons[RARCH_FIRST_META_KEY][128];
   m_controlnoslider.GetValue(&controlno);

   if ( hObjPressed == m_controlslist)
   {
      current_index = m_controlslist.GetCurSel();

      switch(current_index)
      {
         case SETTING_CONTROLS_DEFAULT_ALL:
            rarch_input_set_default_keybinds(0);

            for(i = 0; i < RARCH_FIRST_META_KEY; i++)
            {
               snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_input_get_default_keybind_name(i), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
               rarch_convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer));
               m_controlslist.SetText(i, strw_buffer);
            }
            break;
         default:
            rarch_input_set_keybind(controlno, KEYBIND_DEFAULT, current_index);
            snprintf(buttons[current_index], sizeof(buttons[current_index]), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
            rarch_convert_char_to_wchar(strw_buffer, buttons[current_index], sizeof(strw_buffer));
            m_controlslist.SetText(current_index, strw_buffer);
            break;
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiSettingsList", &m_settingslist);
   GetChildById(L"XuiBackButton", &m_back);

   m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
   m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_console.info_msg_enable ? L"Info messages: ON" : L"Info messages: OFF");
   m_settingslist.SetText(SETTING_EMU_MENUS, g_console.menus_hd_enable ? L"Menus: HD" : L"Menus: SD");
   m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Hardware filtering shader #1: Linear interpolation" : L"Hardware filtering shader #1: Point filtering");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER_2, g_settings.video.second_pass_smooth ? L"Hardware filtering shader #2: Linear interpolation" : L"Hardware filtering shader #2: Point filtering");
   m_settingslist.SetText(SETTING_SCALE_ENABLED, g_console.fbo_enabled ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SHADER, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER, strw_buffer);
   m_settingslist.SetText(SETTING_COLOR_FORMAT, g_console.color_format ? L"Color format: 32bit ARGB" : L"Color format: 16bit RGBA");
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SHADER_2, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER_2, strw_buffer);
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);

   return 0;
}

HRESULT CRetroArchSettings::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   int current_index;
   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;
   
   current_index = m_settingslist.GetCurSel();

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         switch(current_index)
         {
            case SETTING_SCALE_FACTOR:
               if(vid->fbo_enabled)
               {
                  if((g_settings.video.fbo_scale_x > MIN_SCALING_FACTOR))
                  {
                     rarch_settings_change(S_SCALE_FACTOR_DECREMENT);
                     //xdk360_gfx_init_fbo(vid);
                     rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                     m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                  }
               }
            default:
               break;
         }
         break;
    case XUI_CONTROL_NAVIGATE_RIGHT:
        switch(current_index)
        {
            case SETTING_SCALE_FACTOR:
                if(vid->fbo_enabled)
                {
                   if((g_settings.video.fbo_scale_x < MAX_SCALING_FACTOR))
                   {
                       rarch_settings_change(S_SCALE_FACTOR_INCREMENT);
                       //xdk360_gfx_init_fbo(vid);
                       rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                       m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                   }
                }
            default:
                break;
        }
        break;
    case XUI_CONTROL_NAVIGATE_UP:
    case XUI_CONTROL_NAVIGATE_DOWN:
        break;
   }

    bHandled = TRUE;
	
    switch(pControlNavigateData->nControlNavigate)
	{
       case XUI_CONTROL_NAVIGATE_LEFT:
       case XUI_CONTROL_NAVIGATE_RIGHT:
       case XUI_CONTROL_NAVIGATE_UP:
       case XUI_CONTROL_NAVIGATE_DOWN:
          pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
          break;
    }

	return 0;
}

HRESULT CRetroArchQuickMenu::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiQuickMenuList", &m_quickmenulist);
   GetChildById(L"XuiBackButton", &m_back);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, strw_buffer);
   
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);

   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);

   return 0;
}

HRESULT CRetroArchQuickMenu::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   bool aspectratio_changed = false;
   int current_index;
   xdk360_video_t *d3d9 = (xdk360_video_t*)driver.video_data;
   
   current_index = m_quickmenulist.GetCurSel();

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         switch(current_index)
         {
            case MENU_ITEM_LOAD_STATE:
			case MENU_ITEM_SAVE_STATE:
               rarch_state_slot_decrease();
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_ITEM_KEEP_ASPECT_RATIO:
               rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
               aspectratio_changed = true;
               break;
            case MENU_ITEM_ORIENTATION:
               rarch_settings_change(S_ROTATION_DECREMENT);
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
               video_xdk360.set_rotation(driver.video_data, g_console.screen_orientation);
               break;
            default:
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         switch(current_index)
         {
            case MENU_ITEM_LOAD_STATE:
			case MENU_ITEM_SAVE_STATE:
               rarch_state_slot_increase();
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_ITEM_KEEP_ASPECT_RATIO:
               rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
	           aspectratio_changed = true;
               break;
            case MENU_ITEM_ORIENTATION:
               rarch_settings_change(S_ROTATION_INCREMENT);
               rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
               video_xdk360.set_rotation(driver.video_data, g_console.screen_orientation);
               break;
            default:
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_UP:
      case XUI_CONTROL_NAVIGATE_DOWN:
         break;
   }

   if(aspectratio_changed)
   {
      gfx_ctx_set_aspect_ratio(d3d9, g_console.aspect_ratio_index);
	  rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
      m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, strw_buffer);
   }

   bHandled = TRUE;

    switch(pControlNavigateData->nControlNavigate)
	{
       case XUI_CONTROL_NAVIGATE_LEFT:
       case XUI_CONTROL_NAVIGATE_RIGHT:
       case XUI_CONTROL_NAVIGATE_UP:
       case XUI_CONTROL_NAVIGATE_DOWN:
          pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
          break;
    }

   return 0;
}

HRESULT CRetroArchQuickMenu::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk360_video_t *d3d9 = (xdk360_video_t*)driver.video_data;
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
               rarch_settings_change(S_RETURN_TO_GAME);
            }
            break;
	 case MENU_ITEM_SAVE_STATE:
	    if (g_console.emulator_initialized)
	    {
           rarch_save_state();
           rarch_settings_change(S_RETURN_TO_GAME);
	    }
	    break;
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
	    {
           rarch_settings_default(S_DEF_ASPECT_RATIO);

	       gfx_ctx_set_aspect_ratio(d3d9, g_console.aspect_ratio_index);
		   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
	       m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, strw_buffer);
	    }
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
        if(g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_NOT_IMPLEMENTED, S_DELAY_180);
	    break;
	 case MENU_ITEM_ORIENTATION:
        rarch_settings_default(S_DEF_ROTATION);
        rarch_settings_create_menu_item_label(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
        m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
	    video_xdk360.set_rotation(driver.video_data, g_console.screen_orientation);
	    break;
	 case MENU_ITEM_RESIZE_MODE:
	    g_console.input_loop = INPUT_LOOP_RESIZE_MODE;
	    if (g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_RESIZE_SCREEN, S_DELAY_270);
	    break;
	 case MENU_ITEM_FRAME_ADVANCE:
	    if (g_console.emulator_initialized)
               rarch_settings_change(S_FRAME_ADVANCE);
	    break;
	 case MENU_ITEM_SCREENSHOT_MODE:
	    if (g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_NOT_IMPLEMENTED, S_DELAY_180);
	    break;
	 case MENU_ITEM_RESET:
	    if (g_console.emulator_initialized)
	    {
           rarch_settings_change(S_RETURN_TO_GAME);
           rarch_game_reset();
	    }
	    break;
	 case MENU_ITEM_RETURN_TO_GAME:
	    if (g_console.emulator_initialized)
               rarch_settings_change(S_RETURN_TO_GAME);
	    break;
	 case MENU_ITEM_RETURN_TO_DASHBOARD:
	    rarch_settings_change(S_RETURN_TO_DASHBOARD);
	    break;
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
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
   GetChildById(L"XuiBtnLibsnesCore", &m_change_libretro_core);

   char core_text[256];
   snprintf(core_text, sizeof(core_text), "%s (v%s)", id, info.library_version);

   rarch_convert_char_to_wchar(strw_buffer, core_text, sizeof(strw_buffer));
   m_core.SetText(strw_buffer);
   rarch_settings_create_menu_item_label(strw_buffer, S_LBL_RARCH_VERSION, sizeof(strw_buffer));
   m_title.SetText(strw_buffer);

   return 0;
}

HRESULT CRetroArchFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      if(path_file_exists(browser.current_dir.list->elems[index].data))
      {
         struct retro_system_info info;
         retro_get_system_info(&info);
         bool block_zip_extract  = info.block_extract;
		 const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t*)m_romlist.GetText(index));

         if((strstr(strbuffer, ".zip") || strstr(strbuffer, ".ZIP")) && !block_zip_extract)
         {
            char path_tmp[1024];
            snprintf(path_tmp, sizeof(path_tmp), "%s\\%s", filebrowser_get_current_dir(&browser), strbuffer);
            rarch_extract_zipfile(path_tmp);
         }
         else
         {
            memset(g_console.rom_path, 0, sizeof(g_console.rom_path));
            snprintf(g_console.rom_path, sizeof(g_console.rom_path), "%s\\%s", filebrowser_get_current_dir(&browser), strbuffer);
            rarch_settings_change(S_START_RARCH);
         }
      }
      else if(browser.current_dir.list->elems[index].attr.b)
      {

         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&browser), strbuffer);
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
      if (g_console.info_msg_enable)
         rarch_settings_msg(S_MSG_CACHE_PARTITION, S_DELAY_180);
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   if(hObjPressed == m_shaderlist)
   {
      int index = m_shaderlist.GetCurSel();
      if(path_file_exists(tmp_browser.current_dir.list->elems[index].data))
      {
         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_shaderlist.GetText(index));
		 
         switch(set_shader)
         {
            case 1:
               snprintf(g_settings.video.cg_shader_path, sizeof(g_settings.video.cg_shader_path), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), strbuffer);
               hlsl_load_shader(set_shader, g_settings.video.cg_shader_path);
               break;
            case 2:
               snprintf (g_settings.video.second_pass_shader, sizeof(g_settings.video.second_pass_shader), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), strbuffer);
               hlsl_load_shader(set_shader, g_settings.video.second_pass_shader);
               break;
            default:
               break;
         }

         if (g_console.info_msg_enable)
            rarch_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
      }
      else if(tmp_browser.current_dir.list->elems[index].attr.b)
      {
         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_shaderlist.GetText(index));
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), strbuffer);
         filebrowser_fetch_directory_entries(path, &tmp_browser, &m_shaderlist, &m_shaderpathtitle);
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      if(path_file_exists(tmp_browser.current_dir.list->elems[index].data))
      {
         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
         snprintf(g_console.launch_app_on_exit, sizeof(g_console.launch_app_on_exit), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), strbuffer);
         rarch_settings_change(S_RETURN_TO_LAUNCHER);
      }
      else if(tmp_browser.current_dir.list->elems[index].attr.b)
      {
         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
         snprintf(path, sizeof(path), "%s%s\\", filebrowser_get_current_dir(&tmp_browser), strbuffer);
         filebrowser_fetch_directory_entries(path, &tmp_browser, &m_romlist, &m_rompathtitle);
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchSettings::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   int current_index;
   HRESULT hr;

   if ( hObjPressed == m_settingslist)
   {
      current_index = m_settingslist.GetCurSel();

      switch(current_index)
      {
         case SETTING_EMU_REWIND_ENABLED:
            rarch_settings_change(S_REWIND);
            m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
			
            if (g_console.info_msg_enable)
	           rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
            break;
	 case SETTING_EMU_SHOW_INFO_MSG:
	    g_console.info_msg_enable = !g_console.info_msg_enable;
	    m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_console.info_msg_enable ? L"Info messages: ON" : L"Info messages: OFF");
	    break;
	 case SETTING_EMU_MENUS:
	    g_console.menus_hd_enable = !g_console.menus_hd_enable;
	    m_settingslist.SetText(SETTING_EMU_MENUS, g_console.menus_hd_enable ? L"Menus: HD" : L"Menus: SD");
	    break;
	 case SETTING_GAMMA_CORRECTION_ENABLED:
	    g_console.gamma_correction_enable = !g_console.gamma_correction_enable;
	    m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
	    if(g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
	    break;
	 case SETTING_COLOR_FORMAT:
	    g_console.color_format = !g_console.color_format;
	    m_settingslist.SetText(SETTING_COLOR_FORMAT, g_console.color_format ? L"Color format: 32bit ARGB" : L"Color format: 16bit RGBA");
	    if(g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
	    break;
	 case SETTING_SHADER:
	    set_shader = 1;
	    hr = XuiSceneCreate(g_console.menus_hd_enable ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);

	    if (hr < 0)
	    {
               RARCH_ERR("Failed to load scene.\n");
	    }
	    hCur = app.hShaderBrowser;

        if (g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_SELECT_SHADER, S_DELAY_180);
	    NavigateForward(app.hShaderBrowser);
	    break;
	 case SETTING_SHADER_2:
        set_shader = 2;
	    hr = XuiSceneCreate(g_console.menus_hd_enable ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);
	    if (hr < 0)
	    {
		    RARCH_ERR("Failed to load scene.\n");
	    }
	    hCur = app.hShaderBrowser;
	    if (g_console.info_msg_enable)
           rarch_settings_msg(S_MSG_SELECT_SHADER, S_DELAY_180);
	    NavigateForward(app.hShaderBrowser);
	    break;
	 case SETTING_HW_TEXTURE_FILTER:
	    g_settings.video.smooth = !g_settings.video.smooth;
	    m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Hardware filtering shader #1: Linear interpolation" : L"Hardware filtering shader #1: Point filtering");
	    break;
	 case SETTING_HW_TEXTURE_FILTER_2:
	    g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
	    m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER_2, g_settings.video.second_pass_smooth ? L"Hardware filtering shader #2: Linear interpolation" : L"Hardware filtering shader #2: Point filtering");
	    break;
	 case SETTING_SCALE_ENABLED:
	    g_console.fbo_enabled = !g_console.fbo_enabled;
	    m_settingslist.SetText(SETTING_SCALE_ENABLED, g_console.fbo_enabled ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
	    gfx_ctx_set_fbo(g_console.fbo_enabled);
	    break;
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;

   bool hdmenus_allowed = g_console.menus_hd_enable;

   HRESULT hr;

   if ( hObjPressed == m_filebrowser )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_filebrowser.xur", NULL, &app.hFileBrowser);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");

      hCur = app.hFileBrowser;
      NavigateForward(app.hFileBrowser);
   }
   else if ( hObjPressed == m_quick_menu)
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_quickmenu.xur", NULL, &app.hQuickMenu);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");

      hCur = app.hQuickMenu;
      NavigateForward(app.hQuickMenu);
   }
   else if ( hObjPressed == m_controls)
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_controls.xur", NULL, &app.hControlsMenu);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");

      hCur = app.hControlsMenu;
      if (g_console.info_msg_enable)
         rarch_settings_msg(S_MSG_CHANGE_CONTROLS, S_DELAY_180);
      NavigateForward(app.hControlsMenu);
   }
   else if ( hObjPressed == m_change_libretro_core )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_libretrocore_browser.xur", NULL, &app.hCoreBrowser);

      if (hr < 0)
      {
         RARCH_ERR("Failed to load scene.\n");
      }
      hCur = app.hCoreBrowser;

      if (g_console.info_msg_enable)
         rarch_settings_msg(S_MSG_SELECT_LIBRETRO_CORE, S_DELAY_180);
      NavigateForward(app.hCoreBrowser);
   }
   else if ( hObjPressed == m_settings )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_settings.xur", NULL, &app.hRetroArchSettings);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");

      hCur = app.hRetroArchSettings;
      NavigateForward(app.hRetroArchSettings);
   }
   else if ( hObjPressed == m_quit )
      rarch_settings_change(S_RETURN_TO_DASHBOARD);

   bHandled = TRUE;
   return 0;
}

int menu_init (void)
{
   HRESULT hr;

   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;

   bool hdmenus_allowed = g_console.menus_hd_enable;

   hr = app.InitShared(vid->d3d_render_device, &vid->d3dpp, XuiPNGTextureLoader);

   if (hr < 0)
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      return 1;
   }

   /* Register font */
   hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game:/media/rarch.ttf" );
   if (hr < 0)
   {
      RARCH_ERR("Failed to register default typeface.\n");
      return 1;
   }

   hr = app.LoadSkin( L"file://game:/media/rarch_scene_skin.xur");
   if (hr < 0)
   {
      RARCH_ERR("Failed to load skin.\n");
      return 1;
   }

   hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_main.xur", NULL, &app.hMainScene);
   if (hr < 0)
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      return 1;
   }

   hCur = app.hMainScene;
   XuiSceneNavigateFirst(app.GetRootObj(), app.hMainScene, XUSER_INDEX_FOCUS);

   filebrowser_new(&browser, g_console.default_rom_startup_dir, rarch_console_get_rom_ext());

   return 0;
}

void menu_free (void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmp_browser);
   app.Uninit();
}

void menu_loop(void)
{
   HRESULT hr;
   xdk360_video_t *d3d9 = (xdk360_video_t*)driver.video_data;

   g_console.menu_enable = true;

   d3d9->block_swap = true;

   g_console.input_loop = INPUT_LOOP_MENU;

   do
   {
      if(g_console.emulator_initialized)
	  {
         rarch_render_cached_frame();
	  }
	  else
	  {
         d3d9->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
         d3d9->frame_count++;
	  }

      XINPUT_STATE state;
      XInputGetState(0, &state);

      g_console.menu_enable = !((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) 
      && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) && (g_console.emulator_initialized)
      && IS_TIMER_EXPIRED(d3d9));

      g_console.mode_switch = g_console.menu_enable ? MODE_MENU : MODE_EMULATION;

      switch(g_console.input_loop)
      {
         case INPUT_LOOP_MENU:
            app.RunFrame();			/* Update XUI */
            if(state.Gamepad.wButtons & XINPUT_GAMEPAD_B && hCur != app.hMainScene)
               XuiSceneNavigateBack(hCur, app.hMainScene, XUSER_INDEX_ANY);
            break;
         case INPUT_LOOP_RESIZE_MODE:
            xdk360_input_loop();
            break;
         default:
            break;
      }

      hr = app.Render();		/* Render XUI */
      hr = XuiTimersRun();	/* Update XUI timers */

      if(g_console.mode_switch == MODE_EMULATION && !g_console.frame_advance_enable)
      {
         SET_TIMER_EXPIRATION(d3d9, 30);
      }

      const char *message = msg_queue_pull(g_extern.msg_queue);

      if (message)
      {
         if(IS_TIMER_EXPIRED(d3d9))
         {
            xdk360_console_format(message);
            SET_TIMER_EXPIRATION(d3d9, 30);
         }
         xdk360_console_draw();
      }

      gfx_ctx_swap_buffers();
   }while(g_console.menu_enable);

   d3d9->block_swap = false;

   g_console.ingame_menu_enable = false;
}
