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
#include "../console/fileio/file_browser.h"
#include "../console/console_ext.h"
#include "xdk360_video.h"
#include "menu.h"
#include "../message.h"
#include "shared.h"

#include "../general.h"

CRetroArch        app;
HXUIOBJ hCur;
filebrowser_t browser;
filebrowser_t tmp_browser;
bool hdmenus_allowed;
uint32_t set_shader = 0;

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
HRESULT CRetroArch::RegisterXuiClasses (void)
{
   CRetroArchMain::Register();
   CRetroArchFileBrowser::Register();
   CRetroArchCoreBrowser::Register();
   CRetroArchShaderBrowser::Register();
   CRetroArchQuickMenu::Register();
   CRetroArchControls::Register();
   CRetroArchSettings::Register();

   return S_OK;
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

   return S_OK;
}

static void filebrowser_fetch_directory_entries(const char *path, filebrowser_t * browser, CXuiList * romlist, 
   CXuiTextElement * rompath_title)
{
   filebrowser_push_directory(browser, path, true);

   wchar_t * rompath_name = rarch_convert_char_to_wchar(path);
   rompath_title->SetText(rompath_name);
   free(rompath_name);

   romlist->DeleteItems(0, romlist->GetItemCount());
   romlist->InsertItems(0, browser->file_count);
   for(unsigned i = 0; i < browser->file_count; i++)
   {
      wchar_t * entry_name = rarch_convert_char_to_wchar(browser->cur[i].d_name);
      romlist->SetText(i, entry_name);
      free(entry_name);
   }
}

HRESULT CRetroArchFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);
   GetChildById(L"XuiBtnGameDir", &m_dir_game);
   GetChildById(L"XuiBtnCacheDir", &m_dir_cache);

   filebrowser_fetch_directory_entries(g_console.default_rom_startup_dir, &browser, &m_romlist, &m_rompathtitle);

   return S_OK;
}

HRESULT CRetroArchCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);

   filebrowser_new(&tmp_browser, "game:", "xex|XEX");
   filebrowser_fetch_directory_entries("game:", &tmp_browser, &m_romlist, &m_rompathtitle);

   return S_OK;
}

HRESULT CRetroArchShaderBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_shaderlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_shaderpathtitle);

   filebrowser_new(&tmp_browser, "game:\\media\\shaders", "cg|CG");
   filebrowser_fetch_directory_entries("game:\\media\\shaders", &tmp_browser, &m_shaderlist, &m_shaderpathtitle);

   return S_OK;
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
	   snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[i], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      m_controlslist.SetText(i, rarch_convert_char_to_wchar(buttons[i]));
   }
   
   return S_OK;
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
	   snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[i], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      m_controlslist.SetText(i, rarch_convert_char_to_wchar(buttons[i]));
   }

	switch(pControlNavigateData->nControlNavigate)
	{
       case XUI_CONTROL_NAVIGATE_LEFT:
		   if(current_index > 0 && current_index != SETTING_CONTROLS_DEFAULT_ALL)
		   {
              rarch_input_set_keybind(controlno, KEYBIND_DECREMENT, current_index);
			  snprintf(button, sizeof(button), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[current_index], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
              m_controlslist.SetText(current_index, rarch_convert_char_to_wchar(button));
		   }
          break;
	   case XUI_CONTROL_NAVIGATE_RIGHT:
		   if(current_index < RARCH_FIRST_META_KEY && current_index != SETTING_CONTROLS_DEFAULT_ALL)
		   {
              rarch_input_set_keybind(controlno, KEYBIND_INCREMENT, current_index);
			  snprintf(button, sizeof(button), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[current_index], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
              m_controlslist.SetText(current_index, rarch_convert_char_to_wchar(button));
		   }
          break;
	   case XUI_CONTROL_NAVIGATE_UP:
	   case XUI_CONTROL_NAVIGATE_DOWN:
          break;
	}

	return S_OK;
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
				snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[i], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
               m_controlslist.SetText(i, rarch_convert_char_to_wchar(buttons[i]));
            }
            break;
         default:
            rarch_input_set_keybind(controlno, KEYBIND_DEFAULT, current_index);
			snprintf(buttons[current_index], sizeof(buttons[current_index]), "%s #%d: %s", rarch_default_libretro_keybind_name_lut[current_index], controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
            m_controlslist.SetText(current_index, rarch_convert_char_to_wchar(buttons[current_index]));
            break;
      }
   }

   bHandled = TRUE;
   return S_OK;
}

HRESULT CRetroArchSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   char shader1str[128], shader2str[128], scalefactor[128];
   GetChildById(L"XuiSettingsList", &m_settingslist);
   GetChildById(L"XuiBackButton", &m_back);

   snprintf(shader1str, sizeof(shader1str), "Shader #1: %s", g_settings.video.cg_shader_path);
   snprintf(shader2str, sizeof(shader2str), "Shader #2: %s", g_settings.video.second_pass_shader);
   snprintf(scalefactor, sizeof(scalefactor), "Scale Factor: %f (X) / %f (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);

   m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
   m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Hardware filtering shader #1: Linear interpolation" : L"Hardware filtering shader #1: Point filtering");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER_2, g_settings.video.second_pass_smooth ? L"Hardware filtering shader #2: Linear interpolation" : L"Hardware filtering shader #2: Point filtering");
   m_settingslist.SetText(SETTING_SCALE_ENABLED, g_console.fbo_enabled ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
   m_settingslist.SetText(SETTING_SHADER, rarch_convert_char_to_wchar(shader1str));
   m_settingslist.SetText(SETTING_COLOR_FORMAT, g_console.color_format ? L"Color format: 32bit ARGB" : L"Color format: 16bit RGBA");
   m_settingslist.SetText(SETTING_SHADER_2, rarch_convert_char_to_wchar(shader2str));
   m_settingslist.SetText(SETTING_SCALE_FACTOR, rarch_convert_char_to_wchar(scalefactor));

   return S_OK;
}

HRESULT CRetroArchSettings::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   char scalefactor[128];
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
					   g_settings.video.fbo_scale_x -= 1.0f;
					   g_settings.video.fbo_scale_y -= 1.0f;
					   //xdk360_gfx_init_fbo(vid);
					   snprintf(scalefactor, sizeof(scalefactor), "Scale Factor: %f (X) / %f (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
                       m_settingslist.SetText(SETTING_SCALE_FACTOR, rarch_convert_char_to_wchar(scalefactor));
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
					   g_settings.video.fbo_scale_x += 1.0f;
					   g_settings.video.fbo_scale_y += 1.0f;
					   //xdk360_gfx_init_fbo(vid);
                       snprintf(scalefactor, sizeof(scalefactor), "Scale Factor: %f (X) / %f (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
                       m_settingslist.SetText(SETTING_SCALE_FACTOR, rarch_convert_char_to_wchar(scalefactor));
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

	return S_OK;
}

HRESULT CRetroArchQuickMenu::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiQuickMenuList", &m_quickmenulist);
   GetChildById(L"XuiBackButton", &m_back);
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
   snprintf(aspectratio_label, sizeof(aspectratio_label), "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);
   wchar_t * aspectratio_label_w = rarch_convert_char_to_wchar(aspectratio_label);
   m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, aspectratio_label_w);
   free(aspectratio_label_w);

   return S_OK;
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
	 case MENU_ITEM_KEEP_ASPECT_RATIO:
        {
			g_console.aspect_ratio_index++;
			if(g_console.aspect_ratio_index >= ASPECT_RATIO_END)
			   g_console.aspect_ratio_index = 0;

			gfx_ctx_set_aspect_ratio(d3d9, g_console.aspect_ratio_index);
			char aspectratio_label[32];
			snprintf(aspectratio_label, sizeof(aspectratio_label), "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);
			wchar_t * aspectratio_label_w = rarch_convert_char_to_wchar(aspectratio_label);
			m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, aspectratio_label_w);
			free(aspectratio_label_w);
		}
	    break;
	 case MENU_ITEM_OVERSCAN_AMOUNT:
		 msg_queue_clear(g_extern.msg_queue);
	     msg_queue_push(g_extern.msg_queue, "TODO - Not yet implemented.", 1, 180);
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
	    video_xdk360.set_rotation(driver.video_data, g_console.screen_orientation);
	    break;
	 case MENU_ITEM_RESIZE_MODE:
			g_console.input_loop = INPUT_LOOP_RESIZE_MODE;
			msg_queue_clear(g_extern.msg_queue);
			msg_queue_push(g_extern.msg_queue, "INFO - Resize the screen by moving around the two analog sticks.\nPress Y to reset to default values, and B to go back.\nTo select the resized screen mode, set Aspect Ratio to: 'Custom'.", 1, 270);
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
		 msg_queue_clear(g_extern.msg_queue);
	     msg_queue_push(g_extern.msg_queue, "TODO - Not yet implemented.", 1, 180);
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

   bHandled = TRUE;

   return S_OK;
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

   char package_version[32];
   snprintf(package_version, sizeof(core_text), "RetroArch %s", PACKAGE_VERSION);

   wchar_t * core_text_utf = rarch_convert_char_to_wchar(core_text);
   wchar_t * package_version_utf = rarch_convert_char_to_wchar(package_version);
   m_core.SetText(core_text_utf);
   m_title.SetText(package_version_utf);
   free(core_text_utf);
   free(package_version_utf);

   return S_OK;
}

HRESULT CRetroArchFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
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

		 const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t*)m_romlist.GetText(index));

         if((strstr(strbuffer, ".zip") || strstr(strbuffer, ".ZIP")) && !block_zip_extract)
         {
            char path_tmp[1024];
            snprintf(path_tmp, sizeof(path_tmp), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), strbuffer);
            rarch_extract_zipfile(path_tmp);
         }
         else
         {
            memset(g_console.rom_path, 0, sizeof(g_console.rom_path));
            snprintf(g_console.rom_path, sizeof(g_console.rom_path), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(browser), strbuffer);
            return_to_game();
            g_console.initialize_rarch_enable = 1;
         }
      }
      else if(browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
      {

         const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
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
	  msg_queue_clear(g_extern.msg_queue);
	  msg_queue_push(g_extern.msg_queue, "INFO - All the contents of the ZIP files you have selected in the filebrowser\nare extracted to this partition.", 1, 180);
   }

   bHandled = TRUE;

   return S_OK;
}

HRESULT CRetroArchShaderBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[MAX_PATH_LENGTH];

   if(hObjPressed == m_shaderlist)
   {
      int index = m_shaderlist.GetCurSel();
      if(tmp_browser.cur[index].d_type != FILE_ATTRIBUTE_DIRECTORY)
      {
		 const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_shaderlist.GetText(index));

		 switch(set_shader)
		 {
		    case 1:
			   snprintf(g_settings.video.cg_shader_path, sizeof(g_settings.video.cg_shader_path), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
               hlsl_load_shader(set_shader, g_settings.video.cg_shader_path);
               break;
			case 2:
			  snprintf (g_settings.video.second_pass_shader, sizeof(g_settings.video.second_pass_shader), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
               hlsl_load_shader(set_shader, g_settings.video.second_pass_shader);
			   break;
		    default:
	           break;
		 }
		 msg_queue_clear(g_extern.msg_queue);
	     msg_queue_push(g_extern.msg_queue, "INFO - Shader successfully loaded.", 1, 180);
      }
      else if(tmp_browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
      {
		 const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_shaderlist.GetText(index));
		 snprintf(path, sizeof(path), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
		 filebrowser_fetch_directory_entries(path, &tmp_browser, &m_shaderlist, &m_shaderpathtitle);
      }
   }

   bHandled = TRUE;

   return S_OK;
}

HRESULT CRetroArchCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[MAX_PATH_LENGTH];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      if(tmp_browser.cur[index].d_type != FILE_ATTRIBUTE_DIRECTORY)
      {
	     const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
	     snprintf(g_console.launch_app_on_exit, sizeof(g_console.launch_app_on_exit), "%s\\%s", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
	     g_console.return_to_launcher = true;
	     g_console.menu_enable = false;
	     g_console.mode_switch = MODE_EXIT;
      }
      else if(tmp_browser.cur[index].d_type == FILE_ATTRIBUTE_DIRECTORY)
      {
	     const char * strbuffer = rarch_convert_wchar_to_const_char((const wchar_t *)m_romlist.GetText(index));
	     snprintf(path, sizeof(path), "%s%s\\", FILEBROWSER_GET_CURRENT_DIRECTORY_NAME(tmp_browser), strbuffer);
	     filebrowser_fetch_directory_entries(path, &tmp_browser, &m_romlist, &m_rompathtitle);
      }
   }

   bHandled = TRUE;
   return S_OK;
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
           g_settings.rewind_enable = !g_settings.rewind_enable;
	    m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
		msg_queue_clear(g_extern.msg_queue);
		msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch for this change to take effect.", 1, 180);
	    break;
	 case SETTING_GAMMA_CORRECTION_ENABLED:
	    g_console.gamma_correction_enable = !g_console.gamma_correction_enable;
	    m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_console.gamma_correction_enable ? L"Gamma correction: ON" : L"Gamma correction: OFF");
		msg_queue_clear(g_extern.msg_queue);
		msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch for this change to take effect.", 1, 180);
	    break;
	 case SETTING_COLOR_FORMAT:
		g_console.color_format = !g_console.color_format;
	    m_settingslist.SetText(SETTING_COLOR_FORMAT, g_console.color_format ? L"Color format: 32bit ARGB" : L"Color format: 16bit RGBA");
		msg_queue_clear(g_extern.msg_queue);
		msg_queue_push(g_extern.msg_queue, "INFO - You need to restart RetroArch for this change to take effect.", 1, 180);
		 break;
	 case SETTING_SHADER:
		 set_shader = 1;
	     hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);

         if (hr < 0)
         {
            RARCH_ERR("Failed to load scene.\n");
         }
		 hCur = app.hShaderBrowser;
		 msg_queue_clear(g_extern.msg_queue);
	     msg_queue_push(g_extern.msg_queue, "INFO - Select a shader from the menu by pressing the A button.", 1, 180);
         NavigateForward(app.hShaderBrowser);
		 break;
	 case SETTING_SHADER_2:
		 set_shader = 2;
		 hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);

         if (hr < 0)
         {
            RARCH_ERR("Failed to load scene.\n");
         }
		 hCur = app.hShaderBrowser;
		 msg_queue_clear(g_extern.msg_queue);
	     msg_queue_push(g_extern.msg_queue, "INFO - Select a shader from the menu by pressing the A button.", 1, 180);
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
   return S_OK;
}

HRESULT CRetroArchMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;

   hdmenus_allowed = vid->video_mode.fIsHiDef && (g_console.aspect_ratio_index >= ASPECT_RATIO_16_9);

   HRESULT hr;

   if ( hObjPressed == m_filebrowser )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_filebrowser.xur", NULL, &app.hFileBrowser);

      if (hr < 0)
      {
         RARCH_ERR("Failed to load scene.\n");
      }
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
	  msg_queue_clear(g_extern.msg_queue);
	  msg_queue_push(g_extern.msg_queue, "INFO - Press LEFT/RIGHT to change the controls, and press\nSTART/A to reset a button to default values.", 1, 180);
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
	  msg_queue_clear(g_extern.msg_queue);
	  msg_queue_push(g_extern.msg_queue, "INFO - Select a Libretro core from the menu by pressing the A button.", 1, 180);
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
      return_to_dashboard();

   bHandled = TRUE;
   return 0;
}

int menu_init (void)
{
   HRESULT hr;

   xdk360_video_t *vid = (xdk360_video_t*)driver.video_data;

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

   hr = XuiSceneCreate(L"file://game:/media/sd/", L"rarch_main.xur", NULL, &app.hMainScene);
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

void menu_deinit (void)
{
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
      rarch_render_cached_frame();

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

	  /* XBox 360 specific font code */
	  {
		 const char *msg = msg_queue_pull(g_extern.msg_queue);

         if (msg)
         {
            if(IS_TIMER_EXPIRED(d3d9) || g_first_msg)
            {
               xdk360_console_format(msg);
               g_first_msg = 0;
               SET_TIMER_EXPIRATION(d3d9, 30);
            }

            xdk360_console_draw();
         }
	  }

      gfx_ctx_swap_buffers();
   }while(g_console.menu_enable);

   d3d9->block_swap = false;

   g_console.ingame_menu_enable = false;
}
