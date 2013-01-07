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

#include <stdint.h>
#include <crtdefs.h>
#include <tchar.h>
#include <xtl.h>
#include "../../console/fileio/file_browser.h"

#include "../../console/rarch_console.h"
#include "../../console/rarch_console_settings.h"
#include "../../console/rarch_console_video.h"

#include "../../console/rmenu/rmenu.h"
#include "../../gfx/gfx_context.h"

#include "../../xdk/xdk_d3d.h"
#include "menu.h"
#include "../../message.h"

#include "../../general.h"

CRetroArch app;
HXUIOBJ hCur;
filebrowser_t browser;
filebrowser_t tmp_browser;
uint32_t set_shader = 0;

wchar_t strw_buffer[PATH_MAX];
char str_buffer[PATH_MAX];

enum
{
   RMENU_DEVICE_NAV_UP = 0,
   RMENU_DEVICE_NAV_DOWN,
   RMENU_DEVICE_NAV_LEFT,
   RMENU_DEVICE_NAV_RIGHT,
   RMENU_DEVICE_NAV_UP_ANALOG_L,
   RMENU_DEVICE_NAV_DOWN_ANALOG_L,
   RMENU_DEVICE_NAV_LEFT_ANALOG_L,
   RMENU_DEVICE_NAV_RIGHT_ANALOG_L,
   RMENU_DEVICE_NAV_UP_ANALOG_R,
   RMENU_DEVICE_NAV_DOWN_ANALOG_R,
   RMENU_DEVICE_NAV_LEFT_ANALOG_R,
   RMENU_DEVICE_NAV_RIGHT_ANALOG_R,
   RMENU_DEVICE_NAV_B,
   RMENU_DEVICE_NAV_A,
   RMENU_DEVICE_NAV_X,
   RMENU_DEVICE_NAV_Y,
   RMENU_DEVICE_NAV_START,
   RMENU_DEVICE_NAV_SELECT,
   RMENU_DEVICE_NAV_L1,
   RMENU_DEVICE_NAV_R1,
   RMENU_DEVICE_NAV_L2,
   RMENU_DEVICE_NAV_R2,
   RMENU_DEVICE_NAV_L3,
   RMENU_DEVICE_NAV_R3,
   RMENU_DEVICE_NAV_LAST
};

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

static void browser_update(filebrowser_t * b, uint64_t input, const char *extensions)
{
   bool ret = true;
   filebrowser_action_t action = FILEBROWSER_ACTION_NOOP;

   if (input & (1ULL << RMENU_DEVICE_NAV_DOWN))
      action = FILEBROWSER_ACTION_DOWN;
   else if (input & (1ULL << RMENU_DEVICE_NAV_UP))
      action = FILEBROWSER_ACTION_UP;
   else if (input & (1ULL << RMENU_DEVICE_NAV_RIGHT))
      action = FILEBROWSER_ACTION_RIGHT;
   else if (input & (1ULL << RMENU_DEVICE_NAV_LEFT))
      action = FILEBROWSER_ACTION_LEFT;
   else if (input & (1ULL << RMENU_DEVICE_NAV_R2))
      action = FILEBROWSER_ACTION_SCROLL_DOWN;
   else if (input & (1ULL << RMENU_DEVICE_NAV_L2))
      action = FILEBROWSER_ACTION_SCROLL_UP;
   else if (input & (1ULL << RMENU_DEVICE_NAV_A))
      action = FILEBROWSER_ACTION_CANCEL;
   else if (input & (1ULL << RMENU_DEVICE_NAV_START))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root(b, default_paths.filesystem_root_dir);
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      ret = filebrowser_iterate(b, action);

   if(!ret)
      rarch_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);
}

static void filebrowser_fetch_directory_entries(filebrowser_t * browser, uint64_t action, CXuiList * romlist, CXuiTextElement * rompath_title)
{
   browser_update(browser, action, browser->extensions); 

   convert_char_to_wchar(strw_buffer, filebrowser_get_current_dir(browser), sizeof(strw_buffer));
   rompath_title->SetText(strw_buffer);

   romlist->DeleteItems(0, romlist->GetItemCount());
   romlist->InsertItems(0, browser->current_dir.list->size);

   for(unsigned i = 0; i < browser->current_dir.list->size; i++)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, browser->current_dir.list->elems[i].data, sizeof(fname_tmp));
      convert_char_to_wchar(strw_buffer, fname_tmp, sizeof(strw_buffer));
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

   filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), g_extern.console.main_wrap.paths.default_rom_startup_dir);

   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(&browser, action, &m_romlist, &m_rompathtitle);

   return 0;
}

HRESULT CRetroArchFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      convert_wchar_to_char(str_buffer, (const wchar_t *)m_romlist.GetText(index), sizeof(str_buffer));
      if(path_file_exists(browser.current_dir.list->elems[index].data))
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&browser), str_buffer);
         rarch_console_load_game_wrap(path, g_extern.file_state.zip_extract_mode, S_DELAY_45);
      }
      else if(browser.current_dir.list->elems[index].attr.b)
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&browser), str_buffer);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), path);
         filebrowser_fetch_directory_entries(&browser, action, &m_romlist, &m_rompathtitle);
      }
   }
   else if (hObjPressed == m_dir_game)
   {
      filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), g_extern.console.main_wrap.paths.default_rom_startup_dir);
      uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
      filebrowser_fetch_directory_entries(&browser, action, &m_romlist, &m_rompathtitle);
   }
#ifdef HAVE_HDD_CACHE_PARTITION
   else if (hObjPressed == m_dir_cache)
   {
      filebrowser_set_root_and_ext(&browser, rarch_console_get_rom_ext(), "cache:");
      uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
      filebrowser_fetch_directory_entries(&browser, action, &m_romlist, &m_rompathtitle);

      if (g_extern.console.rmenu.state.msg_info.enable)
         rarch_settings_msg(S_MSG_CACHE_PARTITION, S_DELAY_180);
   }
#endif

   bHandled = TRUE;

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

   m_controlnoslider.SetValue(0);
   m_controlnoslider.GetValue(&controlno);

   for(i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", 
            rarch_input_get_default_keybind_name(i), controlno, 
            rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer)); 
      m_controlslist.SetText(i, strw_buffer);
   }

   snprintf(buttons[0], sizeof(buttons[0]), "D-Pad Emulation: %s", rarch_dpad_emulation_name_lut[g_settings.input.dpad_emulation[controlno]]);
   convert_char_to_wchar(strw_buffer, buttons[0], sizeof(strw_buffer));
   m_controlslist.SetText(SETTING_CONTROLS_DPAD_EMULATION, strw_buffer);
   m_controlslist.SetText(SETTING_CONTROLS_DEFAULT_ALL, L"Reset all buttons to default");

   return 0;
}

HRESULT CRetroArchControls::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   char button[128];
   char buttons[RARCH_FIRST_META_KEY][128];
   int controlno, i, current_index;

   current_index = m_controlslist.GetCurSel();
   m_controlnoslider.GetValue(&controlno);

   for(i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", 
            rarch_input_get_default_keybind_name(i), controlno, 
            rarch_input_find_platform_key_label(g_settings.input.binds[controlno][i].joykey));
      convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer));
      m_controlslist.SetText(i, strw_buffer);
   }

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         switch(current_index)
         {
            case SETTING_CONTROLS_DPAD_EMULATION:
               switch(g_settings.input.dpad_emulation[controlno])
               {
                  case DPAD_EMULATION_NONE:
                     break;
                  case DPAD_EMULATION_LSTICK:
                     driver.input->set_analog_dpad_mapping(0, DPAD_EMULATION_NONE, controlno);
                     break;
                  case DPAD_EMULATION_RSTICK:
                     driver.input->set_analog_dpad_mapping(0, DPAD_EMULATION_LSTICK, controlno);
                     break;
               }
               break;
            case SETTING_CONTROLS_DEFAULT_ALL:
               break;
            default:
               rarch_input_set_keybind(controlno, KEYBIND_DECREMENT, current_index);
               snprintf(button, sizeof(button), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
               convert_char_to_wchar(strw_buffer, button, sizeof(strw_buffer));
               m_controlslist.SetText(current_index, strw_buffer);
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         switch(current_index)
         {
            case SETTING_CONTROLS_DPAD_EMULATION:
               switch(g_settings.input.dpad_emulation[controlno])
               {
                  case DPAD_EMULATION_NONE:
                     driver.input->set_analog_dpad_mapping(0, DPAD_EMULATION_LSTICK, controlno);
                     break;
                  case DPAD_EMULATION_LSTICK:
                     driver.input->set_analog_dpad_mapping(0, DPAD_EMULATION_RSTICK, controlno);
                     break;
                  case DPAD_EMULATION_RSTICK:
                     break;
               }
               break;
            case SETTING_CONTROLS_DEFAULT_ALL:
               break;
            default:
               rarch_input_set_keybind(controlno, KEYBIND_INCREMENT, current_index);
               snprintf(button, sizeof(button), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
               convert_char_to_wchar(strw_buffer, button, sizeof(strw_buffer));
               m_controlslist.SetText(current_index, strw_buffer);
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_UP:
      case XUI_CONTROL_NAVIGATE_DOWN:
         break;
   }

   snprintf(button, sizeof(button), "D-Pad Emulation: %s", rarch_dpad_emulation_name_lut[g_settings.input.dpad_emulation[controlno]]);
   convert_char_to_wchar(strw_buffer, button, sizeof(strw_buffer));
   m_controlslist.SetText(SETTING_CONTROLS_DPAD_EMULATION, strw_buffer);
   m_controlslist.SetText(SETTING_CONTROLS_DEFAULT_ALL, L"Reset all buttons to default");

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
         case SETTING_CONTROLS_DPAD_EMULATION:
            break;
         case SETTING_CONTROLS_DEFAULT_ALL:
            rarch_input_set_default_keybinds(0);

            for(i = 0; i < RARCH_FIRST_META_KEY; i++)
            {
               snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", 
                     rarch_input_get_default_keybind_name(i), controlno, 
                     rarch_input_find_platform_key_label(
                        g_settings.input.binds[controlno][i].joykey));
               convert_char_to_wchar(strw_buffer, buttons[i], sizeof(strw_buffer));
               m_controlslist.SetText(i, strw_buffer);
            }
            break;
         default:
            rarch_input_set_keybind(controlno, KEYBIND_DEFAULT, current_index);
            snprintf(buttons[current_index], sizeof(buttons[current_index]), "%s #%d: %s", rarch_input_get_default_keybind_name(current_index), controlno, rarch_input_find_platform_key_label(g_settings.input.binds[controlno][current_index].joykey));
            convert_char_to_wchar(strw_buffer, buttons[current_index], sizeof(strw_buffer));
            m_controlslist.SetText(current_index, strw_buffer);
            break;
      }
   }

   snprintf(buttons[current_index], sizeof(buttons[current_index]), "D-Pad Emulation: %s", rarch_dpad_emulation_name_lut[g_settings.input.dpad_emulation[controlno]]);
   convert_char_to_wchar(strw_buffer, buttons[current_index], sizeof(strw_buffer));
   m_controlslist.SetText(SETTING_CONTROLS_DPAD_EMULATION, strw_buffer);
   m_controlslist.SetText(SETTING_CONTROLS_DEFAULT_ALL, L"Reset all buttons to default");

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiSettingsList", &m_settingslist);
   GetChildById(L"XuiBackButton", &m_back);

   m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
   m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_extern.console.rmenu.state.msg_info.enable ? L"Info messages: ON" : L"Info messages: OFF");
   m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, g_extern.console.rmenu.state.msg_fps.enable ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
   m_settingslist.SetText(SETTING_EMU_MENUS, g_extern.console.rmenu.state.rmenu_hd.enable ? L"Menus: HD" : L"Menus: SD");
   m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Hardware filtering shader #1: Linear interpolation" : L"Hardware filtering shader #1: Point filtering");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER_2, g_settings.video.second_pass_smooth ? L"Hardware filtering shader #2: Linear interpolation" : L"Hardware filtering shader #2: Point filtering");
   m_settingslist.SetText(SETTING_SCALE_ENABLED, g_settings.video.render_to_texture ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SHADER, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER, strw_buffer);
   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SHADER_2, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER_2, strw_buffer);
   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ZIP_EXTRACT, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_ZIP_EXTRACT, strw_buffer);

   return 0;
}

HRESULT CRetroArchSettings::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
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

            if (g_extern.console.rmenu.state.msg_info.enable)
               rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
            break;
         case SETTING_EMU_SHOW_INFO_MSG:
            g_extern.console.rmenu.state.msg_info.enable = !g_extern.console.rmenu.state.msg_info.enable;
            m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_extern.console.rmenu.state.msg_info.enable ? L"Info messages: ON" : L"Info messages: OFF");
            break;
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            g_extern.console.rmenu.state.msg_fps.enable = !g_extern.console.rmenu.state.msg_fps.enable;
            m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, g_extern.console.rmenu.state.msg_fps.enable ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
            break;
         case SETTING_EMU_MENUS:
            g_extern.console.rmenu.state.rmenu_hd.enable = !g_extern.console.rmenu.state.rmenu_hd.enable;
            m_settingslist.SetText(SETTING_EMU_MENUS, g_extern.console.rmenu.state.rmenu_hd.enable ? L"Menus: HD" : L"Menus: SD");
            break;
         case SETTING_GAMMA_CORRECTION_ENABLED:
            g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
            m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
            if (g_extern.console.rmenu.state.msg_info.enable)
               rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
            break;
         case SETTING_SHADER:
            set_shader = 1;
            hr = XuiSceneCreate(g_extern.console.rmenu.state.rmenu_hd.enable ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            hCur = app.hShaderBrowser;

            if (g_extern.console.rmenu.state.msg_info.enable)
               rarch_settings_msg(S_MSG_SELECT_SHADER, S_DELAY_180);

            NavigateForward(app.hShaderBrowser);
            break;
         case SETTING_SHADER_2:
            set_shader = 2;
            hr = XuiSceneCreate(g_extern.console.rmenu.state.rmenu_hd.enable ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);
            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            hCur = app.hShaderBrowser;

            if (g_extern.console.rmenu.state.msg_info.enable)
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
            g_settings.video.render_to_texture = !g_settings.video.render_to_texture;
            m_settingslist.SetText(SETTING_SCALE_ENABLED, g_settings.video.render_to_texture ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");

            if(g_settings.video.render_to_texture)
               device_ptr->ctx_driver->set_fbo(FBO_INIT);
            else
               device_ptr->ctx_driver->set_fbo(FBO_DEINIT);
            break;
         case SETTING_ZIP_EXTRACT:
            if(g_extern.file_state.zip_extract_mode < ZIP_EXTRACT_TO_CACHE_DIR)
               g_extern.file_state.zip_extract_mode++;
            else
               g_extern.file_state.zip_extract_mode = 0;
            rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ZIP_EXTRACT, sizeof(strw_buffer));
            m_settingslist.SetText(SETTING_ZIP_EXTRACT, strw_buffer);
            break;
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchSettings::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   int current_index;
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   current_index = m_settingslist.GetCurSel();

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         switch(current_index)
         {
            case SETTING_EMU_REWIND_ENABLED:
               rarch_settings_change(S_REWIND);
               m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");

               if (g_extern.console.rmenu.state.msg_info.enable)
                  rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
            case SETTING_EMU_SHOW_INFO_MSG:
               g_extern.console.rmenu.state.msg_info.enable = !g_extern.console.rmenu.state.msg_info.enable;
               m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_extern.console.rmenu.state.msg_info.enable ? L"Info messages: ON" : L"Info messages: OFF");
               break;
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            g_extern.console.rmenu.state.msg_fps.enable = !g_extern.console.rmenu.state.msg_fps.enable;
            m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, g_extern.console.rmenu.state.msg_fps.enable ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
            break;
            case SETTING_EMU_MENUS:
               g_extern.console.rmenu.state.rmenu_hd.enable = !g_extern.console.rmenu.state.rmenu_hd.enable;
               m_settingslist.SetText(SETTING_EMU_MENUS, g_extern.console.rmenu.state.rmenu_hd.enable ? L"Menus: HD" : L"Menus: SD");
               break;
            case SETTING_GAMMA_CORRECTION_ENABLED:
               g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
               m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
               if (g_extern.console.rmenu.state.msg_info.enable)
                  rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
            case SETTING_SCALE_FACTOR:
               if(device_ptr->fbo_inited)
               {
                  if((g_settings.video.fbo.scale_x > MIN_SCALING_FACTOR))
                  {
                     rarch_settings_change(S_SCALE_FACTOR_DECREMENT);
                     device_ptr->ctx_driver->set_fbo(FBO_REINIT);
                     rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                     m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                  }
               }
               break;
            case SETTING_ZIP_EXTRACT:
               if(g_extern.file_state.zip_extract_mode)
                  g_extern.file_state.zip_extract_mode--;
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ZIP_EXTRACT, sizeof(strw_buffer));
               m_settingslist.SetText(SETTING_ZIP_EXTRACT, strw_buffer);
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
               g_settings.video.render_to_texture = !g_settings.video.render_to_texture;
               m_settingslist.SetText(SETTING_SCALE_ENABLED, g_settings.video.render_to_texture ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
               if(g_settings.video.render_to_texture)
                  device_ptr->ctx_driver->set_fbo(FBO_INIT);
               else
                  device_ptr->ctx_driver->set_fbo(FBO_DEINIT);
               break;
            default:
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         switch(current_index)
         {
            case SETTING_EMU_SHOW_INFO_MSG:
               g_extern.console.rmenu.state.msg_info.enable = !g_extern.console.rmenu.state.msg_info.enable;
               m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, g_extern.console.rmenu.state.msg_info.enable ? L"Info messages: ON" : L"Info messages: OFF");
               break;
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            g_extern.console.rmenu.state.msg_fps.enable = !g_extern.console.rmenu.state.msg_fps.enable;
            m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, g_extern.console.rmenu.state.msg_fps.enable ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
            break;
            case SETTING_EMU_MENUS:
               g_extern.console.rmenu.state.rmenu_hd.enable = !g_extern.console.rmenu.state.rmenu_hd.enable;
               m_settingslist.SetText(SETTING_EMU_MENUS, g_extern.console.rmenu.state.rmenu_hd.enable ? L"Menus: HD" : L"Menus: SD");
               break;
            case SETTING_GAMMA_CORRECTION_ENABLED:
               g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
               m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
               if (g_extern.console.rmenu.state.msg_info.enable)
                  rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
            case SETTING_EMU_REWIND_ENABLED:
               rarch_settings_change(S_REWIND);
               m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");

               if (g_extern.console.rmenu.state.msg_info.enable)
                  rarch_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
            case SETTING_SCALE_FACTOR:
               if(device_ptr->fbo_inited)
               {
                  if((g_settings.video.fbo.scale_x < MAX_SCALING_FACTOR))
                  {
                     rarch_settings_change(S_SCALE_FACTOR_INCREMENT);
                     device_ptr->ctx_driver->set_fbo(FBO_REINIT);
                     rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                     m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                  }
               }
               break;
            case SETTING_ZIP_EXTRACT:
               if(g_extern.file_state.zip_extract_mode < ZIP_EXTRACT_TO_CACHE_DIR)
                  g_extern.file_state.zip_extract_mode++;
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ZIP_EXTRACT, sizeof(strw_buffer));
               m_settingslist.SetText(SETTING_ZIP_EXTRACT, strw_buffer);
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
				g_settings.video.render_to_texture = !g_settings.video.render_to_texture;
               m_settingslist.SetText(SETTING_SCALE_ENABLED, g_settings.video.render_to_texture ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");

               if(g_settings.video.render_to_texture)
                  device_ptr->ctx_driver->set_fbo(FBO_INIT);
               else
                  device_ptr->ctx_driver->set_fbo(FBO_DEINIT);
               break;
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
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchQuickMenu::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiQuickMenuList", &m_quickmenulist);
   GetChildById(L"XuiBackButton", &m_back);

   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);

   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, strw_buffer);

   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);

   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);

   return 0;
}

HRESULT CRetroArchQuickMenu::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   bool aspectratio_changed = false;
   int current_index;

   current_index = m_quickmenulist.GetCurSel();

   switch(pControlNavigateData->nControlNavigate)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         switch(current_index)
         {
            case MENU_ITEM_LOAD_STATE:
            case MENU_ITEM_SAVE_STATE:
               rarch_state_slot_decrease();
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_ITEM_KEEP_ASPECT_RATIO:
               rarch_settings_change(S_ASPECT_RATIO_DECREMENT);
               aspectratio_changed = true;
               break;
            case MENU_ITEM_ORIENTATION:
               rarch_settings_change(S_ROTATION_DECREMENT);
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
               driver.video->set_rotation(driver.video_data, g_extern.console.screen.orientation);
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
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_LOAD_STATE, strw_buffer);
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_ITEM_KEEP_ASPECT_RATIO:
               rarch_settings_change(S_ASPECT_RATIO_INCREMENT);
               aspectratio_changed = true;
               break;
            case MENU_ITEM_ORIENTATION:
               rarch_settings_change(S_ROTATION_INCREMENT);
               rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
               driver.video->set_rotation(driver.video_data, g_extern.console.screen.orientation);
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
      driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
      rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
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
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchQuickMenu::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
   int current_index;

   if ( hObjPressed == m_quickmenulist)
   {
      current_index = m_quickmenulist.GetCurSel();

      switch(current_index)
      {
         case MENU_ITEM_LOAD_STATE:
            if (g_extern.main_is_init)
            {
               rarch_load_state();
               rarch_settings_change(S_RETURN_TO_GAME);
            }
            break;
         case MENU_ITEM_SAVE_STATE:
            if (g_extern.main_is_init)
            {
               rarch_save_state();
               rarch_settings_change(S_RETURN_TO_GAME);
            }
            break;
         case MENU_ITEM_KEEP_ASPECT_RATIO:
            rarch_settings_default(S_DEF_ASPECT_RATIO);
            driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
            rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
            m_quickmenulist.SetText(MENU_ITEM_KEEP_ASPECT_RATIO, strw_buffer);
            break;
         case MENU_ITEM_OVERSCAN_AMOUNT:
            if (g_extern.console.rmenu.state.msg_info.enable)
               rarch_settings_msg(S_MSG_NOT_IMPLEMENTED, S_DELAY_180);
            break;
         case MENU_ITEM_ORIENTATION:
            rarch_settings_default(S_DEF_ROTATION);
            rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
            m_quickmenulist.SetText(MENU_ITEM_ORIENTATION, strw_buffer);
            driver.video->set_rotation(driver.video_data, g_extern.console.screen.orientation);
            break;
         case MENU_ITEM_RESIZE_MODE:
            g_extern.console.rmenu.input_loop = INPUT_LOOP_RESIZE_MODE;

            if (g_extern.console.rmenu.state.msg_info.enable)
               rarch_settings_msg(S_MSG_RESIZE_SCREEN, S_DELAY_270);
            break;
         case MENU_ITEM_FRAME_ADVANCE:
            if (g_extern.main_is_init)
            {
               g_extern.lifecycle_state |= (1ULL << RARCH_FRAMEADVANCE);
               rarch_settings_change(S_FRAME_ADVANCE);
            }
            break;
         case MENU_ITEM_SCREENSHOT_MODE:
            if (g_extern.console.rmenu.state.msg_info.enable)
               device_ptr->ctx_driver->rmenu_screenshot_dump(NULL);
            break;
         case MENU_ITEM_RESET:
            if (g_extern.main_is_init)
            {
               rarch_settings_change(S_RETURN_TO_GAME);
               rarch_game_reset();
            }
            break;
         case MENU_ITEM_RETURN_TO_GAME:
            if (g_extern.main_is_init)
               rarch_settings_change(S_RETURN_TO_GAME);
            break;
         case MENU_ITEM_QUIT_RARCH:
            rarch_settings_change(S_QUIT_RARCH);
            break;
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_shaderlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_shaderpathtitle);

   filebrowser_set_root_and_ext(&tmp_browser, "cg|CG", "game:\\media\\shaders");
   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(&tmp_browser, action, &m_shaderlist, &m_shaderpathtitle);

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
         convert_wchar_to_char(str_buffer, (const wchar_t *)m_shaderlist.GetText(index), sizeof(str_buffer));

         switch(set_shader)
         {
            case 1:
               snprintf(g_settings.video.cg_shader_path, sizeof(g_settings.video.cg_shader_path), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), str_buffer);
               if (g_settings.video.shader_type != RARCH_SHADER_NONE)
               {
                  driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, g_settings.video.cg_shader_path, (1ULL << RARCH_SHADER_PASS0));
                  if (g_extern.console.rmenu.state.msg_info.enable)
                     rarch_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
               }
               else
                  RARCH_ERR("Shaders are unsupported on this platform.\n");
               break;
            case 2:
               snprintf (g_settings.video.second_pass_shader, sizeof(g_settings.video.second_pass_shader), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), str_buffer);
               if (g_settings.video.shader_type != RARCH_SHADER_NONE)
               {
                  driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, g_settings.video.second_pass_shader, (1ULL << RARCH_SHADER_PASS1));
                  if (g_extern.console.rmenu.state.msg_info.enable)
                     rarch_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
               }
               else
                  RARCH_ERR("Shaders are unsupported on this platform.\n");
               break;
            default:
               break;
         }
      }
      else if(tmp_browser.current_dir.list->elems[index].attr.b)
      {
         convert_wchar_to_char(str_buffer, (const wchar_t *)m_shaderlist.GetText(index), sizeof(str_buffer));
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), str_buffer);
         filebrowser_set_root_and_ext(&tmp_browser, "cg|CG", path);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_fetch_directory_entries(&tmp_browser, action, &m_shaderlist, &m_shaderpathtitle);
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_romlist);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_rompathtitle);

   filebrowser_set_root_and_ext(&tmp_browser, "xex|XEX", "game:");
   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(&tmp_browser, action, &m_romlist, &m_rompathtitle);

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   if(hObjPressed == m_romlist)
   {
      int index = m_romlist.GetCurSel();
      convert_wchar_to_char(str_buffer, (const wchar_t *)m_romlist.GetText(index), sizeof(str_buffer));
      if(path_file_exists(tmp_browser.current_dir.list->elems[index].data))
      {
         snprintf(g_extern.console.external_launch.launch_app, sizeof(g_extern.console.external_launch.launch_app), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), str_buffer);
         rarch_settings_change(S_RETURN_TO_LAUNCHER);
      }
      else if(tmp_browser.current_dir.list->elems[index].attr.b)
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(&tmp_browser), str_buffer);
         filebrowser_set_root_and_ext(&tmp_browser, "xex|XEX", path);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_fetch_directory_entries(&tmp_browser, action, &m_romlist, &m_rompathtitle);
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

   GetChildById(L"XuiLogo", &m_logoimage);
   GetChildById(L"XuiBtnRomBrowser", &m_filebrowser);
   GetChildById(L"XuiBtnSettings", &m_settings);
   GetChildById(L"XuiBtnQuickMenu", &m_quick_menu);
   GetChildById(L"XuiBtnControls", &m_controls);
   GetChildById(L"XuiBtnQuit", &m_quit);
   GetChildById(L"XuiTxtTitle", &m_title);
   GetChildById(L"XuiTxtCoreText", &m_core);
   GetChildById(L"XuiBtnLibretroCore", &m_change_libretro_core);

   char core_text[256];
   snprintf(core_text, sizeof(core_text), "%s %s", id, info.library_version);

   convert_char_to_wchar(strw_buffer, core_text, sizeof(strw_buffer));
   m_core.SetText(strw_buffer);
   rarch_settings_create_menu_item_label_w(strw_buffer, S_LBL_RARCH_VERSION, sizeof(strw_buffer));
   m_title.SetText(strw_buffer);

   g_extern.console.rmenu.input_loop = INPUT_LOOP_NONE;

   return 0;
}

HRESULT CRetroArchMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;

   bool hdmenus_allowed = g_extern.console.rmenu.state.rmenu_hd.enable;

   HRESULT hr;

   if ( hObjPressed == m_filebrowser )
   {
      g_extern.console.rmenu.input_loop = INPUT_LOOP_FILEBROWSER;
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
      {
         RARCH_ERR("Failed to load scene.\n");
      }

      hCur = app.hQuickMenu;
      NavigateForward(app.hQuickMenu);
   }
   else if ( hObjPressed == m_controls)
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_controls.xur", NULL, &app.hControlsMenu);

      if (hr < 0)
      {
         RARCH_ERR("Failed to load scene.\n");
      }

      hCur = app.hControlsMenu;

      if (g_extern.console.rmenu.state.msg_info.enable)
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

      if (g_extern.console.rmenu.state.msg_info.enable)
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
      rarch_settings_change(S_QUIT_RARCH);

   bHandled = TRUE;
   return 0;
}

void menu_init (void)
{
   HRESULT hr;

   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   bool hdmenus_allowed = g_extern.console.rmenu.state.rmenu_hd.enable;

   D3DPRESENT_PARAMETERS d3dpp;
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   video_info.rgb32 = false;

   xdk_d3d_generate_pp(&d3dpp, &video_info);

   hr = app.InitShared(device_ptr->d3d_render_device, &d3dpp, XuiPNGTextureLoader);

   if (hr < 0)
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      return;
   }

   /* Register font */
   hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game:/media/rarch.ttf" );
   if (hr < 0)
   {
      RARCH_ERR("Failed to register default typeface.\n");
      return;
   }

   hr = app.LoadSkin( L"file://game:/media/rarch_scene_skin.xur");
   if (hr < 0)
   {
      RARCH_ERR("Failed to load skin.\n");
      return;
   }

   hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_main.xur", NULL, &app.hMainScene);
   if (hr < 0)
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      return;
   }

   hCur = app.hMainScene;
   XuiSceneNavigateFirst(app.GetRootObj(), app.hMainScene, XUSER_INDEX_FOCUS);

   filebrowser_new(&browser, g_extern.console.main_wrap.paths.default_rom_startup_dir, rarch_console_get_rom_ext());
}

void menu_free (void)
{
   filebrowser_free(&browser);
   filebrowser_free(&tmp_browser);
   app.Uninit();
}

static void ingame_menu_resize (void)
{
   XINPUT_STATE state;

   XInputGetState(0, &state);

   if(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT || state.Gamepad.sThumbLX < -DEADZONE)
      g_extern.console.screen.viewports.custom_vp.x -= 1;
   else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT || state.Gamepad.sThumbLX > DEADZONE)
      g_extern.console.screen.viewports.custom_vp.x += 1;

   if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP || state.Gamepad.sThumbLY > DEADZONE)
      g_extern.console.screen.viewports.custom_vp.y += 1;
   else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN || state.Gamepad.sThumbLY < -DEADZONE) 
      g_extern.console.screen.viewports.custom_vp.y -= 1;

   if (state.Gamepad.sThumbRX < -DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
      g_extern.console.screen.viewports.custom_vp.width -= 1;
   else if (state.Gamepad.sThumbRX > DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
      g_extern.console.screen.viewports.custom_vp.width += 1;

   if (state.Gamepad.sThumbRY > DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
      g_extern.console.screen.viewports.custom_vp.height += 1;
   else if (state.Gamepad.sThumbRY < -DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
      g_extern.console.screen.viewports.custom_vp.height -= 1;

   if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
   {
      g_extern.console.screen.viewports.custom_vp.x = 0;
      g_extern.console.screen.viewports.custom_vp.y = 0;
      g_extern.console.screen.viewports.custom_vp.width = 1280; //FIXME: hardcoded
      g_extern.console.screen.viewports.custom_vp.height = 720; //FIXME: hardcoded
   }
   if(state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
      g_extern.console.rmenu.input_loop = INPUT_LOOP_MENU;
}

bool rmenu_iterate(void)
{
   static bool preinit = true;
   const char *msg;

   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   if(preinit)
   {
      g_extern.console.rmenu.input_loop = INPUT_LOOP_MENU;
      g_extern.draw_menu = true;
      preinit = false;
   }

   g_extern.frame_count++;

   XINPUT_STATE state;
   XInputGetState(0, &state);


   if (!(g_extern.frame_count < g_extern.delay_timer))
   {
      bool rmenu_enable = !((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) 
            && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) && (g_extern.main_is_init));

      switch(g_extern.console.rmenu.mode)
      {
         case MODE_EXIT:
         case MODE_INIT:
         case MODE_EMULATION:
            break;
         default:
            g_extern.console.rmenu.mode = rmenu_enable ? MODE_EMULATION : MODE_MENU;
            break;
      }
   }

   rarch_render_cached_frame();

   switch(g_extern.console.rmenu.input_loop)
   {
      case INPUT_LOOP_FILEBROWSER:
      case INPUT_LOOP_MENU:
         app.RunFrame(); /* Update XUI */
         if((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) && hCur != app.hMainScene)
            XuiSceneNavigateBack(hCur, app.hMainScene, XUSER_INDEX_ANY);
         break;
      case INPUT_LOOP_RESIZE_MODE:
         ingame_menu_resize();
         break;
      default:
         break;
   }

   if(g_extern.console.rmenu.mode != MODE_MENU)
      goto deinit;

   msg = msg_queue_pull(g_extern.msg_queue);

   if (msg)
      device_ptr->font_ctx->render_msg(device_ptr, msg);

   device_ptr->ctx_driver->swap_buffers();

   return true;

deinit:
   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding L3 + R3 in the emulation loop (lasts for 30 frame ticks)
   if(!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer = g_extern.frame_count + 30;

   g_extern.console.rmenu.state.ingame_menu.enable = false;
   g_extern.draw_menu = false;
   preinit = true;

   return false;
}
