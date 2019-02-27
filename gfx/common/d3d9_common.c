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

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
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

static UINT d3d9_SDKVersion = 0;

#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d9_dll;
#ifdef HAVE_D3DX
static dylib_t g_d3d9x_dll;
#endif
static bool d3d9_dylib_initialized = false;
#endif

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
   return D3D9Create(d3d9_SDKVersion);
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

#if defined(DEBUG) || defined(_DEBUG)
   g_d3d9_dll     = dylib_load("d3d9d.dll");
   if(!g_d3d9_dll)
#endif
      g_d3d9_dll  = dylib_load("d3d9.dll");
#ifdef HAVE_D3DX
   g_d3d9x_dll    = dylib_load_d3d9x();

   if (!g_d3d9x_dll)
      return false;
#endif

   if (!g_d3d9_dll)
      return false;
#endif

   d3d9_SDKVersion            = 31;
#ifdef HAVE_DYNAMIC_D3D
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

#ifdef _XBOX
   d3d9_SDKVersion          = 0;
#endif
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

#ifdef HAVE_D3DX
static void *d3d9_texture_new_from_file(
      void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   void *buf  = NULL;
   if (FAILED(D3D9CreateTextureFromFile((LPDIRECT3DDEVICE9)dev,
               path, width, height, miplevels, usage, format,
               (D3DPOOL)pool, filter, mipfilter, color_key,
               (D3DXIMAGE_INFO*)src_info_data,
               palette, (struct IDirect3DTexture9**)&buf)))
      return NULL;
   return buf;
}
#endif

void *d3d9_texture_new(void *_dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   void *buf             = NULL;

   if (path)
   {
#ifdef HAVE_D3DX
      return d3d9_texture_new_from_file(_dev,
            path, width, height, miplevels,
            usage, (D3DFORMAT)format,
            (D3DPOOL)pool, filter, mipfilter,
            color_key, src_info_data, palette);
#else
      return NULL;
#endif
   }

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

#ifndef _XBOX
   if (usage == 0)
      if (IDirect3DDevice9_GetSoftwareVertexProcessing(dev))
         usage = D3DUSAGE_SOFTWAREPROCESSING;
#endif

   if (FAILED(IDirect3DDevice9_CreateVertexBuffer(
               dev, length, usage, fvf,
               (D3DPOOL)pool,
               (LPDIRECT3DVERTEXBUFFER9*)&buf, NULL)))
      return NULL;

   return buf;
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
      d3d9_vertex_declaration_free(vertex_decl);
      vertex_decl = NULL;
   }
}

static bool d3d9_reset_internal(void *data,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev &&
         IDirect3DDevice9_Reset(dev, d3dpp) == D3D_OK)
      return true;

   return false;
}

static HRESULT d3d9_test_cooperative_level(void *data)
{
#ifndef _XBOX
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev)
      return IDirect3DDevice9_TestCooperativeLevel(dev);
#endif
   return E_FAIL;
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

bool d3d9_reset(void *dev, void *d3dpp)
{
   const char *err = NULL;

   if (d3d9_reset_internal(dev, (D3DPRESENT_PARAMETERS*)d3dpp))
      return true;

   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");

#ifndef _XBOX
   /* Try to recreate the device completely. */
   switch (d3d9_test_cooperative_level(dev))
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

void d3d9x_buffer_release(void *data)
{
#ifdef HAVE_D3DX
   LPD3DXBUFFER p = (LPD3DXBUFFER)data;
   if (!p)
      return;

   p->lpVtbl->Release(p);
#endif
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

const void *d3d9x_get_buffer_ptr(void *data)
{
#if defined(HAVE_D3DX)
   ID3DXBuffer *listing = (ID3DXBuffer*)data;
   if (listing)
      return listing->lpVtbl->GetBufferPointer(listing);
#endif
   return NULL;
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
