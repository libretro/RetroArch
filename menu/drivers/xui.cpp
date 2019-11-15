/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#define CINTERFACE

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>

#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <queues/message_queue.h>

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_entries.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../widgets/menu_input_dialog.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../../gfx/common/d3d_common.h"
#include "../../gfx/common/d3d9_common.h"

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
static msg_queue_t *xui_msg_queue = NULL;

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

      menu_entries_get_core_title(str, sizeof(str));
      mbstowcs(strw_buffer, str, sizeof(strw_buffer) / sizeof(wchar_t));
      XuiTextElementSetText(m_menutitlebottom, strw_buffer);
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
         (D3DFORMAT)d3d9_get_argb8888_format(),
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
      pImageInfo->Depth           = pSrc.Depth;
      pImageInfo->Format          = pSrc.Format;
      pImageInfo->Height          = pSrc.Height;
      pImageInfo->ImageFileFormat = pSrc.ImageFileFormat;
      pImageInfo->MipLevels       = pSrc.MipLevels;
      pImageInfo->ResourceType    = pSrc.ResourceType;
      pImageInfo->Width           = pSrc.Width;
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

void d3d9_make_d3dpp(void *data, const video_info_t *info, void *_d3dpp);

static void* xui_init(void **userdata, bool video_is_threaded)
{
   HRESULT hr;
   d3d9_video_t *d3d           = NULL;
   D3DPRESENT_PARAMETERS d3dpp = {0};
   video_info_t video_info     = {0};
   TypefaceDescriptor typeface = {0};
   settings_t *settings        = config_get_ptr();
   menu_handle_t *menu         = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   d3d = (d3d9_video_t*)video_driver_get_ptr(false);

   if (d3d->resolution_hd_enable)
      RARCH_LOG("HD menus enabled.\n");

   video_info.vsync        = settings->bools.video_vsync;
   video_info.force_aspect = false;
   video_info.smooth       = settings->bools.video_smooth;
   video_info.input_scale  = 2;
   video_info.fullscreen   = true;
   video_info.rgb32        = false;

   d3d9_make_d3dpp(d3d, &video_info, &d3dpp);

   hr = app.InitShared((D3DDevice*)d3d->dev, &d3dpp,
         (PFN_XUITEXTURELOADER)XuiTextureLoader);

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

static void xui_free(void *data)
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
   d3d9_video_t             *d3d = (d3d9_video_t*)video_driver_get_ptr(false);

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

      font_parms.x     = msg_width;
      font_parms.y     = msg_height + (msg_offset * j);
      font_parms.scale = 21;

      video_driver_set_osd_msg(msg, &font_parms, NULL);
   }

end:
   string_list_free(list);
}

static void xui_frame(void *data, video_frame_info_t *video_info)
{
   XUIMessage msg;
   XUIMessageRender msgRender;
   D3DXMATRIX matOrigView;
   const char *message   = NULL;
   D3DVIEWPORT9 vp_full  = {0};
   d3d9_video_t *d3d     = (d3d9_video_t*)video_driver_get_ptr(false);

   if (!d3d)
      return;

   menu_display_set_viewport(video_info->width, video_info->height);

   app.RunFrame();
   XuiTimersRun();
   XuiRenderBegin( app.GetDC(), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

   XuiRenderGetViewTransform( app.GetDC(), &matOrigView );

   XuiMessageRender( &msg, &msgRender,
         app.GetDC(), 0xffffffff, XUI_BLEND_NORMAL );
   XuiSendMessage( app.GetRootObj(), &msg );

   XuiRenderSetViewTransform( app.GetDC(), &matOrigView );

#if 0
   /* TODO/FIXME - update this code */
   rarch_ctl(RARCH_CTL_MSG_QUEUE_PULL, &message);

   if (message)
      xui_render_message(message);
   else
   {
      rarch_ctl(RARCH_CTL_MSG_QUEUE_PULL, &message);

      if (message)
         xui_render_message(message);
   }
#endif

   XuiRenderEnd( app.GetDC() );

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void blit_line(int x, int y, const char *message, bool green)
{
}

static void xui_render_background(void)
{
   XuiElementSetShow(m_background, TRUE);
}

static void xui_render_messagebox(void *data, const char *message)
{
   msg_queue_clear(xui_msg_queue);
   msg_queue_push(xui_msg_queue, message, 2, 1);
}

static void xui_set_list_text(int index, const wchar_t* leftText,
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

static void xui_render(void *data,
      unsigned width, unsigned height,
      bool is_idle)
{
   size_t end, i, selection, fb_pitch;
   unsigned fb_width, fb_height;
   char title[PATH_MAX_LENGTH] = {0};
   const char *dir             = NULL;
   const char *label           = NULL;
   unsigned menu_type          = 0;
   bool              msg_force = menu_display_get_msg_force();
   settings_t *settings        = config_get_ptr();

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   if (
         menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL)
         && !msg_force
      )
      return;

   menu_display_unset_framebuffer_dirty_flag();
   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

   xui_render_background();

   if (XuiHandleIsValid(m_menutitle))
   {
      menu_animation_ctx_ticker_t ticker;
      menu_entries_get_title(title, sizeof(title));
      mbstowcs(strw_buffer, title, sizeof(strw_buffer) / sizeof(wchar_t));
      XuiTextElementSetText(m_menutitle, strw_buffer);

      /* Initial ticker configuration */
      ticker.type_enum = settings->uints.menu_ticker_type;
      ticker.spacer = NULL;

	  ticker.s        = title;
	  ticker.len      = RXUI_TERM_WIDTH(fb_width) - 3;
	  ticker.idx      = menu_animation_get_ticker_idx();
	  ticker.str      = title;
	  ticker.selected = true;

      menu_animation_ticker(&ticker);
   }

   if (XuiHandleIsValid(m_menutitle))
   {
      menu_entries_get_core_title(title, sizeof(title));
      mbstowcs(strw_buffer, title, sizeof(strw_buffer) / sizeof(wchar_t));
      XuiTextElementSetText(m_menutitlebottom, strw_buffer);
   }

   end = menu_entries_get_size();
   for (i = 0; i < end; i++)
   {
      menu_entry_t entry;
      const char *entry_path               = NULL;
      const char *entry_value              = NULL;
      wchar_t msg_right[PATH_MAX_LENGTH]   = {0};
      wchar_t msg_left[PATH_MAX_LENGTH]    = {0};

      menu_entry_init(&entry);
      menu_entry_get(&entry, 0, i, NULL, true);

      menu_entry_get_value(&entry, &entry_value);
      menu_entry_get_path(&entry, &entry_path);

      mbstowcs(msg_left,  entry_path,  sizeof(msg_left)  / sizeof(wchar_t));
      mbstowcs(msg_right, entry_value, sizeof(msg_right) / sizeof(wchar_t));
      xui_set_list_text(i, msg_left, msg_right);
   }

   selection = menu_navigation_get_selection();

   XuiListSetCurSelVisible(m_menulist, selection);

   if (menu_input_dialog_get_display_kb())
   {
      char msg[1024]    = {0};
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      xui_render_messagebox(NULL, msg);
   }
}

static void xui_populate_entries(void *data,
      const char *path,
      const char *label, unsigned i)
{
   size_t selection = menu_navigation_get_selection();
   XuiListSetCurSelVisible(m_menulist, selection);
}

static void xui_navigation_clear(void *data, bool pending_push)
{
   size_t selection = menu_navigation_get_selection();
   XuiListSetCurSelVisible(m_menulist, selection);
}

static void xui_navigation_set_visible(void *data)
{
   size_t selection = menu_navigation_get_selection();
   XuiListSetCurSelVisible(m_menulist, selection);
}

static void xui_navigation_alphabet(void *data, size_t *ptr_out)
{
   XuiListSetCurSelVisible(m_menulist, *ptr_out);
}

static void xui_list_insert(void *data,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
	  size_t list_size,
	  unsigned entry_type)
{
   wchar_t buf[PATH_MAX_LENGTH] = {0};

   XuiListInsertItems(m_menulist, list_size, 1);
   mbstowcs(buf, path, sizeof(buf) / sizeof(wchar_t));
   XuiListSetText(m_menulist, list_size, buf);
}

static void xui_list_free(
      file_list_t *list, size_t idx,
      size_t list_size)
{
   int x = XuiListGetItemCount( m_menulist );

   (void)idx;

   if (list_size > (unsigned)x)
      list_size = x;
   if (list_size > 0)
      XuiListDeleteItems(m_menulist, 0, list_size);
}

static void xui_list_clear(file_list_t *list)
{
   XuiListDeleteItems(m_menulist,
         0, XuiListGetItemCount(m_menulist));
}

static void xui_list_set_selection(void *data, file_list_t *list)
{
   if (list)
      XuiListSetCurSel(m_menulist,
            file_list_get_directory_ptr(list));
}

static int xui_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   switch (type)
   {
      case 0:
      default:
         return -1;
   }

   return 0;
}

menu_ctx_driver_t menu_ctx_xui = {
   NULL,
   xui_render_messagebox,
   generic_menu_iterate,
   xui_render,
   xui_frame,
   xui_init,
   xui_free,
   NULL,    /* context_reset */
   NULL,    /* context_destroy */
   xui_populate_entries,
   NULL,    /* toggle */
   xui_navigation_clear,
   xui_navigation_set_visible,
   xui_navigation_set_visible,
   xui_navigation_clear,
   xui_navigation_set_visible,
   xui_navigation_alphabet,
   xui_navigation_alphabet,
   generic_menu_init_list,
   xui_list_insert,
   NULL,          /* list_prepend */
   xui_list_free,
   xui_list_clear,
   NULL,          /* list_cache */
   NULL,          /* list_push */
   NULL,          /* list_get_selection */
   NULL,          /* list_get_size */
   NULL,          /* list_get_entry */
   xui_list_set_selection,
   NULL,          /* bind_init */
   NULL,          /* load_image */
   "xui",
   xui_environ,
   NULL,          /* update_thumbnail_path */
   NULL,          /* update_thumbnail_image */
   NULL,          /* refresh_thumbnail_image */
   NULL,          /* set_thumbnail_system */
   NULL,          /* get_thumbnail_system */
   NULL,          /* set_thumbnail_content */
   NULL,          /* osk_ptr_at_pos */
   NULL,          /* update_savestate_thumbnail_path */
   NULL,          /* update_savestate_thumbnail_image */
   NULL,          /* pointer_down */
   NULL,          /* pointer_up */
   NULL,          /* get_load_content_animation_data */
   generic_menu_entry_action
};
