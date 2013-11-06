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
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>

#include "rmenu_xui.h"

#include "file_browser.h"

#include "../../console/rarch_console.h"

#include "../../gfx/gfx_common.h"
#include "../../gfx/gfx_context.h"

#include "../../message_queue.h"
#include "../../general.h"

enum {
   MENU_XUI_ITEM_HW_TEXTURE_FILTER = 0,
   MENU_XUI_ITEM_GAMMA_CORRECTION_ENABLED,
   MENU_XUI_ITEM_ASPECT_RATIO,
   MENU_XUI_ITEM_RESIZE_MODE,
   MENU_XUI_ITEM_ORIENTATION,
};

enum {
   MENU_XUI_ITEM_AUDIO_MUTE_AUDIO = 0,
};

enum
{
   INGAME_MENU_REWIND_ENABLED = 0,
   INGAME_MENU_REWIND_GRANULARITY,
   SETTING_EMU_SHOW_DEBUG_INFO_MSG,
};

enum
{
   S_LBL_ASPECT_RATIO = 0,
   S_LBL_RARCH_VERSION,
   S_LBL_ROTATION,
   S_LBL_LOAD_STATE_SLOT,
   S_LBL_SAVE_STATE_SLOT,
   S_LBL_REWIND_GRANULARITY,
};

HXUIOBJ m_menulist;
HXUIOBJ m_menutitle;
HXUIOBJ m_menutitlebottom;
HXUIOBJ m_back;
HXUIOBJ root_menu;
HXUIOBJ current_menu;

class CRetroArch : public CXuiModule
{
   protected:
      virtual HRESULT RegisterXuiClasses();
      virtual HRESULT UnregisterXuiClasses();
};

#define CREATE_CLASS(class_type, class_name) \
class class_type: public CXuiSceneImpl \
{ \
   public: \
      HRESULT OnInit( XUIMessageInit* pInitData, int & bHandled ); \
      HRESULT OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled ); \
      HRESULT OnControlNavigate (XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled); \
 \
      HRESULT DispatchMessageMap(XUIMessage *pMessage) \
      { \
         if (pMessage->dwMessage == XM_INIT) \
         { \
            XUIMessageInit *pData = (XUIMessageInit *) pMessage->pvData; \
            return OnInit(pData, pMessage->bHandled); \
         } \
         if (pMessage->dwMessage == XM_CONTROL_NAVIGATE) \
         { \
           XUIMessageControlNavigate *pData = (XUIMessageControlNavigate *)pMessage->pvData; \
           return OnControlNavigate(pData, pMessage->bHandled); \
         } \
         if (pMessage->dwMessage == XM_NOTIFY) \
         { \
            XUINotify *pNotify = (XUINotify *) pMessage->pvData; \
            if (pNotify->dwNotify == XN_PRESS) \
               return OnNotifyPress(pNotify->hObjSource, pMessage->bHandled); \
         } \
         return __super::DispatchMessageMap(pMessage); \
      } \
 \
      static HRESULT Register() \
      { \
         HXUICLASS hClass; \
         XUIClass cls; \
         memset(&cls, 0x00, sizeof(cls)); \
         cls.szClassName = class_name; \
         cls.szBaseClassName = XUI_CLASS_SCENE; \
         cls.Methods.CreateInstance = (PFN_CREATEINST) (CreateInstance); \
         cls.Methods.DestroyInstance = (PFN_DESTROYINST) DestroyInstance; \
         cls.Methods.ObjectProc = (PFN_OBJECT_PROC) _ObjectProc; \
         cls.pPropDefs = _GetPropDef(&cls.dwPropDefCount); \
         HRESULT hr = XuiRegisterClass(&cls, &hClass); \
         if (FAILED(hr)) \
            return hr; \
         return S_OK; \
      } \
 \
      static HRESULT APIENTRY CreateInstance(HXUIOBJ hObj, void **ppvObj) \
      { \
         *ppvObj = NULL; \
         class_type *pThis = new class_type(); \
         if (!pThis) \
            return E_OUTOFMEMORY; \
         pThis->m_hObj = hObj; \
         HRESULT hr = pThis->OnCreate(); \
         if (FAILED(hr)) \
         { \
            DestroyInstance(pThis); \
            return hr; \
         } \
         *ppvObj = pThis; \
         return S_OK; \
      } \
 \
      static HRESULT APIENTRY DestroyInstance(void *pvObj) \
      { \
         class_type *pThis = (class_type *) pvObj; \
         delete pThis; \
         return S_OK; \
      } \
}

CREATE_CLASS(CRetroArchMain, L"RetroArchMain");
CREATE_CLASS(CRetroArchFileBrowser, L"RetroArchFileBrowser");
CREATE_CLASS(CRetroArchCoreBrowser, L"RetroArchCoreBrowser");
CREATE_CLASS(CRetroArchShaderBrowser, L"RetroArchShaderBrowser");
CREATE_CLASS(CRetroArchVideoOptions, L"RetroArchVideoOptions");
CREATE_CLASS(CRetroArchAudioOptions, L"RetroArchAudioOptions");
CREATE_CLASS(CRetroArchCoreOptions, L"RetroArchCoreOptions");
CREATE_CLASS(CRetroArchSettings, L"RetroArchSettings");
CREATE_CLASS(CRetroArchControls, L"RetroArchControls");
CREATE_CLASS(CRetroArchLoadGameHistory, L"RetroArchLoadGameHistory");

CRetroArch app;

wchar_t strw_buffer[PATH_MAX];
char str_buffer[PATH_MAX];

static int process_input_ret = 0;

/* Register custom classes */
HRESULT CRetroArch::RegisterXuiClasses (void)
{
   CRetroArchMain::Register();
   CRetroArchFileBrowser::Register();
   CRetroArchCoreBrowser::Register();
   CRetroArchShaderBrowser::Register();
   CRetroArchVideoOptions::Register();
   CRetroArchAudioOptions::Register();
   CRetroArchCoreOptions::Register();
   CRetroArchControls::Register();
   CRetroArchSettings::Register();
   CRetroArchLoadGameHistory::Register();

   return 0;
}

/* Unregister custom classes */
HRESULT CRetroArch::UnregisterXuiClasses (void)
{
   XuiUnregisterClass(L"RetroArchMain");
   XuiUnregisterClass(L"RetroArchCoreBrowser");
   XuiUnregisterClass(L"RetroArchShaderBrowser");
   XuiUnregisterClass(L"RetroArchFileBrowser");
   XuiUnregisterClass(L"RetroArchVideoOptions");
   XuiUnregisterClass(L"RetroArchAudioOptions");
   XuiUnregisterClass(L"RetroArchCoreOptions");
   XuiUnregisterClass(L"RetroArchControls");
   XuiUnregisterClass(L"RetroArchSettings");
   XuiUnregisterClass(L"RetroArchLoadGameHistory");

   return 0;
}

static void menu_settings_create_menu_item_label_w(wchar_t *strwbuf, unsigned setting, size_t size)
{
   char str[PATH_MAX];

   switch (setting)
   {
      case S_LBL_ASPECT_RATIO:
         snprintf(str, size, "Aspect Ratio: %s", aspectratio_lut[g_settings.video.aspect_ratio_idx].name);
         break;
      case S_LBL_RARCH_VERSION:
         snprintf(str, size, "RetroArch %s", PACKAGE_VERSION);
         break;
      case S_LBL_ROTATION:
         snprintf(str, size, "Rotation: %s", rotation_lut[g_settings.video.rotation]);
         break;
      case S_LBL_LOAD_STATE_SLOT:
         snprintf(str, size, "Load State #%d", g_extern.state_slot);
         break;
      case S_LBL_SAVE_STATE_SLOT:
         snprintf(str, size, "Save State #%d", g_extern.state_slot);
         break;
      case S_LBL_REWIND_GRANULARITY:
         snprintf(str, size, "Rewind granularity: %d", g_settings.rewind_granularity);
         break;
   }

   mbstowcs(strwbuf, str, size / sizeof(wchar_t));
}

void filebrowser_fetch_directory_entries(uint64_t action)
{
   filebrowser_iterate(rgui->browser, action); 

   mbstowcs(strw_buffer, rgui->browser->current_dir.directory_path, sizeof(strw_buffer) / sizeof(wchar_t));
   XuiTextElementSetText(m_menutitle, strw_buffer);

   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));
   XuiListInsertItems(m_menulist, 0, rgui->browser->list->size);

   for(unsigned i = 0; i < rgui->browser->list->size; i++)
   {
      char fname_tmp[256];
      fill_pathname_base(fname_tmp, rgui->browser->list->elems[i].data, sizeof(fname_tmp));
      mbstowcs(strw_buffer, fname_tmp, sizeof(strw_buffer) / sizeof(wchar_t));
      XuiListSetText(m_menulist, i, strw_buffer);
   }
}

HRESULT CRetroArchFileBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   filebrowser_set_root_and_ext(rgui->browser, rgui->info.valid_extensions,
         default_paths.filebrowser_startup_dir);

   filebrowser_fetch_directory_entries(RGUI_ACTION_OK);

   return 0;
}

HRESULT CRetroArchFileBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];
   process_input_ret = 0;

   if(hObjPressed == m_menulist)
   {
      int index = XuiListGetCurSel(m_menulist, NULL);
      wcstombs(str_buffer, (const wchar_t *)XuiListGetText(m_menulist, index), sizeof(str_buffer));
      if (path_file_exists(rgui->browser->list->elems[index].data))
      {
         fill_pathname_join(g_extern.fullpath, rgui->browser->current_dir.directory_path, str_buffer, sizeof(g_extern.fullpath));
         g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);
         process_input_ret = -1;
      }
      else if(rgui->browser->list->elems[index].attr.b)
      {
         fill_pathname_join(path, rgui->browser->current_dir.directory_path, str_buffer, sizeof(path));
         filebrowser_set_root_and_ext(rgui->browser, rgui->info.valid_extensions, path);
         filebrowser_fetch_directory_entries(RGUI_ACTION_OK);
      }
   }

   bHandled = TRUE;

   return 0;
}

static void set_dpad_emulation_label(unsigned port, char *str, size_t sizeof_str)
{
}

static void init_menulist(unsigned menu_id)
{
   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));

   switch (menu_id)
   {
      case INGAME_MENU_CORE_OPTIONS_MODE:
   if (g_extern.system.core_options)
   {
      size_t opts = core_option_size(g_extern.system.core_options);
      for (size_t i = 0; i < opts; i++)
      {
         char label[256];
         strlcpy(label, core_option_get_desc(g_extern.system.core_options, i),
            sizeof(label));
         snprintf(label, sizeof(label), "%s : %s", label,
            core_option_get_val(g_extern.system.core_options, i));
         mbstowcs(strw_buffer, label,
            sizeof(strw_buffer) / sizeof(wchar_t));
         XuiListInsertItems(m_menulist, i, 1);
         XuiListSetText(m_menulist, i, strw_buffer);
      }
   }
   else
   {
      XuiListInsertItems(m_menulist, 0, 1);
      XuiListSetText(m_menulist, 0, L"No options available.");
   }
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
         {
            size_t history_size = rom_history_size(rgui->history);

            if (history_size)
            {
               size_t opts = history_size;
               for (size_t i = 0; i < opts; i++)
               {
                  const char *path = NULL;
                  const char *core_path = NULL;
                  const char *core_name = NULL;

                  rom_history_get_index(rgui->history, i,
                        &path, &core_path, &core_name);

                  char path_short[PATH_MAX];
                  fill_pathname(path_short, path_basename(path), "", sizeof(path_short));

                  char fill_buf[PATH_MAX];
                  snprintf(fill_buf, sizeof(fill_buf), "%s (%s)",
                        path_short, core_name);

                  mbstowcs(strw_buffer, fill_buf, sizeof(strw_buffer) / sizeof(wchar_t));
                  XuiListInsertItems(m_menulist, i, 1);
                  XuiListSetText(m_menulist, i, strw_buffer);
               }
            }
            else
            {
               XuiListInsertItems(m_menulist, 0, 1);
               XuiListSetText(m_menulist, 0, L"No history available.");
            }
         }
         break;
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         {
            unsigned i;
            char buttons[RARCH_FIRST_META_KEY][128];
            unsigned keybind_end = RETRO_DEVICE_ID_JOYPAD_R3 + 1;

            for(i = 0; i < keybind_end; i++)
            {
               struct platform_bind key_label;
               strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
               key_label.joykey = g_settings.input.binds[rgui->current_pad][i].joykey;

               if (driver.input->set_keybinds)
                  driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

               snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", 
                     g_settings.input.binds[rgui->current_pad][i].desc,
                     rgui->current_pad, key_label.desc);
               mbstowcs(strw_buffer, buttons[i], sizeof(strw_buffer) / sizeof(wchar_t));
               XuiListInsertItems(m_menulist, i, 1);
               XuiListSetText(m_menulist, i, strw_buffer);
            }

            //set_dpad_emulation_label(rgui->current_pad, buttons[0], sizeof(buttons[0]));
            //mbstowcs(strw_buffer, buttons[0], sizeof(strw_buffer) / sizeof(wchar_t));
            XuiListInsertItems(m_menulist, keybind_end, 1);
            //XuiListSetText(m_menulist, SETTING_CONTROLS_DPAD_EMULATION, strw_buffer);
            XuiListSetText(m_menulist, SETTING_CONTROLS_DPAD_EMULATION, L"Stub");

            XuiListInsertItems(m_menulist, keybind_end + 1, 1);
            XuiListSetText(m_menulist, SETTING_CONTROLS_DEFAULT_ALL, L"Reset all buttons to default");
         }
         break;
      case INGAME_MENU_SETTINGS_MODE:
         XuiListInsertItems(m_menulist, INGAME_MENU_REWIND_ENABLED, 1);
         XuiListSetText(m_menulist, INGAME_MENU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
         XuiListInsertItems(m_menulist, INGAME_MENU_REWIND_GRANULARITY, 1);
         XuiListSetText(m_menulist, INGAME_MENU_REWIND_GRANULARITY, strw_buffer);

         XuiListInsertItems(m_menulist, SETTING_EMU_SHOW_DEBUG_INFO_MSG, 1);
         XuiListSetText(m_menulist, SETTING_EMU_SHOW_DEBUG_INFO_MSG, (g_settings.fps_show) ? L"Show Framerate: ON" : L"Show Framerate: OFF");
         break;
      case INGAME_MENU_MAIN_MODE:
         XuiListInsertItems(m_menulist, INGAME_MENU_CHANGE_LIBRETRO_CORE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_CHANGE_LIBRETRO_CORE, L"Core ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_LOAD_GAME_HISTORY_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_LOAD_GAME_HISTORY_MODE, L"Load Game (History) ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_CHANGE_GAME, 1);
         XuiListSetText(m_menulist, INGAME_MENU_CHANGE_GAME, L"Load Game ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_CORE_OPTIONS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_CORE_OPTIONS_MODE, L"Core Options ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_VIDEO_OPTIONS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_VIDEO_OPTIONS_MODE, L"Video Options ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_AUDIO_OPTIONS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_AUDIO_OPTIONS_MODE, L"Audio Options ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_INPUT_OPTIONS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_INPUT_OPTIONS_MODE, L"Input Options ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_PATH_OPTIONS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_PATH_OPTIONS_MODE, L"Path Options ...");

         XuiListInsertItems(m_menulist, INGAME_MENU_SETTINGS_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_SETTINGS_MODE, L"Settings ...");

         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
         XuiListInsertItems(m_menulist, INGAME_MENU_LOAD_STATE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_LOAD_STATE, strw_buffer);

         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
         XuiListInsertItems(m_menulist, INGAME_MENU_SAVE_STATE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_SAVE_STATE, strw_buffer);

         XuiListInsertItems(m_menulist, INGAME_MENU_SCREENSHOT_MODE, 1);
         XuiListSetText(m_menulist, INGAME_MENU_SCREENSHOT_MODE, L"Take Screenshot");

         XuiListInsertItems(m_menulist, INGAME_MENU_RETURN_TO_GAME, 1);
         XuiListSetText(m_menulist, INGAME_MENU_RETURN_TO_GAME, L"Resume Game");

         XuiListInsertItems(m_menulist, INGAME_MENU_RESET, 1);
         XuiListSetText(m_menulist, INGAME_MENU_RESET, L"Restart Game");

         XuiListInsertItems(m_menulist, INGAME_MENU_QUIT_RETROARCH, 1);
         XuiListSetText(m_menulist, INGAME_MENU_QUIT_RETROARCH, L"Quit RetroArch");
         break;
   }
}

static unsigned xui_input_to_rgui_action(unsigned input)
{
   switch (input)
   {
      case XUI_CONTROL_NAVIGATE_LEFT:
         return RGUI_ACTION_LEFT;
      case XUI_CONTROL_NAVIGATE_RIGHT:
         return RGUI_ACTION_RIGHT;
      case XUI_CONTROL_NAVIGATE_UP:
         return RGUI_ACTION_UP;
      case XUI_CONTROL_NAVIGATE_DOWN:
         return RGUI_ACTION_DOWN;
      case XUI_CONTROL_NAVIGATE_OK:
         return RGUI_ACTION_OK;
   }

   return RGUI_ACTION_NOOP;
}

HRESULT CRetroArchLoadGameHistory::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{

   unsigned current_index = XuiListGetCurSel(m_menulist, NULL);
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   if (action == RGUI_ACTION_OK)
   {
      load_menu_game_history(current_index);
      process_input_ret = -1;
   }

   bHandled = TRUE;

   switch (action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}


HRESULT CRetroArchControls::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiTextElementSetText(m_menutitle, L"Input options");

   init_menulist(INGAME_MENU_INPUT_OPTIONS_MODE);

   return 0;
}

HRESULT CRetroArchFileBrowser::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   bHandled = TRUE;
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);
   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);
   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}



HRESULT CRetroArchControls::OnControlNavigate(
      XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   char button[128];
   char buttons[RARCH_FIRST_META_KEY][128];
   int i, current_index;

   for(i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      struct platform_bind key_label;
      strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
      key_label.joykey = g_settings.input.binds[rgui->current_pad][i].joykey;

      if (driver.input->set_keybinds)
         driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));

      snprintf(buttons[i], sizeof(buttons[i]), "%s #%d: %s", 
            g_settings.input.binds[rgui->current_pad][i].desc, rgui->current_pad, 
            key_label.desc);
      mbstowcs(strw_buffer, buttons[i], sizeof(strw_buffer) / sizeof(wchar_t));
      XuiListSetText(m_menulist, i, strw_buffer);
   }
   
   current_index = XuiListGetCurSel(m_menulist, NULL);
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   switch(current_index)
   {
      case SETTING_CONTROLS_DPAD_EMULATION:
         break;
      case SETTING_CONTROLS_DEFAULT_ALL:
         break;
      default:
         break;
   }

   //set_dpad_emulation_label(rgui->current_pad, button, sizeof(button));

   //mbstowcs(strw_buffer, button, sizeof(strw_buffer) / sizeof(wchar_t));
   //XuiListSetText(m_menulist, SETTING_CONTROLS_DPAD_EMULATION, strw_buffer);
   XuiListSetText(m_menulist, SETTING_CONTROLS_DPAD_EMULATION, L"Stub");
   XuiListSetText(m_menulist, SETTING_CONTROLS_DEFAULT_ALL, L"Reset all buttons to default");

   return 0;
}

HRESULT CRetroArchLoadGameHistory::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchControls::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchLoadGameHistory::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiTextElementSetText(m_menutitle, L"Load History");

   init_menulist(INGAME_MENU_LOAD_GAME_HISTORY_MODE);

   return 0;
}

HRESULT CRetroArchSettings::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiTextElementSetText(m_menutitle, L"Settings");

   init_menulist(INGAME_MENU_SETTINGS_MODE);

   return 0;
}

HRESULT CRetroArchSettings::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   int current_index;
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   current_index = XuiListGetCurSel(m_menulist, NULL);

   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   switch(current_index)
   {
      case INGAME_MENU_REWIND_ENABLED:
         menu_set_settings(RGUI_SETTINGS_REWIND_ENABLE, action);
         XuiListSetText(m_menulist, INGAME_MENU_REWIND_ENABLED, g_settings.rewind_enable ? L"Rewind: ON" : L"Rewind: OFF");
         break;
      case INGAME_MENU_REWIND_GRANULARITY:
         menu_set_settings(RGUI_SETTINGS_REWIND_GRANULARITY, action);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_REWIND_GRANULARITY, sizeof(strw_buffer));
         XuiListSetText(m_menulist, INGAME_MENU_REWIND_GRANULARITY, strw_buffer);
         break;
      case SETTING_EMU_SHOW_DEBUG_INFO_MSG:
         menu_set_settings(RGUI_SETTINGS_DEBUG_TEXT, action);
         XuiListSetText(m_menulist, SETTING_EMU_SHOW_DEBUG_INFO_MSG, g_settings.fps_show ? L"Show Framerate: ON" : L"Show Framerate: OFF");
         break;
      default:
         break;
   }

   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchSettings::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   process_input_ret = 0;

   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreOptions::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));

   XuiTextElementSetText(m_menutitle, L"Core Options");

   init_menulist(INGAME_MENU_CORE_OPTIONS_MODE);

   return 0;
}

HRESULT CRetroArchCoreOptions::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   process_input_ret = 0;

   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreOptions::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   unsigned current_index = XuiListGetCurSel(m_menulist, NULL);
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   if (g_extern.system.core_options)
   {
      bool update_item = false;

      switch (action)
      {
         case RGUI_ACTION_LEFT:
            core_option_prev(g_extern.system.core_options,  current_index);
            update_item = true;
            break;
         case RGUI_ACTION_RIGHT:
            core_option_next(g_extern.system.core_options,  current_index);
            update_item = true;
            break;
      }

      if (update_item)
      {
         char label[256];
         strlcpy(label, core_option_get_desc(g_extern.system.core_options, current_index),
            sizeof(label));
         snprintf(label, sizeof(label), "%s : %s", label,
            core_option_get_val(g_extern.system.core_options, current_index));
         mbstowcs(strw_buffer, label,
            sizeof(strw_buffer) / sizeof(wchar_t));
         XuiListSetText(m_menulist, current_index, strw_buffer);
      }
   }

   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchAudioOptions::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));

   XuiTextElementSetText(m_menutitle, L"Audio Options");

   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_AUDIO_MUTE_AUDIO, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_AUDIO_MUTE_AUDIO, g_extern.audio_data.mute ? L"Mute Audio : ON" : L"Mute Audio : OFF");

   return 0;
}

HRESULT CRetroArchAudioOptions::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   int current_index = XuiListGetCurSel(m_menulist, NULL);
   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   switch (current_index)
   {
      case MENU_XUI_ITEM_AUDIO_MUTE_AUDIO:
         menu_set_settings(RGUI_SETTINGS_AUDIO_MUTE, action);
         XuiListSetText(m_menulist, MENU_XUI_ITEM_AUDIO_MUTE_AUDIO, g_extern.audio_data.mute ? L"Mute Audio : ON" : L"Mute Audio : OFF");
         break;
   }

   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchAudioOptions::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   process_input_ret = 0;

   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchVideoOptions::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiBackButton", &m_back);
   GetChildById(L"XuiTxtTitle", &m_menutitle);

   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));

   XuiTextElementSetText(m_menutitle, L"Video Options");

   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_HW_TEXTURE_FILTER, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Default Filter: Linear" : L"Default Filter: Nearest");

   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_GAMMA_CORRECTION_ENABLED, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma Correction: ON" : L"Gamma correction: OFF");

   if (driver.video_poke->set_aspect_ratio)
      driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

   menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_ASPECT_RATIO, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);

   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_RESIZE_MODE, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_RESIZE_MODE, L"Custom Ratio ...");

   driver.video->set_rotation(driver.video_data, g_settings.video.rotation);
   menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
   XuiListInsertItems(m_menulist, MENU_XUI_ITEM_ORIENTATION, 1);
   XuiListSetText(m_menulist, MENU_XUI_ITEM_ORIENTATION, strw_buffer);

   return 0;
}

HRESULT CRetroArchVideoOptions::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   bool aspectratio_changed = false;
   int current_index;

   current_index = XuiListGetCurSel(m_menulist, NULL);

   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   switch (current_index)
   {
      case MENU_XUI_ITEM_HW_TEXTURE_FILTER:
         menu_set_settings(RGUI_SETTINGS_VIDEO_FILTER, action);
         XuiListSetText(m_menulist, MENU_XUI_ITEM_HW_TEXTURE_FILTER, g_settings.video.smooth ? L"Default Filter: Linear" : L"Default Filter: Nearest");
         break;
      case MENU_XUI_ITEM_GAMMA_CORRECTION_ENABLED:
         if (action == RGUI_ACTION_LEFT ||
               action == RGUI_ACTION_RIGHT ||
               action == RGUI_ACTION_OK)
         {
            g_extern.console.screen.gamma_correction = g_extern.console.screen.gamma_correction ? 0 : 1;
            XuiListSetText(m_menulist, MENU_XUI_ITEM_GAMMA_CORRECTION_ENABLED, g_extern.console.screen.gamma_correction ? L"Gamma correction: ON" : L"Gamma correction: OFF");
            msg_queue_push(g_extern.msg_queue, "You need to restart for this change to take effect.\n", 1, 90);
         }
         break;
      case MENU_XUI_ITEM_ASPECT_RATIO:
         menu_set_settings(RGUI_SETTINGS_VIDEO_ASPECT_RATIO, action);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ASPECT_RATIO, sizeof(strw_buffer));
         XuiListSetText(m_menulist, MENU_XUI_ITEM_ASPECT_RATIO, strw_buffer);
         break;
      case MENU_XUI_ITEM_ORIENTATION:
         menu_set_settings(RGUI_SETTINGS_VIDEO_ROTATION, action);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_ROTATION, sizeof(strw_buffer));
         XuiListSetText(m_menulist, MENU_XUI_ITEM_ORIENTATION, strw_buffer);
         driver.video->set_rotation(driver.video_data, g_settings.video.rotation);
         break;
   }

   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchVideoOptions::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   process_input_ret = 0;

   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   filebrowser_set_root_and_ext(rgui->browser, "cg", "game:\\media\\shaders");
   filebrowser_fetch_directory_entries(RGUI_ACTION_OK);

   return 0;
}

HRESULT CRetroArchShaderBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];
   process_input_ret = 0;

   if(hObjPressed == m_menulist)
   {
      int index = XuiListGetCurSel(m_menulist, NULL);
      if (path_file_exists(rgui->browser->list->elems[index].data))
         wcstombs(str_buffer, (const wchar_t *)XuiListGetText(m_menulist, index), sizeof(str_buffer));
      else if (rgui->browser->list->elems[index].attr.b)
      {
         wcstombs(str_buffer, (const wchar_t *)XuiListGetText(m_menulist, index), sizeof(str_buffer));
         fill_pathname_join(path, rgui->browser->current_dir.directory_path, str_buffer, sizeof(path));
         filebrowser_set_root_and_ext(rgui->browser, "cg", path);
         filebrowser_fetch_directory_entries(RGUI_ACTION_OK);
      }
   }

   bHandled = TRUE;

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   filebrowser_set_root_and_ext(rgui->browser, "xex|XEX", "game:");
   filebrowser_fetch_directory_entries(RGUI_ACTION_OK);

   return 0;
}

HRESULT CRetroArchCoreBrowser::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
   char path[PATH_MAX];

   process_input_ret = 0;

   if(hObjPressed == m_menulist)
   {
      int index = XuiListGetCurSel(m_menulist, NULL);
      wcstombs(str_buffer, (const wchar_t *)XuiListGetText(m_menulist, index), sizeof(str_buffer));
      if(path_file_exists(rgui->browser->list->elems[index].data))
      {
         fill_pathname_join(path, rgui->browser->current_dir.directory_path, str_buffer, sizeof(path));
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)path);

         g_extern.lifecycle_mode_state |= (1ULL << MODE_EXITSPAWN);
         process_input_ret = -1;
      }
      else if (rgui->browser->list->elems[index].attr.b)
      {
         fill_pathname_join(path, rgui->browser->current_dir.directory_path, str_buffer, sizeof(path));
         filebrowser_set_root_and_ext(rgui->browser, "xex|XEX", path);
         filebrowser_fetch_directory_entries(RGUI_ACTION_OK);
      }
   }

   bHandled = TRUE;
   return 0;
}

HRESULT CRetroArchMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   init_menulist(INGAME_MENU_MAIN_MODE);

   mbstowcs(strw_buffer, g_extern.title_buf, sizeof(strw_buffer) / sizeof(wchar_t));
   XuiTextElementSetText(m_menutitlebottom, strw_buffer);
   menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_RARCH_VERSION, sizeof(strw_buffer));
   XuiTextElementSetText(m_menutitle, strw_buffer);

   return 0;
}

HRESULT CRetroArchMain::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   HRESULT hr;
   int current_index;
   bool hdmenus_allowed = (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU_HD));

   current_index= XuiListGetCurSel(m_menulist, NULL);

   unsigned input = pControlNavigateData->nControlNavigate;
   unsigned action = xui_input_to_rgui_action(input);

   HXUIOBJ current_obj = current_menu;

   switch (current_index)
   {
      case INGAME_MENU_CHANGE_LIBRETRO_CORE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_libretrocore_browser.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_LOAD_GAME_HISTORY_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_load_game_history.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_CHANGE_GAME:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_filebrowser.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_CORE_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_core_options.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_VIDEO_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_video_options.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_AUDIO_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_audio_options.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_INPUT_OPTIONS_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_controls.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_PATH_OPTIONS_MODE:
         break;
      case INGAME_MENU_SETTINGS_MODE:
         if (action == RGUI_ACTION_OK)
         {
            hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_settings.xur", NULL, &current_menu);

            if (hr < 0)
               RARCH_ERR("Failed to load scene.\n");

            XuiSceneNavigateForward(current_obj, false, current_menu, XUSER_INDEX_FOCUS);
         }
         break;
      case INGAME_MENU_LOAD_STATE:
         process_input_ret = menu_set_settings(RGUI_SETTINGS_SAVESTATE_LOAD, action);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
         XuiListSetText(m_menulist, INGAME_MENU_LOAD_STATE, strw_buffer);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
         XuiListSetText(m_menulist, INGAME_MENU_SAVE_STATE, strw_buffer);
         break;
      case INGAME_MENU_SAVE_STATE:
         process_input_ret = menu_set_settings(RGUI_SETTINGS_SAVESTATE_SAVE, action);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_LOAD_STATE_SLOT, sizeof(strw_buffer));
         XuiListSetText(m_menulist, INGAME_MENU_LOAD_STATE, strw_buffer);
         menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_SAVE_STATE_SLOT, sizeof(strw_buffer));
         XuiListSetText(m_menulist, INGAME_MENU_SAVE_STATE, strw_buffer);
         break;
      case INGAME_MENU_SCREENSHOT_MODE:
         break;
      case INGAME_MENU_RETURN_TO_GAME:
         process_input_ret = menu_set_settings(RGUI_SETTINGS_RESUME_GAME, action);
         break;
      case INGAME_MENU_RESET:
         process_input_ret = menu_set_settings(RGUI_SETTINGS_RESTART_GAME, action);
         break;
      case INGAME_MENU_QUIT_RETROARCH:
         process_input_ret = menu_set_settings(RGUI_SETTINGS_QUIT_RARCH, action);
         break;
   }

   bHandled = TRUE;

   switch(action)
   {
      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_UP:
      case RGUI_ACTION_DOWN:
         pControlNavigateData->hObjDest = pControlNavigateData->hObjSource;
         break;
      default:
         break;
   }

   return 0;
}

HRESULT CRetroArchMain::OnNotifyPress( HXUIOBJ hObjPressed,  int & bHandled )
{
   if ( hObjPressed == m_menulist)
   {
      XUIMessageControlNavigate controls;
      controls.nControlNavigate = (XUI_CONTROL_NAVIGATE)XUI_CONTROL_NAVIGATE_OK;
      OnControlNavigate(&controls, bHandled);
   }

   bHandled = TRUE;
   return 0;
}

static void* rgui_init (void)
{
   HRESULT hr;

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));
   if (rgui == NULL)
   {
      RARCH_ERR("Could not allocate RGUI handle.\n");
      return NULL;
   }

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
      free(rgui);
      return NULL;
   }

   /* Register font */
   TypefaceDescriptor typeface = {0};
   typeface.szTypeface = L"Arial Unicode MS";
   typeface.szLocator = L"file://game:/media/rarch.ttf";
   typeface.szReserved1 = NULL;

   hr = XuiRegisterTypeface( &typeface, TRUE );
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to register default typeface.\n");
      free(rgui);
      return NULL;
   }

   hr = XuiLoadVisualFromBinary( L"file://game:/media/rarch_scene_skin.xur", NULL);
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to load skin.\n");
      free(rgui);
      return NULL;
   }

   hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_main.xur", NULL, &root_menu);
   if (hr != S_OK)
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      free(rgui);
      return NULL;
   }

   current_menu = root_menu;
   hr = XuiSceneNavigateFirst(app.GetRootObj(), current_menu, XUSER_INDEX_FOCUS);
   if (hr != S_OK)
   {
      RARCH_ERR("XuiSceneNavigateFirst failed.\n");
      free(rgui);
      return NULL;
   }

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_frame(driver.video_data, NULL,
            true, 0, 0, 1.0f);

   return rgui;
}

static void rgui_free(void *data)
{
   (void)data;
   app.Uninit();
}

static void ingame_menu_resize (void)
{
}

int rmenu_xui_iterate(void *data, unsigned action)
{
   (void)data;

   if (action == RGUI_ACTION_OK)
   {
      XuiSceneNavigateBack(current_menu, root_menu, XUSER_INDEX_ANY);
      current_menu = root_menu;
      XuiElementGetChildById(current_menu, L"XuiMenuList", &m_menulist);
      init_menulist(INGAME_MENU_MAIN_MODE);
   }

   return 0;
}

bool menu_iterate_xui(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   app.RunFrame(); /* Update XUI */

   XuiRenderBegin( app.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

   D3DXMATRIX matOrigView;
   XuiRenderGetViewTransform( app.GetDC(), &matOrigView );

   XUIMessage msg;
   XUIMessageRender msgRender;
   XuiMessageRender( &msg, &msgRender, app.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
   XuiSendMessage( app.GetRootObj(), &msg );

   XuiRenderSetViewTransform( app.GetDC(), &matOrigView );

   XuiRenderEnd( app.GetDC() );
   XuiTimersRun();
   return true;
}

int rgui_input_postprocess(void *data, uint64_t old_state)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   bool quit = false;
   bool resize = false;
   unsigned width;
   unsigned height;
   unsigned frame_count;

   if ((rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
      g_extern.main_is_init)
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      process_input_ret = -1;
   }

   if (quit)
      process_input_ret = -1;

   int process_input_ret_old = process_input_ret; 
   process_input_ret = 0;

   return process_input_ret_old;
}

const menu_ctx_driver_t menu_ctx_rmenu_xui = {
   NULL,
   NULL,
   NULL,
   rgui_init,
   rgui_free,
   "rmenu_xui",
};
