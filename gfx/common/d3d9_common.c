/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif
#include <string/stdstring.h>

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../../verbosity.h"

#include "d3d9_common.h"

#ifdef HAVE_D3DX
#include <d3dx9core.h>
#include <d3dx9tex.h>
#endif

#ifdef _XBOX
#include <xgraphics.h>
#endif

#include "win32_common.h"

#define FS_PRESENTINTERVAL(pp) ((pp)->PresentationInterval)

/* TODO/FIXME - static globals */
LPDIRECT3D9 g_pD3D9;
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d9_dll;
#ifdef HAVE_D3DX
static dylib_t g_d3d9x_dll;
#endif
static bool d3d9_dylib_initialized = false;
#endif

struct d3d9_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

typedef IDirect3D9 *(__stdcall *D3D9Create_t)(UINT);
#ifdef HAVE_D3DX
typedef HRESULT (__stdcall
      *D3D9CompileShader_t)(
         LPCSTR              pSrcData,
         UINT                srcDataLen,
         const D3DXMACRO     *pDefines,
         LPD3DXINCLUDE       pInclude,
         LPCSTR              pFunctionName,
         LPCSTR              pProfile,
         DWORD               Flags,
         LPD3DXBUFFER        *ppShader,
         LPD3DXBUFFER        *ppErrorMsgs,
         LPD3DXCONSTANTTABLE *ppConstantTable);
typedef HRESULT (__stdcall
      *D3D9CompileShaderFromFile_t)(
          LPCTSTR             pSrcFile,
    const D3DXMACRO           *pDefines,
          LPD3DXINCLUDE       pInclude,
          LPCSTR              pFunctionName,
          LPCSTR              pProfile,
          DWORD               Flags,
         LPD3DXBUFFER        *ppShader,
         LPD3DXBUFFER        *ppErrorMsgs,
         LPD3DXCONSTANTTABLE *ppConstantTable);

typedef HRESULT (__stdcall
    *D3D9CreateTextureFromFile_t)(
        LPDIRECT3DDEVICE9         pDevice,
        LPCSTR                    pSrcFile,
        UINT                      Width,
        UINT                      Height,
        UINT                      MipLevels,
        DWORD                     Usage,
        D3DFORMAT                 Format,
        D3DPOOL                   Pool,
        DWORD                     Filter,
        DWORD                     MipFilter,
        D3DCOLOR                  ColorKey,
        D3DXIMAGE_INFO*           pSrcInfo,
        PALETTEENTRY*             pPalette,
        LPDIRECT3DTEXTURE9*       ppTexture);

typedef HRESULT (__stdcall
    *D3D9XCreateFontIndirect_t)(
        LPDIRECT3DDEVICE9       pDevice,
        D3DXFONT_DESC*   pDesc,
        LPD3DXFONT*             ppFont);
#endif

#ifdef HAVE_D3DX
static D3D9XCreateFontIndirect_t    D3D9CreateFontIndirect;
static D3D9CreateTextureFromFile_t  D3D9CreateTextureFromFile;
static D3D9CompileShaderFromFile_t  D3D9CompileShaderFromFile;
static D3D9CompileShader_t          D3D9CompileShader;
#endif
static D3D9Create_t D3D9Create;

void *d3d9_create(void)
{
#ifdef _XBOX
   UINT ver = 0;
#else
   UINT ver = 31;
#endif
   return D3D9Create(ver);
}

#ifdef HAVE_DYNAMIC_D3D

#ifdef HAVE_D3DX
static const char *d3dx9_dll_list[] =
{
   "d3dx9_24.dll",
   "d3dx9_25.dll",
   "d3dx9_26.dll",
   "d3dx9_27.dll",
   "d3dx9_28.dll",
   "d3dx9_29.dll",
   "d3dx9_30.dll",
   "d3dx9_31.dll",
   "d3dx9_32.dll",
   "d3dx9_33.dll",
   "d3dx9_34.dll",
   "d3dx9_35.dll",
   "d3dx9_36.dll",
   "d3dx9_37.dll",
   "d3dx9_38.dll",
   "d3dx9_39.dll",
   "d3dx9_40.dll",
   "d3dx9_41.dll",
   "d3dx9_42.dll",
   "d3dx9_43.dll",
   NULL
};

static dylib_t dylib_load_d3d9x(void)
{
   dylib_t dll           = NULL;

   const char **dll_name = d3dx9_dll_list;

   while (!dll && *dll_name)
      dll = dylib_load(*dll_name++);

   return dll;
}
#endif

#endif

bool d3d9_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (d3d9_dylib_initialized)
      return true;
#ifdef HAVE_D3DX
   if (!(g_d3d9x_dll = dylib_load_d3d9x()))
      return false;
#endif
#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d9_dll     = dylib_load("d3d9d.dll")))
#endif
      if (!(g_d3d9_dll  = dylib_load("d3d9.dll")))
	      return false;
   D3D9Create                 = (D3D9Create_t)dylib_proc(g_d3d9_dll, "Direct3DCreate9");
#ifdef HAVE_D3DX
   D3D9CompileShaderFromFile  = (D3D9CompileShaderFromFile_t)dylib_proc(g_d3d9x_dll, "D3DXCompileShaderFromFile");
   D3D9CompileShader          = (D3D9CompileShader_t)dylib_proc(g_d3d9x_dll, "D3DXCompileShader");
#ifdef UNICODE
   D3D9CreateFontIndirect     = (D3D9XCreateFontIndirect_t)dylib_proc(g_d3d9x_dll, "D3DXCreateFontIndirectW");
#else
   D3D9CreateFontIndirect     = (D3D9XCreateFontIndirect_t)dylib_proc(g_d3d9x_dll, "D3DXCreateFontIndirectA");
#endif
   D3D9CreateTextureFromFile  = (D3D9CreateTextureFromFile_t)dylib_proc(g_d3d9x_dll, "D3DXCreateTextureFromFileExA");
#endif
#else
   D3D9Create                 = Direct3DCreate9;
#ifdef HAVE_D3DX
   D3D9CompileShaderFromFile  = D3DXCompileShaderFromFile;
   D3D9CompileShader          = D3DXCompileShader;
   D3D9CreateFontIndirect     = D3DXCreateFontIndirect;
   D3D9CreateTextureFromFile  = D3DXCreateTextureFromFileExA;
#endif
#endif

   if (!D3D9Create)
      goto error;

#ifdef HAVE_DYNAMIC_D3D
   d3d9_dylib_initialized = true;
#endif

   return true;

error:
   d3d9_deinitialize_symbols();
   return false;
}

void d3d9_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d9_dll)
      dylib_close(g_d3d9_dll);
#ifdef HAVE_D3DX
   if (g_d3d9x_dll)
      dylib_close(g_d3d9x_dll);
   g_d3d9x_dll        = NULL;
#endif
   g_d3d9_dll         = NULL;

   d3d9_dylib_initialized = false;
#endif
}

void *d3d9_texture_new_from_file(void *_dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
#ifdef HAVE_D3DX
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   void *buf             = NULL;
   if (SUCCEEDED(D3D9CreateTextureFromFile((LPDIRECT3DDEVICE9)dev,
               path, width, height, miplevels, usage, (D3DFORMAT)format,
               (D3DPOOL)pool, filter, mipfilter, color_key,
               (D3DXIMAGE_INFO*)src_info_data,
               palette, (struct IDirect3DTexture9**)&buf)))
      return buf;
#endif
   return NULL;
}

void *d3d9_texture_new(void *_dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   void *buf             = NULL;
#ifndef _XBOX
   if (want_mipmap)
      usage |= D3DUSAGE_AUTOGENMIPMAP;
#endif
   if (FAILED(IDirect3DDevice9_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format,
               (D3DPOOL)pool,
               (struct IDirect3DTexture9**)&buf, NULL)))
      return NULL;
   return buf;
}

void *d3d9_vertex_buffer_new(void *_dev,
      unsigned length, unsigned usage,
      unsigned fvf, INT32 pool, void *handle)
{
   void              *buf = NULL;
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
               dev, length, usage, fvf,
               (D3DPOOL)pool,
               (LPDIRECT3DVERTEXBUFFER9*)&buf, NULL)))
      return buf;
   return NULL;
}

void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER9 buf =
         (LPDIRECT3DVERTEXBUFFER9)vertex_data;
      IDirect3DVertexBuffer9_Release(buf);
      buf = NULL;
   }

   if (vertex_declaration)
   {
      LPDIRECT3DVERTEXDECLARATION9 vertex_decl =
         (LPDIRECT3DVERTEXDECLARATION9)vertex_declaration;
      IDirect3DVertexDeclaration9_Release(vertex_decl);
      vertex_decl = NULL;
   }
}

static bool d3d9_create_device_internal(
      void *data,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *_d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   LPDIRECT3D9       d3d = (LPDIRECT3D9)_d3d;
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev &&
         SUCCEEDED(IDirect3D9_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice9**)dev)))
      return true;

   return false;
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d9_create_device_internal(dev,
            (D3DPRESENT_PARAMETERS*)d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d9_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

bool d3d9_reset(void *data, void *d3dpp)
{
   const char       *err = NULL;
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev && IDirect3DDevice9_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) == D3D_OK)
      return true;
#ifndef _XBOX
   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");
   /* Try to recreate the device completely. */
   switch (IDirect3DDevice9_TestCooperativeLevel(dev))
   {
      case D3DERR_DEVICELOST:
         err = "DEVICELOST";
         break;

      case D3DERR_DEVICENOTRESET:
         err = "DEVICENOTRESET";
         break;

      case D3DERR_DRIVERINTERNALERROR:
         err = "DRIVERINTERNALERROR";
         break;

      default:
         err = "Unknown";
   }
   RARCH_WARN("[D3D]: recovering from dead state: (%s).\n", err);
#endif
   return false;
}

bool d3d9x_create_font_indirect(void *_dev,
      void *desc, void **font_data)
{
#ifdef HAVE_D3DX
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (SUCCEEDED(D3D9CreateFontIndirect(
               dev, (D3DXFONT_DESC*)desc,
               (struct ID3DXFont**)font_data)))
      return true;
#endif

   return false;
}

bool d3d9x_compile_shader(
      const char *src,
      unsigned src_data_len,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable)
{
#if defined(HAVE_D3DX)
   if (D3D9CompileShader)
      if (D3D9CompileShader(
               (LPCTSTR)src,
               (UINT)src_data_len,
               (const D3DXMACRO*)pdefines,
               (LPD3DXINCLUDE)pinclude,
               (LPCSTR)pfunctionname,
               (LPCSTR)pprofile,
               (DWORD)flags,
               (LPD3DXBUFFER*)ppshader,
               (LPD3DXBUFFER*)pperrormsgs,
               (LPD3DXCONSTANTTABLE*)ppconstanttable) >= 0)
         return true;
#endif
   return false;
}

void d3d9x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->DrawText(font, (LPD3DXSPRITE)sprite_data,
            (LPCTSTR)string_data, count, (LPRECT)rect_data,
            (DWORD)format, (D3DCOLOR)color);
#endif
}

void d3d9x_font_release(void *data)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->Release(font);
#endif
}

void d3d9x_font_get_text_metrics(void *data, void *metrics)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->GetTextMetrics(font, (TEXTMETRICA*)metrics);
#endif
}

bool d3d9x_compile_shader_from_file(
      const char *src,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable)
{
#if defined(HAVE_D3DX)
   if (D3D9CompileShaderFromFile)
      if (D3D9CompileShaderFromFile(
               (LPCTSTR)src,
               (const D3DXMACRO*)pdefines,
               (LPD3DXINCLUDE)pinclude,
               (LPCSTR)pfunctionname,
               (LPCSTR)pprofile,
               (DWORD)flags,
               (LPD3DXBUFFER*)ppshader,
               (LPD3DXBUFFER*)pperrormsgs,
               (LPD3DXCONSTANTTABLE*)ppconstanttable) >= 0)
         return true;
#endif
   return false;
}

void *d3d9x_constant_table_get_constant_by_name(void *_tbl,
      void *_handle, void *_name)
{
#if defined(HAVE_D3DX)
   D3DXHANDLE        handle     = (D3DXHANDLE)_handle;
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)_tbl;
   LPCSTR              name     = (LPCSTR)_name;
   if (consttbl && handle && name)
      return (void*)consttbl->lpVtbl->GetConstantByName(consttbl,
            handle, name);
#endif
   return NULL;
}

void d3d9x_constant_table_set_float_array(LPDIRECT3DDEVICE9 dev,
      void *p, void *_handle, const void *_pf, unsigned count)
{
#if defined(HAVE_D3DX)
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)p;
   D3DXHANDLE           handle  = (D3DXHANDLE)_handle;
   CONST FLOAT              *pf = (CONST FLOAT*)_pf;
   if (consttbl && dev)
      consttbl->lpVtbl->SetFloatArray(consttbl, dev, handle, pf,
            (UINT)count);
#endif
}

void d3d9x_constant_table_set_defaults(LPDIRECT3DDEVICE9 dev,
      void *p)
{
#if defined(HAVE_D3DX)
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)p;
   if (consttbl && dev)
   {
      if (consttbl->lpVtbl->SetDefaults)
         consttbl->lpVtbl->SetDefaults(consttbl, dev);
   }
#endif
}

void d3d9x_constant_table_set_matrix(LPDIRECT3DDEVICE9 dev,
      void *p,
      void *data, const void *_matrix)
{
#if defined(HAVE_D3DX)
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)p;
   D3DXHANDLE        handle     = (D3DXHANDLE)data;
   const D3DXMATRIX  *matrix    = (const D3DXMATRIX*)_matrix;
   if (consttbl && dev && handle)
      consttbl->lpVtbl->SetMatrix(consttbl, dev, handle, matrix);
#endif
}

const bool d3d9x_constant_table_set_float(void *p,
      void *a, void *b, float val)
{
#if defined(HAVE_D3DX)
   LPDIRECT3DDEVICE9    dev     = (LPDIRECT3DDEVICE9)a;
   D3DXHANDLE        handle     = (D3DXHANDLE)b;
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)p;
   if (consttbl && dev && handle &&
         consttbl->lpVtbl->SetFloat(
            consttbl, dev, handle, val) == D3D_OK)
      return true;
#endif
   return false;
}

#ifdef _XBOX
static bool d3d9_is_windowed_enable(bool info_fullscreen)
{
   return false;
}

static D3DFORMAT d3d9_get_color_format_backbuffer(bool rgb32)
{
   if (rgb32)
      return D3DFMT_X8R8G8B8;
   return D3D9_RGB565_FORMAT;
}

bool d3d9_has_windowed(void *data) { return false; }

static void d3d9_get_video_size(d3d9_video_t *d3d,
      unsigned *width, unsigned *height)
{
   XVIDEO_MODE video_mode;

   XGetVideoMode(&video_mode);

   *width                       = video_mode.dwDisplayWidth;
   *height                      = video_mode.dwDisplayHeight;

   d3d->resolution_hd_enable    = false;

   if(video_mode.fIsHiDef)
   {
      *width                    = 1280;
      *height                   = 720;
      d3d->resolution_hd_enable = true;
   }
   else
   {
      *width                    = 640;
      *height                   = 480;
   }

   d3d->widescreen_mode         = video_mode.fIsWideScreen;
}

static D3DFORMAT d3d9_get_color_format_front_buffer(void)
{
   return D3DFMT_LE_X8R8G8B8;
}
#else
static bool d3d9_is_windowed_enable(bool info_fullscreen)
{
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
   return false;
}

static D3DFORMAT d3d9_get_color_format_backbuffer(
      bool rgb32, bool windowed)
{
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (IDirect3D9_GetAdapterDisplayMode(g_pD3D9, 0, &display_mode))
         return display_mode.Format;
   }
   return D3DFMT_X8R8G8B8;
}

bool d3d9_has_windowed(void *data) { return true; }
#endif

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp)
{
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
#ifdef _XBOX
   /* TODO/FIXME - get rid of global state dependencies. */
   global_t *global               = global_get_ptr();
   int gamma_enable               = global ?
      global->console.screen.gamma_correction : 0;
#endif
   bool windowed_enable           = d3d9_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings         = config_get_ptr();
      unsigned video_swap_interval = runloop_get_video_swap_interval(
            settings->uints.video_swap_interval);

      switch (video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;

#ifdef _XBOX
   d3dpp->BackBufferFormat        = d3d9_get_color_format_backbuffer(
         info->rgb32);
   d3dpp->FrontBufferFormat       = d3d9_get_color_format_front_buffer();

   if (gamma_enable)
   {
      d3dpp->BackBufferFormat     = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->BackBufferFormat);
      d3dpp->FrontBufferFormat    = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->FrontBufferFormat);
   }
#else
   d3dpp->BackBufferFormat        = d3d9_get_color_format_backbuffer(
         info->rgb32, windowed_enable);
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      unsigned width  = 0;
      unsigned height = 0;
      d3d9_get_video_size(d3d, &width, &height);
      video_driver_set_size(width, height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   if (!d3d->widescreen_mode)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   d3dpp->MultiSampleQuality      = 0;
#endif
}


void d3d9_log_info(const struct LinkInfo *info)
{
   RARCH_LOG("[D3D9]: Render pass info:\n");
   RARCH_LOG("\tTexture width: %u\n", info->tex_w);
   RARCH_LOG("\tTexture height: %u\n", info->tex_h);

   RARCH_LOG("\tScale type (X): ");

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_x);
         break;
   }

   RARCH_LOG("\tScale type (Y): ");

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_y);
         break;
   }

   RARCH_LOG("\tBilinear filter: %s\n",
         info->pass->filter == RARCH_FILTER_LINEAR ? "true" : "false");
}

static void d3d9_init_singlepass(d3d9_video_t *d3d)
{
   struct video_shader_pass *pass        = NULL;

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes                    = 1;

   pass                                  = (struct video_shader_pass*)
      &d3d->shader.pass[0];

   pass->fbo.valid                       = true;
   pass->fbo.scale_y                     = 1.0;
   pass->fbo.type_y                      = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                     = pass->fbo.scale_y;
   pass->fbo.type_x                      = pass->fbo.type_y;

   if (!string_is_empty(d3d->shader_path))
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));
}

static bool d3d9_init_multipass(d3d9_video_t *d3d, const char *shader_path)
{
   unsigned i;
   bool            use_extra_pass = false;
   struct video_shader_pass *pass = NULL;

   memset(&d3d->shader, 0, sizeof(d3d->shader));

   if (!video_shader_load_preset_into_shader(shader_path, &d3d->shader))
      return false;

   RARCH_LOG("[D3D9]: Found %u shaders.\n", d3d->shader.passes);

   for (i = 0; i < d3d->shader.passes; i++)
   {
      if (d3d->shader.pass[i].fbo.valid)
         continue;

      d3d->shader.pass[i].fbo.scale_y = 1.0f;
      d3d->shader.pass[i].fbo.scale_x = 1.0f;
      d3d->shader.pass[i].fbo.type_x  = RARCH_SCALE_INPUT;
      d3d->shader.pass[i].fbo.type_y  = RARCH_SCALE_INPUT;
   }

   use_extra_pass       = d3d->shader.passes < GFX_MAX_SHADERS &&
      d3d->shader.pass[d3d->shader.passes - 1].fbo.valid;

   if (use_extra_pass)
   {
      d3d->shader.passes++;
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
      pass->filter      = RARCH_FILTER_UNSPEC;
   }
   else
   {
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
   }

   return true;
}

bool d3d9_process_shader(d3d9_video_t *d3d)
{
   const char *shader_path = d3d->shader_path;
   if (!string_is_empty(shader_path))
   {
      RARCH_ERR("[D3D9]: Failed to parse shader preset.\n");
      return d3d9_init_multipass(d3d, shader_path);
   }

   d3d9_init_singlepass(d3d);
   return true;
}

void d3d9_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   d3d9_video_t *d3d   = (d3d9_video_t*)data;

   video_driver_get_size(&width, &height);

   vp->x               = d3d->final_viewport.X;
   vp->y               = d3d->final_viewport.Y;
   vp->width           = d3d->final_viewport.Width;
   vp->height          = d3d->final_viewport.Height;

   vp->full_width      = width;
   vp->full_height     = height;
}

static void d3d9_set_font_rect(
      d3d9_video_t *d3d,
      const struct font_params *params)
{
   settings_t *settings             = config_get_ptr();
   float pos_x                      = settings->floats.video_msg_pos_x;
   float pos_y                      = settings->floats.video_msg_pos_y;
   float font_size                  = settings->floats.video_font_size;

   if (params)
   {
      pos_x                       = params->x;
      pos_y                       = params->y;
      font_size                  *= params->scale;
   }

   d3d->font_rect.left            = d3d->video_info.width * pos_x;
   d3d->font_rect.right           = d3d->video_info.width;
   d3d->font_rect.top             = (1.0f - pos_y) * d3d->video_info.height - font_size;
   d3d->font_rect.bottom          = d3d->video_info.height;

   d3d->font_rect_shifted         = d3d->font_rect;
   d3d->font_rect_shifted.left   -= 2;
   d3d->font_rect_shifted.right  -= 2;
   d3d->font_rect_shifted.top    += 2;
   d3d->font_rect_shifted.bottom += 2;
}


void d3d9_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   int x               = 0;
   int y               = 0;
   d3d9_video_t *d3d   = (d3d9_video_t*)data;

   d3d9_calculate_rect(d3d, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->final_viewport.X      = x;
   d3d->final_viewport.Y      = y;
   d3d->final_viewport.Width  = width;
   d3d->final_viewport.Height = height;
   d3d->final_viewport.MinZ   = 0.0f;
   d3d->final_viewport.MaxZ   = 1.0f;

   d3d9_set_font_rect(d3d, NULL);
}

#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
void d3d9_overlay_render(d3d9_video_t *d3d,
      unsigned width,
      unsigned height,
      overlay_t *overlay, bool force_linear)
{
   D3DTEXTUREFILTERTYPE filter_type;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   LPDIRECT3DDEVICE9 dev;
   struct video_viewport vp;
   void *verts;
   unsigned i;
   Vertex vert[4];
   D3DVERTEXELEMENT9 vElems[4] = {
      {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };

   if (!overlay || !overlay->tex)
      return;

   dev                 = d3d->dev;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d9_vertex_buffer_new(
      dev, sizeof(vert), D3DUSAGE_WRITEONLY,
#ifdef _XBOX
     0,
#else
      D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
#endif
      D3DPOOL_MANAGED, NULL);

     if (!overlay->vert_buf)
        return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z       = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d9_viewport_info(d3d, &vp);

   vert[0].x      = overlay->vert_coords[0];
   vert[1].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[2].x      = overlay->vert_coords[0];
   vert[3].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[0].y      = overlay->vert_coords[1];
   vert[1].y      = overlay->vert_coords[1];
   vert[2].y      = overlay->vert_coords[1] + overlay->vert_coords[3];
   vert[3].y      = overlay->vert_coords[1] + overlay->vert_coords[3];

   vert[0].u      = overlay->tex_coords[0];
   vert[1].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[2].u      = overlay->tex_coords[0];
   vert[3].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[0].v      = overlay->tex_coords[1];
   vert[1].v      = overlay->tex_coords[1];
   vert[2].v      = overlay->tex_coords[1] + overlay->tex_coords[3];
   vert[3].v      = overlay->tex_coords[1] + overlay->tex_coords[3];

   IDirect3DVertexBuffer9_Lock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf, 0, 0, &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   IDirect3DVertexBuffer9_Unlock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf);

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);

   /* set vertex declaration for overlay. */
   d3d9_vertex_declaration_new(dev, &vElems, (void**)&vertex_decl);
   IDirect3DDevice9_SetVertexDeclaration(dev, vertex_decl);
   IDirect3DVertexDeclaration9_Release(vertex_decl);

   IDirect3DDevice9_SetStreamSource(dev, 0,
         (LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf, 0, sizeof(*vert));

   if (overlay->fullscreen)
   {
      D3DVIEWPORT9 vp_full;

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)&vp_full);
   }

   filter_type = D3DTEXF_LINEAR;

   if (!force_linear)
   {
      settings_t *settings    = config_get_ptr();
      bool menu_linear_filter = settings->bools.menu_linear_filter;
      if (!menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   IDirect3DDevice9_SetTexture(dev, 0,(IDirect3DBaseTexture9*)overlay->tex);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSU,
         D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSV,
         D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER, filter_type);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER, filter_type);
   IDirect3DDevice9_BeginScene(dev);
   IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(dev);

   /* Restore previous state. */
   IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, false);
   IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)&d3d->final_viewport);
}

void d3d9_free_overlay(d3d9_video_t *d3d, overlay_t *overlay)
{
   if ((LPDIRECT3DTEXTURE9)overlay->tex)
      IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)overlay->tex);
   d3d9_vertex_buffer_free(overlay->vert_buf, NULL);
}
#endif

void d3d9_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

void d3d9_apply_state_changes(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

void d3d9_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font)
{
   d3d9_video_t          *d3d = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9     dev  = d3d->dev;
   const struct font_params *d3d_font_params = (const
         struct font_params*)params;

   d3d9_set_font_rect(d3d, d3d_font_params);
   IDirect3DDevice9_BeginScene(dev);
   font_driver_render_msg(d3d,
         msg, d3d_font_params, font);
   IDirect3DDevice9_EndScene(dev);
}

void d3d9_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   if (    !d3d->menu->tex            ||
            d3d->menu->tex_w != width ||
            d3d->menu->tex_h != height)
   {
      IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)d3d->menu->tex);

      d3d->menu->tex = d3d9_texture_new(d3d->dev,
            width, height, 1,
            0, D3D9_ARGB8888_FORMAT,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D9]: Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
   }

   d3d->menu->alpha_mod = alpha;

   IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)d3d->menu->tex,
         0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   {
      unsigned h, w;
      if (rgb32)
      {
         uint8_t        *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;

         for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
         {
            memcpy(dst, src, width * sizeof(uint32_t));
            memset(dst + width * sizeof(uint32_t), 0,
                  d3dlr.Pitch - width * sizeof(uint32_t));
         }
      }
      else
      {
         uint32_t       *dst = (uint32_t*)d3dlr.pBits;
         const uint16_t *src = (const uint16_t*)frame;

         for (h = 0; h < height; h++, 
               dst += d3dlr.Pitch >> 2,
               src += width)
         {
            for (w = 0; w < width; w++)
            {
               uint16_t c = src[w];
               uint32_t r = (c >> 12) & 0xf;
               uint32_t g = (c >>  8) & 0xf;
               uint32_t b = (c >>  4) & 0xf;
               uint32_t a = (c >>  0) & 0xf;
               r          = ((r << 4) | r) << 16;
               g          = ((g << 4) | g) <<  8;
               b          = ((b << 4) | b) <<  0;
               a          = ((a << 4) | a) << 24;
               dst[w]     = r | g | b | a;
            }
         }
      }
   }

   IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)d3d->menu->tex, 0);
}

void d3d9_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d9_video_texture_load_d3d(
      struct d3d9_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   LPDIRECT3DTEXTURE9 tex   = NULL;
   unsigned usage           = 0;
   bool want_mipmap         = false;
   d3d9_video_t *d3d        = (d3d9_video_t*)info->userdata;
   struct texture_image *ti = (struct texture_image*)info->data;

   if (!ti)
      return;

   if((info->type == TEXTURE_FILTER_MIPMAP_LINEAR) ||
      (info->type == TEXTURE_FILTER_MIPMAP_NEAREST))
      want_mipmap        = true;

   tex = (LPDIRECT3DTEXTURE9)d3d9_texture_new(d3d->dev,
               ti->width, ti->height, 0,
               usage, D3D9_ARGB8888_FORMAT,
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, want_mipmap);

   if (!tex)
      return;

   IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;
      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
   }
   IDirect3DTexture9_UnlockRect(tex, 0);

   *id = (uintptr_t)tex;
}

#ifdef HAVE_THREADS
static int d3d9_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d9_texture_info *info = (struct d3d9_texture_info*)data;
   if (!info)
      return 0;
   d3d9_video_texture_load_d3d(info, &id);
   return id;
}
#endif

uintptr_t d3d9_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;
   struct d3d9_texture_info info;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

#ifdef HAVE_THREADS
   if (threaded)
      return video_thread_texture_load(&info,
            d3d9_video_texture_load_wrap_d3d);
#endif

   d3d9_video_texture_load_d3d(&info, &id);
   return id;
}

void d3d9_unload_texture(void *data, 
      bool threaded, uintptr_t id)
{
   LPDIRECT3DTEXTURE9 texid;
   if (!id)
      return;

   texid = (LPDIRECT3DTEXTURE9)id;
   IDirect3DTexture9_Release(texid);
}

void d3d9_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   win32_show_cursor(data, !fullscreen);
#endif
}

static INLINE bool d3d9_device_get_render_target_data(
      LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DSURFACE9 src, LPDIRECT3DSURFACE9 dst)
{
#ifndef _XBOX
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
               dev, src, dst)))
      return true;
#endif
   return false;
}

bool d3d9_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned width, height;
   D3DLOCKED_RECT rect;
   LPDIRECT3DSURFACE9 target = NULL;
   LPDIRECT3DSURFACE9 dest   = NULL;
   bool ret                  = true;
   d3d9_video_t *d3d         = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9 d3dr    = d3d->dev;

   video_driver_get_size(&width, &height);

   if (
         !d3d9_device_get_render_target(d3dr, 0, (void**)&target)     ||
         !d3d9_device_create_offscreen_plain_surface(d3dr, width, height,
            D3D9_XRGB8888_FORMAT,
            D3DPOOL_SYSTEMMEM, (void**)&dest, NULL) ||
         !d3d9_device_get_render_target_data(d3dr, target, dest)
         )
   {
      ret = false;
      goto end;
   }

   IDirect3DSurface9_LockRect(dest, &rect, NULL, D3DLOCK_READONLY);

   {
      unsigned x, y;
      unsigned pitchpix       = rect.Pitch / 4;
      const uint32_t *pixels  = (const uint32_t*)rect.pBits;

      pixels                 += d3d->final_viewport.X;
      pixels                 += (d3d->final_viewport.Height - 1) * pitchpix;
      pixels                 -= d3d->final_viewport.Y * pitchpix;

      for (y = 0; y < d3d->final_viewport.Height; y++, pixels -= pitchpix)
      {
         for (x = 0; x < d3d->final_viewport.Width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      IDirect3DSurface9_UnlockRect(dest);
   }

end:
   if (target)
      IDirect3DSurface9_Release(target);
   if (dest)
      IDirect3DSurface9_Release(dest);
   return ret;
}

void d3d9_calculate_rect(d3d9_video_t *d3d,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   float device_aspect   = (float)*width / *height;
   settings_t *settings  = config_get_ptr();
   bool scale_integer    = settings->bools.video_scale_integer;

   video_driver_get_size(width, height);

   *x                   = 0;
   *y                   = 0;

   if (scale_integer && !force_full)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_viewport_get_scaled_integer(&vp,
            *width,
            *height,
            video_driver_get_aspect_ratio(),
            d3d->keep_aspect);

      *x                          = vp.x;
      *y                          = vp.y;
      *width                      = vp.width;
      *height                     = vp.height;
   }
   else if (d3d->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom = video_viewport_get_custom();

         *x          = custom->x;
         *y          = custom->y;
         *width      = custom->width;
         *height     = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta        = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            *x           = (int)(roundf(*width * (0.5f - delta)));
            *width       = (unsigned)(roundf(2.0f * (*width) * delta));
         }
         else
         {
            delta        = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            *y           = (int)(roundf(*height * (0.5f - delta)));
            *height      = (unsigned)(roundf(2.0f * (*height) * delta));
         }
      }
   }
}

void d3d9_set_rotation(void *data, unsigned rot)
{
   d3d9_video_t        *d3d = (d3d9_video_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!d3d)
      return;

   d3d->dev_rotation = rot;
}

void d3d9_blit_to_texture(
      LPDIRECT3DTEXTURE9 tex,
      const void *frame,
      unsigned tex_width,  unsigned tex_height,
      unsigned width,      unsigned height,
      unsigned last_width, unsigned last_height,
      unsigned pitch, unsigned pixel_size)
{
   unsigned y;
   D3DLOCKED_RECT d3dlr;
   d3dlr.Pitch  = 0;
   d3dlr.pBits  = NULL;

   if ((last_width != width || last_height != height))
   {
      IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, tex_height * d3dlr.Pitch);
      IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)tex, 0);
   }

   IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, 0);
   for (y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t      *out = (uint8_t*)d3dlr.pBits   + y * d3dlr.Pitch;
      memcpy(out, in, width * pixel_size);
   }
   IDirect3DTexture9_UnlockRect(tex, 0);
}

#ifdef HAVE_OVERLAY
void d3d9_free_overlays(d3d9_video_t *d3d)
{
   unsigned i;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d9_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}

static void d3d9_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0] = x;
   d3d->overlays[index].tex_coords[1] = y;
   d3d->overlays[index].tex_coords[2] = w;
   d3d->overlays[index].tex_coords[3] = h;
}

static void d3d9_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d9_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   overlay_t *new_overlays            = NULL;
   d3d9_video_t *d3d                  = (d3d9_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)
      image_data;

   if (!d3d)
      return false;

   d3d9_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d9_texture_new(d3d->dev,
                  width, height, 1,
                  0,
                  D3D9_ARGB8888_FORMAT,
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D9]: Failed to create overlay texture\n");
         return false;
      }

      IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
      }
      IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0);

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d9_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d9_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d9_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d9_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d9_overlay_interface = {
   d3d9_overlay_enable,
   d3d9_overlay_load,
   d3d9_overlay_tex_geom,
   d3d9_overlay_vertex_geom,
   d3d9_overlay_full_screen,
   d3d9_overlay_set_alpha,
};

void d3d9_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_overlay_interface;
}
#endif
