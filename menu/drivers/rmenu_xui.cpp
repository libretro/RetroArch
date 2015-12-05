/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2015-     - Swizzy
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>

#include <file/file_path.h>
#include <string/string_list.h>
#include <queues/message_queue.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu.h"
#include "../menu_animation.h"
#include "../menu_entry.h"
#include "../menu_entries.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_display.h"

#include "../../gfx/video_context_driver.h"

#include "../../general.h"

#include "../../gfx/d3d/d3d.h"

#define XUI_CONTROL_NAVIGATE_OK (XUI_CONTROL_NAVIGATE_RIGHT + 1)

#define FONT_WIDTH 5
#define FONT_HEIGHT 10
#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)
#define FONT_HEIGHT_STRIDE (FONT_HEIGHT + 1)
#define RXUI_TERM_START_X 15
#define RXUI_TERM_START_Y 27
#define RXUI_TERM_WIDTH(width) (((width - RXUI_TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define RXUI_TERM_HEIGHT(height) (((height - RXUI_TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

HXUIOBJ m_menulist;
HXUIOBJ m_menutitle;
HXUIOBJ m_menutitlebottom;
HXUIOBJ m_background;
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

wchar_t strw_buffer[PATH_MAX_LENGTH];

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
   GetChildById(L"XuiBackground", &m_background);

   if (XuiHandleIsValid(m_menutitlebottom))
   {
	   char str[PATH_MAX_LENGTH] = {0};

      if (menu_entries_get_core_title(str, sizeof(str)) == 0)
      {
         mbstowcs(strw_buffer, str, sizeof(strw_buffer) / sizeof(wchar_t));
         XuiTextElementSetText(m_menutitlebottom, strw_buffer);
      }
   }

   return 0;
}

HRESULT XuiTextureLoader(IXuiDevice *pDevice, LPCWSTR szFileName,
      XUIImageInfo *pImageInfo, IDirect3DTexture9 **ppTex)
{
   D3DXIMAGE_INFO pSrc;
   CONST BYTE  *pbTextureData      = 0;
   UINT         cbTextureData      = 0;
   HXUIRESOURCE hResource          = 0;
   BOOL         bIsMemoryResource  = FALSE;
   IDirect3DDevice9 * d3dDevice    = NULL;
   HRESULT      hr                 = 
      XuiResourceOpenNoLoc(szFileName, &hResource, &bIsMemoryResource);

   if (FAILED(hr))
      return hr; 

   if (bIsMemoryResource)
   {
      hr = XuiResourceGetBuffer(hResource, &pbTextureData);
      if (FAILED(hr))
         goto cleanup;
      cbTextureData = XuiResourceGetTotalSize(hResource);
   }
   else 
   {
      hr = XuiResourceRead(hResource, NULL, 0, &cbTextureData);
      if (FAILED(hr))
         goto cleanup;

      pbTextureData = (BYTE *)XuiAlloc(cbTextureData);
      if (pbTextureData == 0)
      {
         hr = E_OUTOFMEMORY;
         goto cleanup;
      }

      hr = XuiResourceRead(hResource,
            (BYTE*)pbTextureData, cbTextureData, &cbTextureData);
      if (FAILED(hr))
         goto cleanup;

      XuiResourceClose(hResource);
      hResource = 0;

   }

   /* Cast our d3d device into our IDirect3DDevice9* interface */
   d3dDevice = (IDirect3DDevice9*)pDevice->GetD3DDevice();
   if(!d3dDevice)
      goto cleanup;

   /* Create our texture based on our conditions */
   hr = D3DXCreateTextureFromFileInMemoryEx(
         d3dDevice,
         pbTextureData,  
         cbTextureData,
         D3DX_DEFAULT_NONPOW2, 
         D3DX_DEFAULT_NONPOW2,
         1,
         D3DUSAGE_CPU_CACHED_MEMORY,
         D3DFMT_LIN_A8R8G8B8,
         D3DPOOL_DEFAULT,
         D3DX_FILTER_NONE,
         D3DX_FILTER_NONE,
         0,
         &pSrc,
         NULL,
         ppTex
         );

   if(hr != D3DXERR_INVALIDDATA )
   {
      pImageInfo->Depth = pSrc.Depth;
      pImageInfo->Format = pSrc.Format;
      pImageInfo->Height = pSrc.Height;
      pImageInfo->ImageFileFormat = pSrc.ImageFileFormat;
      pImageInfo->MipLevels = pSrc.MipLevels;
      pImageInfo->ResourceType = pSrc.ResourceType;
      pImageInfo->Width = pSrc.Width;
   }
   else
      RARCH_ERR("D3DXERR_INVALIDDATA Encountered\n");

cleanup:

   if (bIsMemoryResource && hResource != 0)
      XuiResourceReleaseBuffer(hResource, pbTextureData);
   else
      XuiFree((LPVOID)pbTextureData);

   if (hResource != 0)
      XuiResourceClose(hResource);
   return hr;
}

static void* rmenu_xui_init(void)
{
   HRESULT hr;
   d3d_video_t *d3d            = NULL;
   D3DPRESENT_PARAMETERS d3dpp = {0};
   video_info_t video_info     = {0};
   TypefaceDescriptor typeface = {0};
   settings_t *settings        = config_get_ptr();
   menu_handle_t *menu         = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (d3d->resolution_hd_enable)
      RARCH_LOG("HD menus enabled.\n");

   video_info.vsync        = settings->video.vsync;
   video_info.force_aspect = false;
   video_info.smooth       = settings->video.smooth;
   video_info.input_scale  = 2;
   video_info.fullscreen   = true;
   video_info.rgb32        = false;

   d3d_make_d3dpp(d3d, &video_info, &d3dpp);
   
   hr = app.InitShared(d3d->dev, &d3dpp, (PFN_XUITEXTURELOADER)XuiTextureLoader);

   if (FAILED(hr))
   {
      RARCH_ERR("Failed initializing XUI application.\n");
      goto error;
   }

   /* Register font */
   typeface.szTypeface  = L"Arial Unicode MS";
   typeface.szLocator   = L"file://game:/media/rarch.ttf";
   typeface.szReserved1 = NULL;

   hr = XuiRegisterTypeface( &typeface, TRUE );
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to register default typeface.\n");
      goto error;
   }

   hr = XuiLoadVisualFromBinary(
         L"file://game:/media/rarch_scene_skin.xur", NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to load skin.\n");
      goto error;
   }

   hr = XuiSceneCreate(d3d->resolution_hd_enable ?
         L"file://game:/media/hd/" : L"file://game:/media/sd/",
         L"rarch_main.xur", NULL, &root_menu);
   if (FAILED(hr))
   {
      RARCH_ERR("Failed to create scene 'rarch_main.xur'.\n");
      goto error;
   }

   current_menu = root_menu;
   hr = XuiSceneNavigateFirst(app.GetRootObj(),
         current_menu, XUSER_INDEX_FOCUS);
   if (FAILED(hr))
   {
      RARCH_ERR("XuiSceneNavigateFirst failed.\n");
      goto error;
   }

   video_driver_set_texture_frame(NULL,
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
   struct font_params font_parms = {0};
   size_t i                      = 0;
   size_t j                      = 0;
   struct string_list *list      = NULL;
   d3d_video_t              *d3d = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return;
   
   list = string_split(msg, "\n");

   if (!list)
      return;

   if (list->elems == 0)
      goto end;

   for (i = 0; i < list->size; i++, j++)
   {
      char *msg        = (char*)list->elems[i].data;
      unsigned msglen  = strlen(msg);
      float msg_width  = d3d->resolution_hd_enable ? 160 : 100;
      float msg_height = 120;
      float msg_offset = 32;

      font_parms.x = msg_width;
      font_parms.y = msg_height + (msg_offset * j);
      font_parms.scale = 21;

      video_driver_set_osd_msg(msg, &font_parms, NULL);
   }

end:
   string_list_free(list);
}

static void rmenu_xui_frame(void)
{
   XUIMessage msg;
   XUIMessageRender msgRender;
   D3DXMATRIX matOrigView;
   LPDIRECT3DDEVICE d3dr;
   const char *message;
   D3DVIEWPORT vp_full   = {0};
   d3d_video_t *d3d      = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();

   if (!menu)
      return;

   d3d = (d3d_video_t*)video_driver_get_ptr(false);
   
   if (!d3d)
      return;

   d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   if (!d3dr)
      return;

   menu_display_ctl(MENU_DISPLAY_CTL_SET_VIEWPORT, NULL);

   app.RunFrame();
   XuiTimersRun();
   XuiRenderBegin( app.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

   XuiRenderGetViewTransform( app.GetDC(), &matOrigView );

   XuiMessageRender( &msg, &msgRender, app.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
   XuiSendMessage( app.GetRootObj(), &msg );

   XuiRenderSetViewTransform( app.GetDC(), &matOrigView );

   message = rarch_main_msg_queue_pull();

   if (message)
      xui_render_message(message);
   else
   {
      message = rarch_main_msg_queue_pull();
      if (message)
         xui_render_message(message);
   }

   XuiRenderEnd( app.GetDC() );

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_VIEWPORT, NULL);
}

static void blit_line(int x, int y, const char *message, bool green)
{
}

static void rmenu_xui_render_background(void)
{
   bool libretro_running           = menu_display_ctl(MENU_DISPLAY_CTL_LIBRETRO_RUNNING, NULL);

	if (libretro_running)
		XuiElementSetShow(m_background, FALSE);
	else
		XuiElementSetShow(m_background, TRUE);
}

static void rmenu_xui_render_messagebox(const char *message)
{
   msg_queue_clear(xui_msg_queue);
   msg_queue_push(xui_msg_queue, message, 2, 1);
}

static void rmenu_xui_set_list_text(int index, const wchar_t* leftText,
      const wchar_t* rightText)
{
   LPCWSTR currText;
   float width, height;
   XUIRect pRect;
   D3DXVECTOR3 textPos, rightEdgePos;
   HXUIOBJ hVisual = NULL, hControl = NULL, hTextLeft = NULL,
           hTextRight = NULL, hRightEdge = NULL;

   hControl = XuiListGetItemControl(m_menulist, index);

   if (XuiHandleIsValid(hControl))
      XuiControlGetVisual(hControl, &hVisual);

   if(!XuiHandleIsValid(hVisual))
      return;

   XuiElementGetChildById(hVisual, L"LeftText", &hTextLeft);

   if (!XuiHandleIsValid(hTextLeft))
      return;

   currText = XuiTextElementGetText(hTextLeft);
   XuiElementGetBounds(hTextLeft, &width, &height);

   if (!currText || wcscmp(currText, leftText) || width <= 5)
   {
      XuiTextElementMeasureText(hTextLeft, leftText, &pRect);
      XuiElementSetBounds(hTextLeft, pRect.GetWidth(), height);
   }

   XuiTextElementSetText(hTextLeft, leftText);
   XuiElementGetChildById(hVisual, L"RightText", &hTextRight);

   if(XuiHandleIsValid(hTextRight)) 
   {
      currText = XuiTextElementGetText(hTextRight);
      XuiElementGetBounds(hTextRight, &width, &height);

      if (!currText || wcscmp(currText, rightText) || width <= 5)
      {
         XuiTextElementMeasureText(hTextRight, rightText, &pRect);
         XuiElementSetBounds(hTextRight, pRect.GetWidth(), height);
         XuiElementGetPosition(hTextLeft, &textPos);

         XuiElementGetChildById(hVisual, L"graphic_CapRight", &hRightEdge);
         XuiElementGetPosition(hRightEdge, &rightEdgePos);

         textPos.x = rightEdgePos.x - (pRect.GetWidth() + textPos.x);
         XuiElementSetPosition(hTextRight, &textPos);
      }

      XuiTextElementSetText(hTextRight, rightText);
   }
}

static void rmenu_xui_render(void)
{
   uint64_t *frame_count;
   bool display_kb, msg_force;
   unsigned fb_width;
	size_t end, i, selection;
	char title[PATH_MAX_LENGTH] = {0};
	const char *dir             = NULL;
   const char *label           = NULL;
	unsigned menu_type          = 0;
   menu_handle_t *menu         = menu_driver_get_ptr();

   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   menu_display_ctl(MENU_DISPLAY_CTL_WIDTH,     &fb_width);
   menu_display_ctl(MENU_DISPLAY_CTL_MSG_FORCE, &msg_force);

   if (!menu)
      return;
   if (
         menu_entries_needs_refresh() 
         && menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL)
         && !msg_force
      )
		return;

   menu_display_ctl(MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG, NULL);
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

	rmenu_xui_render_background();

	if (XuiHandleIsValid(m_menutitle))
	{
      menu_entries_get_title(title, sizeof(title));
		mbstowcs(strw_buffer, title, sizeof(strw_buffer) / sizeof(wchar_t));
		XuiTextElementSetText(m_menutitle, strw_buffer);
		menu_animation_ticker_str(title, RXUI_TERM_WIDTH(fb_width) - 3,
            (unsigned int)*frame_count / 15, title, true);
	}

	if (XuiHandleIsValid(m_menutitle))
	{
      if (menu_entries_get_core_title(title, sizeof(title)) == 0)
      {
         mbstowcs(strw_buffer, title, sizeof(strw_buffer) / sizeof(wchar_t));
         XuiTextElementSetText(m_menutitlebottom, strw_buffer);
      }
	}

	end = menu_entries_get_end();
	for (i = 0; i < end; i++)
   {
      char entry_path[PATH_MAX_LENGTH]     = {0};
      char entry_value[PATH_MAX_LENGTH]    = {0};
      wchar_t msg_right[PATH_MAX_LENGTH]   = {0};
      wchar_t msg_left[PATH_MAX_LENGTH]    = {0};

      menu_entry_get_value(i, entry_value, sizeof(entry_value));
      menu_entry_get_path(i, entry_path, sizeof(entry_path));

      mbstowcs(msg_left,  entry_path,  sizeof(msg_left)  / sizeof(wchar_t));
      mbstowcs(msg_right, entry_value, sizeof(msg_right) / sizeof(wchar_t));
      rmenu_xui_set_list_text(i, msg_left, msg_right);
   }
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;
	XuiListSetCurSelVisible(m_menulist, selection);

   menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_DISPLAY, &display_kb);

   if (display_kb)
	{
		char msg[1024]  = {0};
      const char *str = NULL, *label = NULL;
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_BUFF_PTR, &str);
      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL,    &label);

		if (!str)
			str = "";
      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
		rmenu_xui_render_messagebox(msg);			
	}
}

static void rmenu_xui_populate_entries(const char *path,
      const char *label, unsigned i)
{
   size_t selection;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   (void)label;
   (void)path;

   XuiListSetCurSelVisible(m_menulist, selection);
}

static void rmenu_xui_navigation_clear(bool pending_push)
{
   size_t selection;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   XuiListSetCurSelVisible(m_menulist, selection);
}

static void rmenu_xui_navigation_set_visible(void)
{
   size_t selection;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   XuiListSetCurSelVisible(m_menulist, selection);
}

static void rmenu_xui_navigation_alphabet(size_t *ptr_out)
{
   XuiListSetCurSelVisible(m_menulist, *ptr_out);
}

static void rmenu_xui_list_insert(file_list_t *list,
      const char *path, const char *, size_t list_size)
{
   wchar_t buf[PATH_MAX_LENGTH] = {0};

   XuiListInsertItems(m_menulist, list_size, 1);
   mbstowcs(buf, path, sizeof(buf) / sizeof(wchar_t));
   XuiListSetText(m_menulist, list_size, buf);
}

static void rmenu_xui_list_free(file_list_t *list, size_t idx,
      size_t list_size)
{
   int x = XuiListGetItemCount( m_menulist );

   (void)idx;

   if( list_size > x)
      list_size = x;
   if( list_size > 0)
      XuiListDeleteItems(m_menulist, 0, list_size);
}

static void rmenu_xui_list_clear(file_list_t *list)
{
   XuiListDeleteItems(m_menulist, 0, XuiListGetItemCount(m_menulist));
}

static void rmenu_xui_list_set_selection(file_list_t *list)
{
   if (list)
      XuiListSetCurSel(m_menulist, file_list_get_directory_ptr(list));
}

static int rmenu_xui_environ(menu_environ_cb_t type, void *data)
{
   switch (type)
   {
      case 0:
      default:
         return -1;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_rmenu_xui = {
   NULL,
   rmenu_xui_render_messagebox,
   generic_menu_iterate,
   rmenu_xui_render,
   rmenu_xui_frame,
   rmenu_xui_init,
   rmenu_xui_free,
   NULL,
   NULL,
   rmenu_xui_populate_entries,
   NULL,
   rmenu_xui_navigation_clear,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_clear,
   rmenu_xui_navigation_set_visible,
   rmenu_xui_navigation_alphabet,
   rmenu_xui_navigation_alphabet,
   generic_menu_init_list,
   rmenu_xui_list_insert,
   rmenu_xui_list_free,
   rmenu_xui_list_clear,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   rmenu_xui_list_set_selection,
   NULL,
   NULL,
   "rmenu_xui",
   rmenu_xui_environ,
   NULL,
};
