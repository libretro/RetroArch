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

#include "../backend/menu_common_backend.h"
#include "../menu_common.h"

#include "../../../gfx/gfx_common.h"
#include "../../../gfx/gfx_context.h"

#include "../../../settings_data.h"
#include "../../../message_queue.h"
#include "../../../general.h"

#define XUI_CONTROL_NAVIGATE_OK (XUI_CONTROL_NAVIGATE_RIGHT + 1)

#define FONT_WIDTH 5
#define FONT_HEIGHT 10
#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)
#define FONT_HEIGHT_STRIDE (FONT_HEIGHT + 1)
#define RXUI_TERM_START_X 15
#define RXUI_TERM_START_Y 27
#define RXUI_TERM_WIDTH (((driver.menu->width - RXUI_TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define RXUI_TERM_HEIGHT (((driver.menu->height - RXUI_TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

HXUIOBJ m_menulist;
HXUIOBJ m_menutitle;
HXUIOBJ m_menutitlebottom;
HXUIOBJ m_back;
HXUIOBJ root_menu;
HXUIOBJ current_menu;
static msg_queue_t *xui_msg_queue;

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
      HRESULT DispatchMessageMap(XUIMessage *pMessage) \
      { \
         if (pMessage->dwMessage == XM_INIT) \
         { \
            XUIMessageInit *pData = (XUIMessageInit *) pMessage->pvData; \
            return OnInit(pData, pMessage->bHandled); \
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

HRESULT CRetroArchMain::OnInit(XUIMessageInit * pInitData, BOOL& bHandled)
{
   GetChildById(L"XuiMenuList", &m_menulist);
   GetChildById(L"XuiTxtTitle", &m_menutitle);
   GetChildById(L"XuiTxtBottom", &m_menutitlebottom);

   char str[PATH_MAX];
   snprintf(str, sizeof(str), "%s - %s", PACKAGE_VERSION, g_extern.title_buf);
   mbstowcs(strw_buffer, str, sizeof(strw_buffer) / sizeof(wchar_t));
   XuiTextElementSetText(m_menutitlebottom, strw_buffer);

   return 0;
}

static void* rmenu_xui_init(void)
{
   HRESULT hr;
   TypefaceDescriptor typeface = {0};

   menu_handle_t *menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   d3d_video_t *d3d= (d3d_video_t*)driver.video_data;

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

   d3d_make_d3dpp(d3d, &video_info, &d3dpp);

   hr = app.InitShared(d3d->dev, &d3dpp, XuiPNGTextureLoader);

   if (FAILED(hr))
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      goto error;
   }

   /* Register font */
   typeface.szTypeface = L"Arial Unicode MS";
   typeface.szLocator = L"file://game:/media/rarch.ttf";
   typeface.szReserved1 = NULL;

   hr = XuiRegisterTypeface( &typeface, TRUE );
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to register default typeface.\n");
      goto error;
   }

   hr = XuiLoadVisualFromBinary( L"file://game:/media/rarch_scene_skin.xur", NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to load skin.\n");
      goto error;
   }

   hr = XuiSceneCreate(hdmenus_allowed ? L"file://game:/media/hd/" : L"file://game:/media/sd/", L"rarch_main.xur", NULL, &root_menu);
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      goto error;
   }

   current_menu = root_menu;
   hr = XuiSceneNavigateFirst(app.GetRootObj(), current_menu, XUSER_INDEX_FOCUS);
   if (FAILED(hr))
   {
      RARCH_ERR("XuiSceneNavigateFirst failed.\n");
      goto error;
   }

   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_frame(driver.video_data, NULL,
            true, 0, 0, 1.0f);

   xui_msg_queue = msg_queue_new(16);

   return menu;

error:
   free(menu);
   return NULL;
}

static void rmenu_xui_free(void *data)
{
   (void)data;
   app.Uninit();

   if (xui_msg_queue)
      msg_queue_free(xui_msg_queue);
}

static void xui_render_message(const char *msg)
{
	struct font_params font_parms;
	size_t i, j;

	struct string_list *list = string_split(msg, "\n");
	if (!list)
		return;

	if (list->elems == 0)
	{
		string_list_free(list);
		return;
	}

	j = 0;
	for (i = 0; i < list->size; i++, j++)
	{
		char *msg = (char*)list->elems[i].data;
		unsigned msglen = strlen(msg);
	#if 0
		if (msglen > RMENU_TERM_WIDTH)
		{
			msg[RMENU_TERM_WIDTH - 2] = '.';
			msg[RMENU_TERM_WIDTH - 1] = '.';
			msg[RMENU_TERM_WIDTH - 0] = '.';
			msg[RMENU_TERM_WIDTH + 1] = '\0';
			msglen = RMENU_TERM_WIDTH;
		}
	#endif
		float msg_width  = (g_extern.lifecycle_state & (1ULL << MODE_MENU_HD)) ? 160 : 100;
		float msg_height = 120;
		float msg_offset = 32;

		font_parms.x = msg_width;
		font_parms.y = msg_height + (msg_offset * j);
		font_parms.scale = 21;

		if (driver.video_poke && driver.video_poke->set_osd_msg)
			driver.video_poke->set_osd_msg(driver.video_data, msg, &font_parms);
	}
}

static void rmenu_xui_frame(void)
{
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   D3DVIEWPORT vp_full;
   vp_full.X = 0;
   vp_full.Y = 0;
   vp_full.Width = d3d->screen_width;
   vp_full.Height = d3d->screen_height;
   vp_full.MinZ = 0.0f;
   vp_full.MaxZ = 1.0f;
   d3dr->SetViewport(&vp_full);

   app.RunFrame();
   XuiTimersRun();
   XuiRenderBegin( app.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

   D3DXMATRIX matOrigView;
   XuiRenderGetViewTransform( app.GetDC(), &matOrigView );

   XUIMessage msg;
   XUIMessageRender msgRender;
   XuiMessageRender( &msg, &msgRender, app.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
   XuiSendMessage( app.GetRootObj(), &msg );

   XuiRenderSetViewTransform( app.GetDC(), &matOrigView );

   const char *message = msg_queue_pull(xui_msg_queue);

   if (message)
      xui_render_message(message);
   else
   {
      const char *message = msg_queue_pull(g_extern.msg_queue);
      if (message)
         xui_render_message(message);
   }

   XuiRenderEnd( app.GetDC() );

   d3dr->SetViewport(&d3d->final_viewport);
}

static int rmenu_xui_input_postprocess(uint64_t old_state)
{
   bool quit = false;
   bool resize = false;
   unsigned width;
   unsigned height;
   unsigned frame_count;

   (void)width;
   (void)height;
   (void)frame_count;

   if ((driver.menu->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
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

static void blit_line(int x, int y, const char *message, bool green)
{
}

static void rmenu_xui_render_background(void)
{
}

static void rmenu_xui_render_messagebox(const char *message)
{
   msg_queue_clear(xui_msg_queue);
   msg_queue_push(xui_msg_queue, message, 2, 1);
}

static void rmenu_xui_render(void)
{
   size_t begin, end;
   char title[256];
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   unsigned menu_type_is = 0;

   if (!driver.menu || driver.menu->need_refresh && 
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !driver.menu->msg_force)
      return;

   begin = driver.menu->selection_ptr;
   end   = file_list_get_size(driver.menu->selection_buf);

   rmenu_xui_render_background();

   file_list_get_last(driver.menu->menu_stack, &dir, &label, &menu_type);

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->type_is)
      menu_type_is = driver.menu_ctx->backend->type_is(menu_type);

   if (menu_type == MENU_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION %s", dir);
   else if (menu_type == MENU_SETTINGS_DEFERRED_CORE)
      snprintf(title, sizeof(title), "DETECTED CORES %s", dir);
   else if (menu_type == MENU_SETTINGS_CONFIG)
      snprintf(title, sizeof(title), "CONFIG %s", dir);
   else if (menu_type == MENU_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof(title), "DISK APPEND %s", dir);
   else if (menu_type == MENU_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_INPUT_OPTIONS)
      strlcpy(title, "INPUT OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_OVERLAY_OPTIONS)
      strlcpy(title, "OVERLAY OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_NETPLAY_OPTIONS)
      strlcpy(title, "NETPLAY OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_USER_OPTIONS)
      strlcpy(title, "USER OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_PATH_OPTIONS)
      strlcpy(title, "PATH OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_OPTIONS)
      strlcpy(title, "SETTINGS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_DRIVERS)
      strlcpy(title, "DRIVER OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS)
      strlcpy(title, "PERFORMANCE COUNTERS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO)
      strlcpy(title, "CORE PERFORMANCE COUNTERS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND)
      strlcpy(title, "FRONTEND PERFORMANCE COUNTERS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == MENU_SETTINGS_SHADER_OPTIONS)
      strlcpy(title, "SHADER OPTIONS", sizeof(title));
#endif
   else if (menu_type == MENU_SETTINGS_FONT_OPTIONS)
      strlcpy(title, "FONT OPTIONS", sizeof(title));
   else if (!strcmp(dir, "General Options"))
      strlcpy(title, "GENERAL OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_AUDIO_OPTIONS)
      strlcpy(title, "AUDIO OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_DISK_OPTIONS)
      strlcpy(title, "DISK OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_CORE_OPTIONS)
      strlcpy(title, "CORE OPTIONS", sizeof(title));
   else if (menu_type == MENU_SETTINGS_CORE_INFO)
      strlcpy(title, "CORE INFO", sizeof(title));		  
   else if (menu_type == MENU_SETTINGS_PRIVACY_OPTIONS)
      strlcpy(title, "PRIVACY OPTIONS", sizeof(title)); 	  
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type_is == MENU_SETTINGS_SHADER_OPTIONS)
      snprintf(title, sizeof(title), "SHADER %s", dir);
#endif
   else if ((menu_type == MENU_SETTINGS_INPUT_OPTIONS) ||
         (menu_type == MENU_SETTINGS_PATH_OPTIONS) ||
         (menu_type == MENU_SETTINGS_OPTIONS) ||
         (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
          !strcmp(label, "custom_viewport_2")) ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         !strcmp(label, "help") ||
         menu_type == MENU_SETTINGS)
      snprintf(title, sizeof(title), "MENU %s", dir);
   else if (menu_type == MENU_SETTINGS_OPEN_HISTORY)
      strlcpy(title, "LOAD HISTORY", sizeof(title));
   else if (!strcmp(label, "info_screen"))
      strlcpy(title, "INFO", sizeof(title));
#ifdef HAVE_OVERLAY
   else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
      snprintf(title, sizeof(title), "OVERLAY %s", dir);
#endif
   else if (menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER)
      snprintf(title, sizeof(title), "FILTER %s", dir);
   else if (menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER)
      snprintf(title, sizeof(title), "DSP FILTER %s", dir);
   else if (menu_type == MENU_BROWSER_DIR_PATH)
      snprintf(title, sizeof(title), "BROWSER DIR %s", dir);
   else if (menu_type == MENU_CONTENT_DIR_PATH)
      snprintf(title, sizeof(title), "CONTENT DIR %s", dir);
   else if (menu_type == MENU_SCREENSHOT_DIR_PATH)
      snprintf(title, sizeof(title), "SCREENSHOT DIR %s", dir);
   else if (menu_type == MENU_AUTOCONFIG_DIR_PATH)
      snprintf(title, sizeof(title), "AUTOCONFIG DIR %s", dir);
   else if (menu_type == MENU_SHADER_DIR_PATH)
      snprintf(title, sizeof(title), "SHADER DIR %s", dir);
   else if (menu_type == MENU_FILTER_DIR_PATH)
      snprintf(title, sizeof(title), "FILTER DIR %s", dir);
   else if (menu_type == MENU_DSP_FILTER_DIR_PATH)
      snprintf(title, sizeof(title), "DSP_FILTER DIR %s", dir);
   else if (menu_type == MENU_SAVESTATE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVESTATE DIR %s", dir);
#ifdef HAVE_DYNAMIC
   else if (menu_type == MENU_LIBRETRO_DIR_PATH)
      snprintf(title, sizeof(title), "LIBRETRO DIR %s", dir);
#endif
   else if (menu_type == MENU_CONFIG_DIR_PATH)
      snprintf(title, sizeof(title), "CONFIG DIR %s", dir);
   else if (menu_type == MENU_SAVEFILE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVEFILE DIR %s", dir);
#ifdef HAVE_OVERLAY
   else if (menu_type == MENU_OVERLAY_DIR_PATH)
      snprintf(title, sizeof(title), "OVERLAY DIR %s", dir);
#endif
   else if (menu_type == MENU_SYSTEM_DIR_PATH)
      snprintf(title, sizeof(title), "SYSTEM DIR %s", dir);
   else if (menu_type == MENU_ASSETS_DIR_PATH)
      snprintf(title, sizeof(title), "ASSETS DIR %s", dir);
   else
   {
      if (driver.menu->defer_core)
         snprintf(title, sizeof(title), "CONTENT %s", dir);
      else
      {
         const char *core_name = driver.menu->info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof(title), "CONTENT (%s) %s", core_name, dir);
      }
   }

   mbstowcs(strw_buffer, title, sizeof(strw_buffer) / sizeof(wchar_t));
   XuiTextElementSetText(m_menutitle, strw_buffer);

   char title_buf[256];
   menu_ticker_line(title_buf, RXUI_TERM_WIDTH - 3, g_extern.frame_count / 15, title, true);
   blit_line(RXUI_TERM_START_X + 15, 15, title_buf, true);

   char title_msg[64];
   const char *core_name = driver.menu->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = driver.menu->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION, core_name, core_version);
   blit_line(RXUI_TERM_START_X + 15, (RXUI_TERM_HEIGHT * FONT_HEIGHT_STRIDE) + RXUI_TERM_START_Y + 2, title_msg, true);

   unsigned x, y;
   size_t i;

   x = RXUI_TERM_START_X;
   y = RXUI_TERM_START_Y;

   for (i = begin; i < end; i++/*, y += FONT_HEIGHT_STRIDE */)
   {
      const char *path = NULL;
      const char *path = NULL;
      unsigned type = 0;
      file_list_get_at_offset(driver.menu->selection_buf, i, &path, &label, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;
      if (menu_type == MENU_SETTINGS_INPUT_OPTIONS || menu_type == MENU_SETTINGS_CUSTOM_BIND)
         w = 21;
      else if (menu_type == MENU_SETTINGS_PATH_OPTIONS)
         w = 24;

      if (type >= MENU_SETTINGS_SHADER_FILTER &&
            type <= MENU_SETTINGS_SHADER_LAST)
      {
         // HACK. Work around that we're using the menu_type as dir type to propagate state correctly.
         if ((menu_type_is == MENU_SETTINGS_SHADER_OPTIONS)
               && (menu_type_is == MENU_SETTINGS_SHADER_OPTIONS))
         {
            type = MENU_FILE_DIRECTORY;
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            w = 5;
         }
         else if (type == MENU_SETTINGS_SHADER_OPTIONS || type == MENU_SETTINGS_SHADER_PRESET)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == MENU_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_get_str)
            driver.menu_ctx->backend->shader_manager_get_str(driver.menu->shader, type_str, sizeof(type_str), type);
      }
      else
      // Pretty-print libretro cores from menu.
      if (menu_type == MENU_SETTINGS_CORE || menu_type == MENU_SETTINGS_DEFERRED_CORE)
      {
         if (type == MENU_FILE_PLAIN)
         {
            strlcpy(type_str, "(CORE)", sizeof(type_str));
            file_list_get_alt_at_offset(driver.menu->selection_buf, i, &path);
            w = 6;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = MENU_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == MENU_SETTINGS_CONFIG ||
#ifdef HAVE_OVERLAY
            menu_type == MENU_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == MENU_SETTINGS_VIDEO_SOFTFILTER ||
            menu_type == MENU_SETTINGS_AUDIO_DSP_FILTER ||
            menu_type == MENU_SETTINGS_DISK_APPEND ||
            menu_type_is == MENU_FILE_DIRECTORY)
      {
         if (type == MENU_FILE_PLAIN)
         {
            strlcpy(type_str, "(FILE)", sizeof(type_str));
            w = 6;
         }
         else if (type == MENU_FILE_USE_DIRECTORY)
         {
            *type_str = '\0';
            w = 0;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = MENU_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == MENU_SETTINGS_OPEN_HISTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type >= MENU_SETTINGS_CORE_OPTION_START)
         strlcpy(type_str,
               core_option_get_val(g_extern.system.core_options, type - MENU_SETTINGS_CORE_OPTION_START),
               sizeof(type_str));
      else if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->setting_set_label)
         driver.menu_ctx->backend->setting_set_label(type_str, sizeof(type_str), &w, type, i);

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == driver.menu->selection_ptr;

      strlcpy(entry_title_buf, path, sizeof(entry_title_buf));
      strlcpy(type_str_buf, type_str, sizeof(type_str_buf));

#if 0
      if ((type == MENU_FILE_PLAIN || type == MENU_FILE_DIRECTORY))
         menu_ticker_line(entry_title_buf, RXUI_TERM_WIDTH - (w + 1 + 2), g_extern.frame_count / 15, path, selected);
      else
         menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15, type_str, selected);
#endif

      snprintf(message, sizeof(message), "%s : %s",
            entry_title_buf,
            type_str_buf);

      wchar_t msg_w[256];
      mbstowcs(msg_w, message, sizeof(msg_w) / sizeof(wchar_t));
      XuiListSetText(m_menulist, i, msg_w);
      blit_line(x, y, message, i);
   }

   if (driver.menu->keyboard.display)
   {
      char msg[1024];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", driver.menu->keyboard.label, str);
      rmenu_xui_render_messagebox(msg);
   }
}

static void rmenu_xui_populate_entries(void *data, const char *path,
      const char *label, unsigned i)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   (void)label;
   (void)path;

   XuiListSetCurSelVisible(m_menulist, menu->selection_ptr);
}

static void rmenu_xui_navigation_clear(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   XuiListSetCurSelVisible(m_menulist, menu->selection_ptr);
}

static void rmenu_xui_navigation_set_visible(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   XuiListSetCurSelVisible(m_menulist, menu->selection_ptr);
}

static void rmenu_xui_navigation_alphabet(void *data, size_t *ptr_out)
{
   XuiListSetCurSelVisible(m_menulist, *ptr_out);
}

static void rmenu_xui_list_insert(void *data, const char *path, size_t list_size)
{
   (void)data;
   wchar_t buf[PATH_MAX];

   XuiListInsertItems(m_menulist, list_size, 1);
   mbstowcs(buf, path, sizeof(buf) / sizeof(wchar_t));
   XuiListSetText(m_menulist, list_size, buf);
}

static void rmenu_xui_list_delete(void *data, size_t list_size)
{
   (void)data;
   XuiListDeleteItems(m_menulist, 0, list_size);
}

static void rmenu_xui_list_clear(void *data)
{
   (void)data;
   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));
}

static void rmenu_xui_list_set_selection(void *data)
{
   file_list_t *list = (file_list_t*)data;
   if (list)
      XuiListSetCurSel(m_menulist, file_list_get_directory_ptr(list));
}

static void rmenu_xui_init_core_info(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   core_info_list_free(menu->core_info);
   menu->core_info = NULL;
   if (*g_settings.libretro_directory)
      menu->core_info = core_info_list_new(g_settings.libretro_directory);
}

const menu_ctx_driver_t menu_ctx_rmenu_xui = {
   NULL,
   rmenu_xui_render_messagebox,
   rmenu_xui_render,
   rmenu_xui_frame,
   rmenu_xui_init,
   rmenu_xui_free,
   NULL,
   NULL,
   rmenu_xui_populate_entries,
   NULL,
   rmenu_xui_input_postprocess,
   rmenu_xui_navigation_clear,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_alphabet,
   rmenu_xui_navigation_alphabet,
   rmenu_xui_list_insert,
   rmenu_xui_list_delete,
   rmenu_xui_list_clear,
   rmenu_xui_list_set_selection,
   rmenu_xui_init_core_info,
   &menu_ctx_backend_common,
   "rmenu_xui",
};
