/*  RetroArch - A frontend for libretro.
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

/* Textures the CPU writes, on either kind of Direct3D 9 device.
 *
 * A device made by Direct3D9Ex has no managed pool. There such a texture
 * is a default-pool texture with a system-memory twin hung off it as
 * private data, which lives and goes with it: a lock takes the twin, and
 * the unlock of a write uploads the region written with UpdateTexture -
 * the managed pool done by hand, with the GPU doing the copy. A device
 * without Ex has managed textures as before. Either way the texture is
 * the one handle the drivers pass around. Included by d3d9_common.h, so
 * each driver has these in its own translation unit. */

#ifndef _D3D9_TEX_H
#define _D3D9_TEX_H

#include <boolean.h>
#include <retro_inline.h>

#ifdef _XBOX
/* The Xbox 360 has managed textures alone */
static INLINE bool d3d9_dev_is_ex(LPDIRECT3DDEVICE9 dev)
{
   (void)dev;
   return false;
}

static INLINE HRESULT d3d9_tex_create(LPDIRECT3DDEVICE9 dev,
      UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT fmt,
      LPDIRECT3DTEXTURE9 *out)
{
   return IDirect3DDevice9_CreateTexture(dev, width, height, levels,
         usage, fmt, D3DPOOL_MANAGED, out, NULL);
}

static INLINE HRESULT d3d9_tex_lock(LPDIRECT3DTEXTURE9 tex, UINT level,
      D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
   return IDirect3DTexture9_LockRect(tex, level, lr, rect, flags);
}

static INLINE void d3d9_tex_unlock(LPDIRECT3DTEXTURE9 tex, UINT level,
      bool wrote)
{
   (void)wrote;
   IDirect3DTexture9_UnlockRect(tex, level);
}
#else
/* What an SDK older than Vista leaves out: the device's Ex interface,
 * asked for by its IID, and the mode CreateDeviceEx takes for a full
 * screen of its own */
static const GUID d3d9_iid_device_ex = { 0xb18b10ce, 0x2649, 0x405a,
   { 0x87, 0x0f, 0x95, 0xf7, 0x77, 0xd4, 0x31, 0x3a } };

/* The system-memory twin, as a texture's private data */
static const GUID d3d9_tex_twin_guid = { 0x5f1a7c2e, 0x9b4d, 0x4e63,
   { 0xa1, 0x58, 0x2c, 0x7e, 0x94, 0x0b, 0x3d, 0x61 } };

typedef struct d3d9_display_mode_ex
{
   UINT      Size;
   UINT      Width;
   UINT      Height;
   UINT      RefreshRate;
   D3DFORMAT Format;
   UINT      ScanLineOrdering;
} d3d9_display_mode_ex_t;

/* IDirect3D9Ex: IDirect3D9's table, then its own */
typedef struct d3d9_ex_vtbl
{
   IDirect3D9Vtbl base;
   void *GetAdapterModeCountEx;
   void *EnumAdapterModesEx;
   void *GetAdapterDisplayModeEx;
   HRESULT (STDMETHODCALLTYPE *CreateDeviceEx)(IDirect3D9 *self,
         UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
         D3DPRESENT_PARAMETERS *pp, d3d9_display_mode_ex_t *mode,
         IDirect3DDevice9 **dev);
   void *GetAdapterLUID;
} d3d9_ex_vtbl_t;

/* IDirect3DDevice9Ex: IDirect3DDevice9's table, then its own, as far
 * as the frame latency calls */
typedef struct d3d9_dev_ex_vtbl
{
   IDirect3DDevice9Vtbl base;
   void *SetConvolutionMonoKernel;
   void *ComposeRects;
   HRESULT (STDMETHODCALLTYPE *PresentEx)(IDirect3DDevice9 *self,
         const RECT *src, const RECT *dst, HWND wnd, const RGNDATA *dirty,
         DWORD flags);
   void *GetGPUThreadPriority;
   void *SetGPUThreadPriority;
   HRESULT (STDMETHODCALLTYPE *WaitForVBlank)(IDirect3DDevice9 *self,
         UINT swap_chain);
   void *CheckResourceResidency;
   HRESULT (STDMETHODCALLTYPE *SetMaximumFrameLatency)(
         IDirect3DDevice9 *self, UINT max_latency);
   HRESULT (STDMETHODCALLTYPE *GetMaximumFrameLatency)(
         IDirect3DDevice9 *self, UINT *max_latency);
} d3d9_dev_ex_vtbl_t;

/* IDirect3DSwapChain9Ex, and the present statistics it keeps, laid
 * out as Microsoft's d3d9types.h does: packed to 4 bytes on 32-bit
 * x86, natural elsewhere */
static const GUID d3d9_iid_swap_chain_ex = { 0x91886caf, 0x1c3d, 0x4d2e,
   { 0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x03 } };

#if defined(_X86_) || defined(_M_IX86) || defined(__i386__)
#pragma pack(push, 4)
#endif
typedef struct d3d9_present_stats
{
   UINT          PresentCount;
   UINT          PresentRefreshCount;
   UINT          SyncRefreshCount;
   LARGE_INTEGER SyncQPCTime;
   LARGE_INTEGER SyncGPUTime;
} d3d9_present_stats_t;
#if defined(_X86_) || defined(_M_IX86) || defined(__i386__)
#pragma pack(pop)
#endif

typedef struct d3d9_chain_ex_vtbl
{
   IDirect3DSwapChain9Vtbl base;
   HRESULT (STDMETHODCALLTYPE *GetLastPresentCount)(
         IDirect3DSwapChain9 *self, UINT *count);
   HRESULT (STDMETHODCALLTYPE *GetPresentStats)(
         IDirect3DSwapChain9 *self, d3d9_present_stats_t *stats);
   void *GetDisplayModeEx;
} d3d9_chain_ex_vtbl_t;

/* The flip model's swap effect, and the present that skips the
 * interval, which only it takes */
#define D3D9_SWAPEFFECT_FLIPEX      ((D3DSWAPEFFECT)5)
#define D3D9_CREATE_ENABLE_PRESENTSTATS 0x00004000L
#define D3D9_PRESENT_FORCEIMMEDIATE 0x00000100L

/* Whether @dev came from Direct3D9Ex: no managed pool, and system
 * memory a texture can be made over */
static INLINE bool d3d9_dev_is_ex(LPDIRECT3DDEVICE9 dev)
{
   void *ex = NULL;
   if (!dev || FAILED(IDirect3DDevice9_QueryInterface(dev,
               &d3d9_iid_device_ex, &ex)) || !ex)
      return false;
   ((IUnknown*)ex)->lpVtbl->Release((IUnknown*)ex);
   return true;
}

/* A texture the CPU writes, as CreateTexture's other arguments say */
static INLINE HRESULT d3d9_tex_create(LPDIRECT3DDEVICE9 dev,
      UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT fmt,
      LPDIRECT3DTEXTURE9 *out)
{
   LPDIRECT3DTEXTURE9 tex  = NULL;
   LPDIRECT3DTEXTURE9 twin = NULL;
   HRESULT hr;

   *out = NULL;
   if (!d3d9_dev_is_ex(dev))
      return IDirect3DDevice9_CreateTexture(dev, width, height, levels,
            usage, fmt, D3DPOOL_MANAGED, out, NULL);

   if (FAILED(hr = IDirect3DDevice9_CreateTexture(dev, width, height,
               levels, usage, fmt, D3DPOOL_DEFAULT, &tex, NULL)))
      return hr;
   /* A texture that makes its own mipmaps takes its top level */
   if (FAILED(hr = IDirect3DDevice9_CreateTexture(dev, width, height,
               (usage & D3DUSAGE_AUTOGENMIPMAP) ? 1 : levels, 0, fmt,
               D3DPOOL_SYSTEMMEM, &twin, NULL)))
   {
      IDirect3DTexture9_Release(tex);
      return hr;
   }
   hr = IDirect3DTexture9_SetPrivateData(tex, &d3d9_tex_twin_guid,
         (void*)twin, sizeof(IUnknown*), D3DSPD_IUNKNOWN);
   IDirect3DTexture9_Release(twin);
   if (FAILED(hr))
   {
      IDirect3DTexture9_Release(tex);
      return hr;
   }
   *out = tex;
   return D3D_OK;
}

/* @tex's system-memory twin, or NULL for a managed texture; held by
 * @tex, so not to be released */
static INLINE LPDIRECT3DTEXTURE9 d3d9_tex_twin(LPDIRECT3DTEXTURE9 tex)
{
   IUnknown *twin = NULL;
   DWORD     size = sizeof(twin);
   if (     !tex
         || FAILED(IDirect3DTexture9_GetPrivateData(tex,
               &d3d9_tex_twin_guid, &twin, &size))
         || !twin)
      return NULL;
   twin->lpVtbl->Release(twin);
   return (LPDIRECT3DTEXTURE9)twin;
}

/* As IDirect3DTexture9_LockRect, on the twin where there is one, which
 * takes no hint meant for a resource the GPU reads */
static INLINE HRESULT d3d9_tex_lock(LPDIRECT3DTEXTURE9 tex, UINT level,
      D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
   LPDIRECT3DTEXTURE9 twin = d3d9_tex_twin(tex);
   if (twin)
      return IDirect3DTexture9_LockRect(twin, level, lr, rect,
            flags & ~(D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE));
   return IDirect3DTexture9_LockRect(tex, level, lr, rect, flags);
}

/* Ends a lock of d3d9_tex_lock; @wrote sends what it covered up */
static INLINE void d3d9_tex_unlock(LPDIRECT3DTEXTURE9 tex, UINT level,
      bool wrote)
{
   LPDIRECT3DTEXTURE9 twin = d3d9_tex_twin(tex);
   if (!twin)
   {
      IDirect3DTexture9_UnlockRect(tex, level);
      return;
   }
   IDirect3DTexture9_UnlockRect(twin, level);
   if (wrote)
   {
      LPDIRECT3DDEVICE9 dev = NULL;
      if (SUCCEEDED(IDirect3DTexture9_GetDevice(tex, &dev)) && dev)
      {
         IDirect3DDevice9_UpdateTexture(dev,
               (IDirect3DBaseTexture9*)twin, (IDirect3DBaseTexture9*)tex);
         IDirect3DDevice9_Release(dev);
      }
   }
}

/* A vertex buffer the CPU rewrites whole now and then: managed before
 * Ex, dynamic there, and locked with what its kind takes */
static INLINE HRESULT d3d9_vbuf_create(LPDIRECT3DDEVICE9 dev, UINT length,
      DWORD usage, LPDIRECT3DVERTEXBUFFER9 *out)
{
   if (d3d9_dev_is_ex(dev))
      return IDirect3DDevice9_CreateVertexBuffer(dev, length,
            usage | D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, out, NULL);
   return IDirect3DDevice9_CreateVertexBuffer(dev, length, usage, 0,
         D3DPOOL_MANAGED, out, NULL);
}

static INLINE DWORD d3d9_vbuf_rewrite_flags(LPDIRECT3DVERTEXBUFFER9 vb)
{
   D3DVERTEXBUFFER_DESC desc;
   if (SUCCEEDED(IDirect3DVertexBuffer9_GetDesc(vb, &desc))
         && (desc.Usage & D3DUSAGE_DYNAMIC))
      return D3DLOCK_DISCARD;
   return 0;
}

#endif

#endif
