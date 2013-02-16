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

#include "rmenu_xui.h"

#include "utils/file_browser.h"

#include "../../console/rarch_console.h"
#include "rmenu_settings.h"
#include "../../console/rarch_console_video.h"

#include "../../gfx/gfx_context.h"

#include "../../message.h"

#include "../../general.h"

enum {
   MENU_XUI_ITEM_LOAD_STATE = 0,
   MENU_XUI_ITEM_SAVE_STATE,
   MENU_XUI_ITEM_ASPECT_RATIO,
   MENU_XUI_ITEM_ORIENTATION,
   MENU_XUI_ITEM_RESIZE_MODE,
   MENU_XUI_ITEM_FRAME_ADVANCE,
   MENU_XUI_ITEM_SCREENSHOT_MODE,
   MENU_XUI_ITEM_RESET,
   MENU_XUI_ITEM_RETURN_TO_GAME,
   MENU_XUI_ITEM_QUIT_RARCH,
};

CRetroArch app;
CXuiList m_list;
CXuiTextElement m_list_path;
HXUIOBJ hCur;
filebrowser_t *browser;
filebrowser_t *tmp_browser;

wchar_t strw_buffer[PATH_MAX];
char str_buffer[PATH_MAX];

static int process_input_ret = 0;
static unsigned input_loop = 0;

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

static void browser_update(filebrowser_t * b, uint64_t input, const char *extensions);

static void filebrowser_fetch_directory_entries(filebrowser_t * browser, uint64_t action)
{
   CXuiList *romlist = &m_list;
   CXuiTextElement *rompath_title = &m_list_path;
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
   else if (input & (1ULL << RMENU_DEVICE_NAV_SELECT))
   {
      action = FILEBROWSER_ACTION_RESET;
      filebrowser_set_root_and_ext(b, g_extern.system.valid_extensions,
            g_extern.console.main_wrap.default_rom_startup_dir);
      strlcpy(b->extensions, extensions, sizeof(b->extensions));
      filebrowser_fetch_directory_entries(browser, (1ULL << RMENU_DEVICE_NAV_B));
   }

   if(action != FILEBROWSER_ACTION_NOOP)
      ret = filebrowser_iterate(b, action);

   if(!ret)
      rmenu_settings_msg(S_MSG_DIR_LOADING_ERROR, S_DELAY_180);
}

HRESULT CRetroArchFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_list);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_list_path);
   GetChildById(L"XuiBtnGameDir", &m_dir_game);

   filebrowser_set_root_and_ext(browser, g_extern.system.valid_extensions, default_paths.filebrowser_startup_dir);

   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(browser, action);

   return 0;
}

HRESULT CRetroArchFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];
   process_input_ret = 0;

   if(hObjPressed == m_list)
   {
      int index = m_list.GetCurSel();
      convert_wchar_to_char(str_buffer, (const wchar_t *)m_list.GetText(index), sizeof(str_buffer));
      if(path_file_exists(browser->current_dir.list->elems[index].data))
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(browser), str_buffer);
         strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));
         g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
      }
      else if(browser->current_dir.list->elems[index].attr.b)
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(browser), str_buffer);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_set_root_and_ext(browser, g_extern.system.valid_extensions, path);
         filebrowser_fetch_directory_entries(browser, action);
      }
   }
   else if (hObjPressed == m_dir_game)
   {
      filebrowser_set_root_and_ext(browser, g_extern.system.valid_extensions,
            g_extern.console.main_wrap.default_rom_startup_dir);
      uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
      filebrowser_fetch_directory_entries(browser, action);
   }

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
   process_input_ret = 0;

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
   m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? L"Info messages: ON" : L"Info messages: OFF");
   m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
   m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
   m_settingslist.SetText(SETTING_AUDIO_RESAMPLER_TYPE, strstr(g_settings.audio.resampler, "sinc") ? L"Audio Resampler: Sinc" : L"Audio Resampler: Hermite");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Hardware filtering shader #1: Linear interpolation" : L"Hardware filtering shader #1: Point filtering");
   m_settingslist.SetText(SETTING_HW_TEXTURE_FILTER_2, g_settings.video.second_pass_smooth ? L"Hardware filtering shader #2: Linear interpolation" : L"Hardware filtering shader #2: Point filtering");
   m_settingslist.SetText(SETTING_SCALE_ENABLED, g_settings.video.render_to_texture ? L"Custom Scaling/Dual Shaders: ON" : L"Custom Scaling/Dual Shaders: OFF");
   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SHADER, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER, strw_buffer);
   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SHADER_2, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SHADER_2, strw_buffer);
   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
   m_settingslist.SetText(SETTING_EMU_REWIND_GRANULARITY, strw_buffer);
   m_settingslist.SetText(SETTING_ENABLE_SRAM_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? L"SRAM Path Enable: ON" : L"SRAM Path Enable: OFF");
   m_settingslist.SetText(SETTING_ENABLE_STATE_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? L"Savestate Path Enable: ON" : L"Savestate Path Enable: OFF");

   return 0;
}

HRESULT CRetroArchSettings::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
   int current_index;
   HRESULT hr;
   process_input_ret = 0;

   if ( hObjPressed == m_settingslist)
   {
      current_index = m_settingslist.GetCurSel();

      switch(current_index)
      {
         case SETTING_EMU_REWIND_ENABLED:
            rmenu_settings_set(S_REWIND);
            m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               rmenu_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
            break;
	 case SETTING_EMU_REWIND_GRANULARITY:
	    g_settings.rewind_granularity++;

	    rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
	    m_settingslist.SetText(SETTING_EMU_REWIND_GRANULARITY, strw_buffer);
	    break;
     case SETTING_ENABLE_SRAM_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_SRAM_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? L"SRAM Path Enable: ON" : L"SRAM Path Enable: OFF");
        break;
     case SETTING_ENABLE_STATE_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_STATE_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? L"Savestate Path Enable: ON" : L"Savestate Path Enable: OFF");
        break;
         case SETTING_EMU_SHOW_INFO_MSG:
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INFO_DRAW);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_INFO_DRAW);
            m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? L"Info messages: ON" : L"Info messages: OFF");
            break;
         case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
            m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
            break;
         case SETTING_AUDIO_RESAMPLER_TYPE:
#ifdef HAVE_SINC
            if( strstr(g_settings.audio.resampler, "hermite"))
               snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "sinc");
            else
#endif
               snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "hermite");
	    m_settingslist.SetText(SETTING_AUDIO_RESAMPLER_TYPE, strstr(g_settings.audio.resampler, "sinc") ? L"Audio Resampler: Sinc" : L"Audio Resampler: Hermite");

            if (g_extern.main_is_init)
            {
               if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                        g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
               {
                  RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                  g_extern.audio_active = false;
               }
            }
            break;
         case SETTING_GAMMA_CORRECTION_ENABLED:
            g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
            driver.video->restart();
            m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
            break;
         case SETTING_SHADER:
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_FIRST_SHADER);
            hr = XuiSceneCreate((g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD)) ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            hCur = app.hShaderBrowser;

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               rmenu_settings_msg(S_MSG_SELECT_SHADER, S_DELAY_180);

            NavigateForward(app.hShaderBrowser);
            break;
         case SETTING_SHADER_2:
            g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_SECOND_SHADER);
            hr = XuiSceneCreate((g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD)) ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_shader_browser.xur", NULL, &app.hShaderBrowser);
            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            hCur = app.hShaderBrowser;

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               rmenu_settings_msg(S_MSG_SELECT_SHADER, S_DELAY_180);

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
               rmenu_settings_set(S_REWIND);
               m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");

               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  rmenu_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
	    case SETTING_EMU_REWIND_GRANULARITY:
	       if (g_settings.rewind_granularity > 1)
		       g_settings.rewind_granularity--;

	       rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
	       m_settingslist.SetText(SETTING_EMU_REWIND_GRANULARITY, strw_buffer);
	       break;
     case SETTING_ENABLE_SRAM_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_SRAM_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? L"SRAM Path Enable: ON" : L"SRAM Path Enable: OFF");
        break;
     case SETTING_ENABLE_STATE_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_STATE_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? L"Savestate Path Enable: ON" : L"Savestate Path Enable: OFF");
        break;
            case SETTING_EMU_SHOW_INFO_MSG:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INFO_DRAW);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_INFO_DRAW);
               m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? L"Info messages: ON" : L"Info messages: OFF");
               break;
            case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
               m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
               break;
            case SETTING_AUDIO_RESAMPLER_TYPE:
#ifdef HAVE_SINC
               if( strstr(g_settings.audio.resampler, "hermite"))
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "sinc");
               else
#endif
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "hermite");
	       m_settingslist.SetText(SETTING_AUDIO_RESAMPLER_TYPE, strstr(g_settings.audio.resampler, "sinc") ? L"Audio Resampler: Sinc" : L"Audio Resampler: Hermite");

               if (g_extern.main_is_init)
               {
                  if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                           g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
                  {
                     RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                     g_extern.audio_active = false;
                  }
               }
               break;
            case SETTING_GAMMA_CORRECTION_ENABLED:
               g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
               driver.video->restart();
               m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
               break;
            case SETTING_SCALE_FACTOR:
               if(device_ptr->fbo_inited)
               {
                  if((g_settings.video.fbo.scale_x > MIN_SCALING_FACTOR))
                  {
                     rmenu_settings_set(S_SCALE_FACTOR_DECREMENT);
                     device_ptr->ctx_driver->set_fbo(FBO_REINIT);
                     rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                     m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                  }
               }
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
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INFO_DRAW);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_INFO_DRAW);
               m_settingslist.SetText(SETTING_EMU_SHOW_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW)) ? L"Info messages: ON" : L"Info messages: OFF");
               break;
            case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
               if (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW))
                  g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
               else
                  g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
               m_settingslist.SetText(SETTING_EMU_SHOW_DEBUG_INFO_MSG, (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? L"Debug Info messages: ON" : L"Debug Info messages: OFF");
               break;
            case SETTING_AUDIO_RESAMPLER_TYPE:
#ifdef HAVE_SINC
               if( strstr(g_settings.audio.resampler, "hermite"))
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "sinc");
               else
#endif
                  snprintf(g_settings.audio.resampler, sizeof(g_settings.audio.resampler), "hermite");
	       m_settingslist.SetText(SETTING_AUDIO_RESAMPLER_TYPE, strstr(g_settings.audio.resampler, "sinc") ? L"Audio Resampler: Sinc" : L"Audio Resampler: Hermite");

               if (g_extern.main_is_init)
               {
                  if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
                           g_settings.audio.resampler, g_extern.audio_data.orig_src_ratio == 0.0 ? 1.0 : g_extern.audio_data.orig_src_ratio))
                  {
                     RARCH_ERR("Failed to initialize resampler \"%s\".\n", g_settings.audio.resampler);
                     g_extern.audio_active = false;
                  }
               }
               break;
            case SETTING_GAMMA_CORRECTION_ENABLED:
               g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
               driver.video->restart();
               m_settingslist.SetText(SETTING_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
               break;
            case SETTING_EMU_REWIND_ENABLED:
               rmenu_settings_set(S_REWIND);
               m_settingslist.SetText(SETTING_EMU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");

               if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                  rmenu_settings_msg(S_MSG_RESTART_RARCH, S_DELAY_180);
               break;
	    case SETTING_EMU_REWIND_GRANULARITY:
	       g_settings.rewind_granularity++;

	       rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
	       m_settingslist.SetText(SETTING_EMU_REWIND_GRANULARITY, strw_buffer);
	       break;
     case SETTING_ENABLE_SRAM_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_SRAM_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_SRAM_DIR_ENABLE)) ? L"SRAM Path Enable: ON" : L"SRAM Path Enable: OFF");
        break;
     case SETTING_ENABLE_STATE_PATH:
        if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE))
           g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
        else
           g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE);
	    m_settingslist.SetText(SETTING_ENABLE_STATE_PATH, (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME_STATE_DIR_ENABLE)) ? L"Savestate Path Enable: ON" : L"Savestate Path Enable: OFF");
        break;
            case SETTING_SCALE_FACTOR:
               if(device_ptr->fbo_inited)
               {
                  if((g_settings.video.fbo.scale_x < MAX_SCALING_FACTOR))
                  {
                     rmenu_settings_set(S_SCALE_FACTOR_INCREMENT);
                     device_ptr->ctx_driver->set_fbo(FBO_REINIT);
                     rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SCALE_FACTOR, sizeof(strw_buffer));
                     m_settingslist.SetText(SETTING_SCALE_FACTOR, strw_buffer);
                  }
               }
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

   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_XUI_ITEM_ORIENTATION, strw_buffer);

   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);

   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_XUI_ITEM_LOAD_STATE, strw_buffer);

   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
   m_quickmenulist.SetText(MENU_XUI_ITEM_SAVE_STATE, strw_buffer);

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
            case MENU_XUI_ITEM_LOAD_STATE:
            case MENU_XUI_ITEM_SAVE_STATE:
               rarch_state_slot_decrease();
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_LOAD_STATE, strw_buffer);
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_XUI_ITEM_ASPECT_RATIO:
               rmenu_settings_set(S_ASPECT_RATIO_DECREMENT);
               aspectratio_changed = true;
               break;
            case MENU_XUI_ITEM_ORIENTATION:
               rmenu_settings_set(S_ROTATION_DECREMENT);
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_ORIENTATION, strw_buffer);
               driver.video->set_rotation(driver.video_data, g_extern.console.screen.orientation);
               break;
            default:
               break;
         }
         break;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         switch(current_index)
         {
            case MENU_XUI_ITEM_LOAD_STATE:
            case MENU_XUI_ITEM_SAVE_STATE:
               rarch_state_slot_increase();
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_LOAD_STATE, strw_buffer);
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_SAVE_STATE, strw_buffer);
               break;
            case MENU_XUI_ITEM_ASPECT_RATIO:
               rmenu_settings_set(S_ASPECT_RATIO_INCREMENT);
               aspectratio_changed = true;
               break;
            case MENU_XUI_ITEM_ORIENTATION:
               rmenu_settings_set(S_ROTATION_INCREMENT);
               rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
               m_quickmenulist.SetText(MENU_XUI_ITEM_ORIENTATION, strw_buffer);
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
      rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
      m_quickmenulist.SetText(MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);
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
   int current_index = 0;
   process_input_ret = 0;

   if ( hObjPressed == m_quickmenulist)
   {
      current_index = m_quickmenulist.GetCurSel();

      switch(current_index)
      {
         case MENU_XUI_ITEM_LOAD_STATE:
            if (g_extern.main_is_init)
            {
               rarch_load_state();
               g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
               process_input_ret = -1;
            }
            break;
         case MENU_XUI_ITEM_SAVE_STATE:
            if (g_extern.main_is_init)
            {
               rarch_save_state();
               g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
               process_input_ret = -1;
            }
            break;
         case MENU_XUI_ITEM_ASPECT_RATIO:
            rmenu_settings_set_default(S_DEF_ASPECT_RATIO);
            driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
            rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
            m_quickmenulist.SetText(MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);
            break;
         case MENU_XUI_ITEM_ORIENTATION:
            rmenu_settings_set_default(S_DEF_ROTATION);
            rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
            m_quickmenulist.SetText(MENU_XUI_ITEM_ORIENTATION, strw_buffer);
            driver.video->set_rotation(driver.video_data, g_extern.console.screen.orientation);
            break;
         case MENU_XUI_ITEM_RESIZE_MODE:
            input_loop = INPUT_LOOP_RESIZE_MODE;
            g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;
            rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
            m_quickmenulist.SetText(MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);

            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               rmenu_settings_msg(S_MSG_RESIZE_SCREEN, S_DELAY_270);
            break;
         case MENU_XUI_ITEM_FRAME_ADVANCE:
            if (g_extern.main_is_init)
            {
               g_extern.lifecycle_state |= (1ULL << RARCH_FRAMEADVANCE);
               rmenu_settings_set(S_FRAME_ADVANCE);
               process_input_ret = -1;
            }
            break;
         case MENU_XUI_ITEM_SCREENSHOT_MODE:
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
               device_ptr->ctx_driver->rmenu_screenshot_dump(NULL);
            break;
         case MENU_XUI_ITEM_RESET:
            if (g_extern.main_is_init)
            {
               rarch_game_reset();
               g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
               process_input_ret = -1;
            }
            break;
         case MENU_XUI_ITEM_RETURN_TO_GAME:
            if (g_extern.main_is_init)
            {
               g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
               process_input_ret = -1;
            }
            break;
         case MENU_XUI_ITEM_QUIT_RARCH:
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
            g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
            process_input_ret = -1;
            break;
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_list);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_list_path);

   filebrowser_set_root_and_ext(tmp_browser, "cg|CG", "game:\\media\\shaders");
   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(tmp_browser, action);

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];
   process_input_ret = 0;

   if(hObjPressed == m_list)
   {
      int index = m_list.GetCurSel();
      if(path_file_exists(tmp_browser->current_dir.list->elems[index].data))
      {
         convert_wchar_to_char(str_buffer, (const wchar_t *)m_list.GetText(index), sizeof(str_buffer));

         if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_FIRST_SHADER))
         {
               snprintf(g_settings.video.cg_shader_path, sizeof(g_settings.video.cg_shader_path), "%s\\%s", filebrowser_get_current_dir(tmp_browser), str_buffer);
               if (g_settings.video.shader_type != RARCH_SHADER_NONE)
               {
                  driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, g_settings.video.cg_shader_path, RARCH_SHADER_INDEX_PASS0);
                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                     rmenu_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
                  XuiSceneNavigateBack(hCur, app.hMainScene, XUSER_INDEX_ANY);
               }
               else
                  RARCH_ERR("Shaders are unsupported on this platform.\n");
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_FIRST_SHADER);
         }

         if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_SECOND_SHADER))
         {
               snprintf (g_settings.video.second_pass_shader, sizeof(g_settings.video.second_pass_shader), "%s\\%s", filebrowser_get_current_dir(tmp_browser), str_buffer);
               if (g_settings.video.shader_type != RARCH_SHADER_NONE)
               {
                  driver.video->set_shader(driver.video_data, (enum rarch_shader_type)g_settings.video.shader_type, g_settings.video.second_pass_shader, RARCH_SHADER_INDEX_PASS1);
                  if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
                     rmenu_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
               }
               else
                  RARCH_ERR("Shaders are unsupported on this platform.\n");
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_SECOND_SHADER);
         }
      }
      else if(tmp_browser->current_dir.list->elems[index].attr.b)
      {
         convert_wchar_to_char(str_buffer, (const wchar_t *)m_list.GetText(index), sizeof(str_buffer));
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(tmp_browser), str_buffer);
         filebrowser_set_root_and_ext(tmp_browser, "cg|CG", path);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_fetch_directory_entries(tmp_browser, action);
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiRomList", &m_list);
   GetChildById(L"XuiBackButton1", &m_back);
   GetChildById(L"XuiTxtRomPath", &m_list_path);

   filebrowser_set_root_and_ext(tmp_browser, "xex|XEX", "game:");
   uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
   filebrowser_fetch_directory_entries(tmp_browser, action);

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   process_input_ret = 0;

   if(hObjPressed == m_list)
   {
      int index = m_list.GetCurSel();
      convert_wchar_to_char(str_buffer, (const wchar_t *)m_list.GetText(index), sizeof(str_buffer));
      if(path_file_exists(tmp_browser->current_dir.list->elems[index].data))
      {
         snprintf(g_extern.fullpath, sizeof(g_extern.fullpath), "%s\\%s", filebrowser_get_current_dir(tmp_browser), str_buffer);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
         process_input_ret = -1;
      }
      else if(tmp_browser->current_dir.list->elems[index].attr.b)
      {
         snprintf(path, sizeof(path), "%s\\%s", filebrowser_get_current_dir(tmp_browser), str_buffer);
         filebrowser_set_root_and_ext(tmp_browser, "xex|XEX", path);
         uint64_t action = (1ULL << RMENU_DEVICE_NAV_B);
         filebrowser_fetch_directory_entries(tmp_browser, action);
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiLogo", &m_logoimage);
   GetChildById(L"XuiBtnRomBrowser", &m_filebrowser);
   GetChildById(L"XuiBtnSettings", &m_settings);
   GetChildById(L"XuiBtnQuickMenu", &m_quick_menu);
   GetChildById(L"XuiBtnControls", &m_controls);
   GetChildById(L"XuiBtnQuit", &m_quit);
   GetChildById(L"XuiTxtTitle", &m_title);
   GetChildById(L"XuiTxtCoreText", &m_core);
   GetChildById(L"XuiBtnLibretroCore", &m_change_libretro_core);

   convert_char_to_wchar(strw_buffer, g_extern.title_buf, sizeof(strw_buffer));
   m_core.SetText(strw_buffer);
   rmenu_settings_create_menu_item_label_w(strw_buffer, S_LBL_RARCH_VERSION, sizeof(strw_buffer));
   m_title.SetText(strw_buffer);

   return 0;
}

HRESULT CRetroArchMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;

   bool hdmenus_allowed = (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD));

   HRESULT hr;

   if ( hObjPressed == m_filebrowser )
   {
      input_loop = INPUT_LOOP_FILEBROWSER;
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

      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         rmenu_settings_msg(S_MSG_CHANGE_CONTROLS, S_DELAY_180);

      NavigateForward(app.hControlsMenu);
   }
   else if ( hObjPressed == m_change_libretro_core )
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_libretrocore_browser.xur", NULL, &app.hCoreBrowser);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");
      hCur = app.hCoreBrowser;

      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         rmenu_settings_msg(S_MSG_SELECT_LIBRETRO_CORE, S_DELAY_180);

      NavigateForward(app.hCoreBrowser);
   }
   else if (hObjPressed == m_settings)
   {
      hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_settings.xur", NULL, &app.hRetroArchSettings);

      if (hr < 0)
         RARCH_ERR("Failed to load scene.\n");

      hCur = app.hRetroArchSettings;
      NavigateForward(app.hRetroArchSettings);
   }
   else if (hObjPressed == m_quit)
   {
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_GAME);
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
      process_input_ret = -1;
   }

   bHandled = TRUE;
   return 0;
}

void menu_init (void)
{
   HRESULT hr;

   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   bool hdmenus_allowed = (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD));

   if (hdmenus_allowed)
      RARCH_LOG("HD menus enabled.\n");

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

   if (hr != S_OK)
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      return;
   }

   /* Register font */
   hr = app.RegisterDefaultTypeface(L"Arial Unicode MS", L"file://game:/media/rarch.ttf" );
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to register default typeface.\n");
      return;
   }

   hr = app.LoadSkin( L"file://game:/media/rarch_scene_skin.xur");
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to load skin.\n");
      return;
   }

   hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_main.xur", NULL, &app.hMainScene);
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      return;
   }

   hCur = app.hMainScene;
   hr = XuiSceneNavigateFirst(app.GetRootObj(), app.hMainScene, XUSER_INDEX_FOCUS);
   if (hr != S_OK)
   {
      RARCH_ERR("XuiSceneNavigateFirst failed.\n");
      return;
   }

   browser = (filebrowser_t*)filebrowser_init(default_paths.filebrowser_startup_dir, g_extern.system.valid_extensions);
   tmp_browser = (filebrowser_t*)filebrowser_init(default_paths.filebrowser_startup_dir, "");
}

void menu_free (void)
{
   filebrowser_free(browser);
   filebrowser_free(tmp_browser);
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
      input_loop = INPUT_LOOP_MENU;
}

bool rmenu_iterate(void)
{
   const char *msg;

   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_PREINIT))
   {
      input_loop = INPUT_LOOP_MENU;
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_PREINIT);
   }

   g_extern.frame_count++;

   XINPUT_STATE state;
   XInputGetState(0, &state);

   if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         rmenu_settings_msg(S_MSG_LOADING_ROM, 100);

      g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      process_input_ret = -1;
   }


   if (!(g_extern.frame_count < g_extern.delay_timer[0]))
   {
      bool rmenu_enable = ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) 
            && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) && (g_extern.main_is_init));

      if (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
         if (rmenu_enable)
         {
            g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
            process_input_ret = -1;
         }
   }

   rarch_render_cached_frame();

   switch(input_loop)
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

   msg = msg_queue_pull(g_extern.msg_queue);

   if (msg)
      device_ptr->font_ctx->render_msg(device_ptr, msg);

   device_ptr->ctx_driver->swap_buffers();

   if(process_input_ret != 0)
      goto deinit;

   return true;

deinit:
   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding L3 + R3 in the emulation loop (lasts for 30 frame ticks)
   if(!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer[0] = g_extern.frame_count + 30;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_INGAME);
   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);

   process_input_ret = 0;

   return false;
}
