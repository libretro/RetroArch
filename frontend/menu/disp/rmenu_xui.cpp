/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../menu_common.h"

#include "../../../gfx/gfx_common.h"
#include "../../../gfx/gfx_context.h"

#include "../../../message_queue.h"
#include "../../../general.h"

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
   INPUT_LOOP_RESIZE_MODE,
   INPUT_LOOP_FILEBROWSER
};

enum
{
    INGAME_MENU_CHANGE_LIBRETRO_CORE = 0,
    INGAME_MENU_LOAD_GAME_HISTORY_MODE,
    INGAME_MENU_CHANGE_GAME,
    INGAME_MENU_CORE_OPTIONS_MODE,
    INGAME_MENU_VIDEO_OPTIONS_MODE,
    INGAME_MENU_AUDIO_OPTIONS_MODE,
    INGAME_MENU_INPUT_OPTIONS_MODE,
    INGAME_MENU_PATH_OPTIONS_MODE,
    INGAME_MENU_SETTINGS_MODE,
    INGAME_MENU_LOAD_STATE,
    INGAME_MENU_SAVE_STATE,
    INGAME_MENU_SCREENSHOT_MODE,
    INGAME_MENU_RETURN_TO_GAME,
    INGAME_MENU_RESET,
    INGAME_MENU_QUIT_RETROARCH,
    INGAME_MENU_MAIN_MODE,
};

#define XUI_CONTROL_NAVIGATE_OK (XUI_CONTROL_NAVIGATE_RIGHT + 1)

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

CRetroArch app;

wchar_t strw_buffer[PATH_MAX];
char str_buffer[PATH_MAX];

static int process_input_ret = 0;

/* Register custom classes */
HRESULT CRetroArch::RegisterXuiClasses (void)
{
   CRetroArchMain::Register();

   return 0;
}

/* Unregister custom classes */
HRESULT CRetroArch::UnregisterXuiClasses (void)
{
   XuiUnregisterClass(L"RetroArchMain");

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

static void set_dpad_emulation_label(unsigned port, char *str, size_t sizeof_str)
{
}

static void rmenu_xui_populate_entries(void *data, unsigned menu_type)
{
   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));

   switch (menu_type)
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

static unsigned xui_input_to_rmenu_xui_action(unsigned input)
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

HRESULT CRetroArchMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   rmenu_xui_populate_entries(NULL, INGAME_MENU_MAIN_MODE);

   mbstowcs(strw_buffer, g_extern.title_buf, sizeof(strw_buffer) / sizeof(wchar_t));
   XuiTextElementSetText(m_menutitlebottom, strw_buffer);
   menu_settings_create_menu_item_label_w(strw_buffer, S_LBL_RARCH_VERSION, sizeof(strw_buffer));
   XuiTextElementSetText(m_menutitle, strw_buffer);

   return 0;
}

HRESULT CRetroArchMain::OnControlNavigate(XUIMessageControlNavigate *pControlNavigateData, BOOL& bHandled)
{
   bHandled = TRUE;

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

static void* rmenu_xui_init (void)
{
   HRESULT hr;

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));
   if (rgui == NULL)
   {
      RARCH_ERR("Could not allocate RGUI handle.\n");
      return NULL;
   }

   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;

   bool hdmenus_allowed = (g_extern.lifecycle_state & (1ULL << MODE_MENU_HD));

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

static void rmenu_xui_free(void *data)
{
   (void)data;
   app.Uninit();
}

static void ingame_menu_resize (void)
{
}

void rmenu_xui_iterate(void *data, unsigned action)
{
   (void)data;

   if (action == RGUI_ACTION_OK)
   {
      XuiSceneNavigateBack(current_menu, root_menu, XUSER_INDEX_ANY);
      current_menu = root_menu;
      XuiElementGetChildById(current_menu, L"XuiMenuList", &m_menulist);
      rmenu_xui_populate_entries(NULL, INGAME_MENU_MAIN_MODE);
   }
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

static int rmenu_xui_input_postprocess(void *data, uint64_t old_state)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   bool quit = false;
   bool resize = false;
   unsigned width;
   unsigned height;
   unsigned frame_count;

   (void)width;
   (void)height;
   (void)frame_count;

   if ((rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
      g_extern.main_is_init)
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      process_input_ret = -1;
   }

   if (quit)
      process_input_ret = -1;

   int process_input_ret_old = process_input_ret; 
   process_input_ret = 0;

   return process_input_ret_old;
}

static void blit_line(rgui_handle_t *rgui,
int x, int y, const char *message, bool green)
{
}

static void rmenu_xui_render_background(rgui_handle_t *rgui)
{
   (void)rgui;
}

static void rmenu_xui_render_messagebox(void *data, const char *message)
{
   (void)data;
   (void)message;
}

static void rmenu_xui_render(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (rgui->need_refresh && 
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   size_t begin = rgui->selection_ptr;
   size_t end = rgui->selection_buf->size;

   rmenu_xui_render_background(rgui);

   char title[256];
   const char *dir = NULL;
   unsigned menu_type = 0;
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (menu_type == RGUI_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION %s", dir);
   else if (menu_type == RGUI_SETTINGS_DEFERRED_CORE)
      snprintf(title, sizeof(title), "DETECTED CORES %s", dir);
   else if (menu_type == RGUI_SETTINGS_CONFIG)
      snprintf(title, sizeof(title), "CONFIG %s", dir);
   else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof(title), "DISK APPEND %s", dir);
   else if (menu_type == RGUI_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_INPUT_OPTIONS)
      strlcpy(title, "INPUT OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_OVERLAY_OPTIONS)
      strlcpy(title, "OVERLAY OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_PATH_OPTIONS)
      strlcpy(title, "PATH OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_OPTIONS)
      strlcpy(title, "SETTINGS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_DRIVERS)
      strlcpy(title, "DRIVER OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == RGUI_SETTINGS_SHADER_OPTIONS)
      strlcpy(title, "SHADER OPTIONS", sizeof(title));
#endif
   else if (menu_type == RGUI_SETTINGS_GENERAL_OPTIONS)
      strlcpy(title, "GENERAL OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_AUDIO_OPTIONS)
      strlcpy(title, "AUDIO OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_DISK_OPTIONS)
      strlcpy(title, "DISK OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_CORE_OPTIONS)
      strlcpy(title, "CORE OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
      snprintf(title, sizeof(title), "SHADER %s", dir);
#endif
   else if ((menu_type == RGUI_SETTINGS_INPUT_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_PATH_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2) ||
         menu_type == RGUI_SETTINGS_CUSTOM_BIND ||
         menu_type == RGUI_START_SCREEN ||
         menu_type == RGUI_SETTINGS)
      snprintf(title, sizeof(title), "MENU %s", dir);
   else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      strlcpy(title, "LOAD HISTORY", sizeof(title));
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
      snprintf(title, sizeof(title), "OVERLAY %s", dir);
#endif
   else if (menu_type == RGUI_BROWSER_DIR_PATH)
      snprintf(title, sizeof(title), "BROWSER DIR %s", dir);
#ifdef HAVE_SCREENSHOTS
   else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
      snprintf(title, sizeof(title), "SCREENSHOT DIR %s", dir);
#endif
   else if (menu_type == RGUI_SHADER_DIR_PATH)
      snprintf(title, sizeof(title), "SHADER DIR %s", dir);
   else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVESTATE DIR %s", dir);
#ifdef HAVE_DYNAMIC
   else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
      snprintf(title, sizeof(title), "LIBRETRO DIR %s", dir);
#endif
   else if (menu_type == RGUI_CONFIG_DIR_PATH)
      snprintf(title, sizeof(title), "CONFIG DIR %s", dir);
   else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVEFILE DIR %s", dir);
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_OVERLAY_DIR_PATH)
      snprintf(title, sizeof(title), "OVERLAY DIR %s", dir);
#endif
   else if (menu_type == RGUI_SYSTEM_DIR_PATH)
      snprintf(title, sizeof(title), "SYSTEM DIR %s", dir);
   else
   {
      if (rgui->defer_core)
         snprintf(title, sizeof(title), "CONTENT %s", dir);
      else
      {
         const char *core_name = rgui->info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof(title), "CONTENT (%s) %s", core_name, dir);
      }
   }

   char title_buf[256];
   snprintf(title_buf, sizeof(title_buf), title);
//   menu_ticker_line(title_buf, RGUI_TERM_WIDTH - 3, g_extern.frame_count / 15, title, true);
//   blit_line(rgui, RGUI_TERM_START_X + 15, 15, title_buf, true);

   char title_msg[64];
   const char *core_name = rgui->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = rgui->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   //snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION, core_name, core_version);
   //blit_line(rgui, RGUI_TERM_START_X + 15, (RGUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) + RGUI_TERM_START_Y + 2, title_msg, true);

   unsigned x, y;
   size_t i;

   //x = RGUI_TERM_START_X;
   //y = RGUI_TERM_START_Y;

   for (i = begin; i < end; i++/*, y += FONT_HEIGHT_STRIDE */)
   {
      const char *path = 0;
      unsigned type = 0;
      file_list_get_at_offset(rgui->selection_buf, i, &path, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;
      if (menu_type == RGUI_SETTINGS_INPUT_OPTIONS || menu_type == RGUI_SETTINGS_CUSTOM_BIND)
         w = 21;
      else if (menu_type == RGUI_SETTINGS_PATH_OPTIONS)
         w = 24;

#ifdef HAVE_SHADER_MANAGER
      if (type >= RGUI_SETTINGS_SHADER_FILTER &&
            type <= RGUI_SETTINGS_SHADER_LAST)
      {
         // HACK. Work around that we're using the menu_type as dir type to propagate state correctly.
         if ((menu_type_is(menu_type) == RGUI_SETTINGS_SHADER_OPTIONS)
               && (menu_type_is(type) == RGUI_SETTINGS_SHADER_OPTIONS))
         {
            type = RGUI_FILE_DIRECTORY;
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            w = 5;
         }
         else if (type == RGUI_SETTINGS_SHADER_OPTIONS || type == RGUI_SETTINGS_SHADER_PRESET)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == RGUI_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else
            shader_manager_get_str(&rgui->shader, type_str, sizeof(type_str), type);
      }
      else
#endif
      // Pretty-print libretro cores from menu.
      if (menu_type == RGUI_SETTINGS_CORE || menu_type == RGUI_SETTINGS_DEFERRED_CORE)
      {
         if (type == RGUI_FILE_PLAIN)
         {
            strlcpy(type_str, "(CORE)", sizeof(type_str));
            file_list_get_alt_at_offset(rgui->selection_buf, i, &path);
            w = 6;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = RGUI_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == RGUI_SETTINGS_CONFIG ||
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
      {
         if (type == RGUI_FILE_PLAIN)
         {
            strlcpy(type_str, "(FILE)", sizeof(type_str));
            w = 6;
         }
         else if (type == RGUI_FILE_USE_DIRECTORY)
         {
            *type_str = '\0';
            w = 0;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = RGUI_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type >= RGUI_SETTINGS_CORE_OPTION_START)
         strlcpy(type_str,
               core_option_get_val(g_extern.system.core_options, type - RGUI_SETTINGS_CORE_OPTION_START),
               sizeof(type_str));
      else
         menu_set_settings_label(type_str, sizeof(type_str), &w, type);

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == rgui->selection_ptr;

      strlcpy(entry_title_buf, path, sizeof(entry_title_buf));
      strlcpy(type_str_buf, type_str, sizeof(type_str_buf));

/*
      if ((type == RGUI_FILE_PLAIN || type == RGUI_FILE_DIRECTORY))
         menu_ticker_line(entry_title_buf, RGUI_TERM_WIDTH - (w + 1 + 2), g_extern.frame_count / 15, path, selected);
      else
         menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15, type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            //RGUI_TERM_WIDTH - (w + 1 + 2), RGUI_TERM_WIDTH - (w + 1 + 2),
			20,
            entry_title_buf,
            w,
            type_str_buf);
*/

      blit_line(rgui, x, y, message, selected);
   }

   if (rgui->keyboard.display)
   {
      char msg[1024];
      const char *str = *rgui->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", rgui->keyboard.label, str);
      rmenu_xui_render_messagebox(rgui, msg);
   }
}


const menu_ctx_driver_t menu_ctx_rmenu_xui = {
   NULL,
   NULL,
   rmenu_xui_render,
   rmenu_xui_init,
   rmenu_xui_free,
   NULL,
   NULL,
   rmenu_xui_populate_entries,
   rmenu_xui_iterate,
   rmenu_xui_input_postprocess,
   "rmenu_xui",
};
