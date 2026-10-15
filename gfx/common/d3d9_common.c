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
#include <stdlib.h>

#include <string/stdstring.h>
#include <retro_timers.h>

#include "../../verbosity.h"

#include "d3d9_common.h"

#ifdef _XBOX
#include <xgraphics.h>

#include "../../configuration.h"
#endif

#include "win32_common.h"

#define FS_PRESENTINTERVAL(pp) ((pp)->PresentationInterval)

/* TODO/FIXME - static globals */
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d9_dll;
static bool d3d9_dylib_initialized = false;
#endif

typedef IDirect3D9 *(__stdcall *D3D9Create_t)(UINT);
static D3D9Create_t D3D9Create;
#ifdef HAVE_DYNAMIC_D3D
/* Direct3D9Ex, from Vista on: resolved where there is one, and the
 * devices made without it otherwise */
typedef HRESULT (__stdcall *D3D9CreateEx_t)(UINT, IDirect3D9**);
static D3D9CreateEx_t D3D9CreateEx;
#endif
/* Whether the object d3d9_create last made is Direct3D9Ex's */
static bool d3d9_object_ex;

void *d3d9_create(void)
{
#ifdef _XBOX
   UINT ver = 0;
#else
   UINT ver = 31;
#endif
#ifdef HAVE_DYNAMIC_D3D
   /* RETROARCH_D3D9EX=0 keeps the devices of a system without it, so
    * that path stays tested where Ex is there */
   const char *want = getenv("RETROARCH_D3D9EX");
   if (D3D9CreateEx && !(want && want[0] == '0'))
   {
      IDirect3D9 *obj = NULL;
      if (SUCCEEDED(D3D9CreateEx(32, &obj)) && obj)
      {
         d3d9_object_ex = true;
         return obj;
      }
   }
#endif
   d3d9_object_ex = false;
   return D3D9Create(ver);
}

bool d3d9_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (d3d9_dylib_initialized)
      return true;
#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d9_dll  = dylib_load("d3d9d.dll")))
#endif
   if (!(g_d3d9_dll  = dylib_load("d3d9.dll")))
   {
      /* On a system where the D3D9 user-mode runtime is missing, the
       * caller would otherwise see only the generic "Cannot open video
       * driver" message. Surface the real cause here. */
      RARCH_ERR("[D3D9] Failed to load d3d9.dll: %s\n",
            dylib_error() ? dylib_error() : "(no error reported)");
      RARCH_ERR("[D3D9] The DirectX 9 runtime is not present on this "
            "system. Install it or pick a different video driver.\n");
      return false;
   }
   if (!(D3D9Create = (D3D9Create_t)dylib_proc(g_d3d9_dll, "Direct3DCreate9")))
      RARCH_ERR("[D3D9] d3d9.dll does not export Direct3DCreate9: %s\n",
            dylib_error() ? dylib_error() : "(no error reported)");
   D3D9CreateEx = (D3D9CreateEx_t)dylib_proc(g_d3d9_dll, "Direct3DCreate9Ex");
#else
   D3D9Create                 = Direct3DCreate9;
#endif

   if (!D3D9Create)
   {
      d3d9_deinitialize_symbols();
      return false;
   }

#ifdef HAVE_DYNAMIC_D3D
   d3d9_dylib_initialized = true;
#endif

   return true;
}

void d3d9_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d9_dll)
      dylib_close(g_d3d9_dll);
   g_d3d9_dll         = NULL;
   D3D9CreateEx       = NULL;

   d3d9_dylib_initialized = false;
#endif
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
#ifdef HAVE_DYNAMIC_D3D
   if (dev && d3d9_object_ex)
   {
      /* A full screen of its own is given its mode; a window none */
      d3d9_display_mode_ex_t mode;
      mode.Size             = sizeof(mode);
      mode.Width            = d3dpp->BackBufferWidth;
      mode.Height           = d3dpp->BackBufferHeight;
      mode.RefreshRate      = d3dpp->FullScreen_RefreshRateInHz;
      mode.Format           = d3dpp->BackBufferFormat;
      mode.ScanLineOrdering = 1; /* D3DSCANLINEORDERING_PROGRESSIVE */
      if (SUCCEEDED(((d3d9_ex_vtbl_t*)d3d->lpVtbl)->CreateDeviceEx(d3d,
                  cur_mon_id, D3DDEVTYPE_HAL, focus_window,
                  behavior_flags | (d3dpp->SwapEffect == D3D9_SWAPEFFECT_FLIPEX
                     ? D3D9_CREATE_ENABLE_PRESENTSTATS : 0), d3dpp,
                  d3dpp->Windowed ? NULL : &mode,
                  (IDirect3DDevice9**)dev)))
         return true;
      /* The flip model is not a reason to go without a device: the
       * swap effect of before, then */
      if (d3dpp->SwapEffect == D3D9_SWAPEFFECT_FLIPEX)
      {
         RARCH_WARN("[D3D9] No device presenting in the flip model, "
               "presenting with the discard swap effect.\n");
         d3dpp->SwapEffect = D3DSWAPEFFECT_DISCARD;
         if (SUCCEEDED(((d3d9_ex_vtbl_t*)d3d->lpVtbl)->CreateDeviceEx(d3d,
                     cur_mon_id, D3DDEVTYPE_HAL, focus_window,
                     behavior_flags, d3dpp,
                     d3dpp->Windowed ? NULL : &mode,
                     (IDirect3DDevice9**)dev)))
            return true;
      }
      /* and the device of a system without Ex if that will not do */
   }
#endif
   return (dev &&
         SUCCEEDED(IDirect3D9_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice9**)dev)));
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   int retries;
   for (retries = 0; retries < 10; retries++)
   {
      if (d3d9_create_device_internal(dev,
               (D3DPRESENT_PARAMETERS*)d3dpp,
               d3d,
               focus_window,
               cur_mon_id,
               D3DCREATE_HARDWARE_VERTEXPROCESSING))
         goto success;
      if (d3d9_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         goto success;
      if (retries == 0)
         RARCH_WARN("[D3D9] Device creation failed, retrying...\n");
      retro_sleep(3000);
   }
   RARCH_ERR("[D3D9] Could not create device after %d retries.\n", retries);
   return false;

success:
   if (retries > 0)
      RARCH_LOG("[D3D9] Device created successfully after %d retries.\n",
            retries);
   RARCH_LOG("[D3D9] %s device.\n",
         d3d9_dev_is_ex(*(LPDIRECT3DDEVICE9*)dev)
         ? "Direct3D9Ex" : "Direct3D 9");
   return true;
}

bool d3d9_reset(void *data, void *d3dpp)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev)
   {
      const char *err = NULL;
      if (IDirect3DDevice9_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) == D3D_OK)
         return true;
#ifndef _XBOX
      RARCH_WARN("[D3D] Attempting to recover from dead state...\n");
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
      RARCH_WARN("[D3D] Recovering from dead state: (%s).\n", err);
#endif
   }
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

static void d3d9_get_video_size(d3d9_video_t *d3d,
      unsigned *dims)
{
   XVIDEO_MODE video_mode;

   XGetVideoMode(&video_mode);

   *dims = VIDEO_SCALE_PACK(video_mode.dwDisplayWidth,
         video_mode.dwDisplayHeight);

   if (video_mode.fIsHiDef)
      *dims = VIDEO_SCALE_PACK(1280, 720);
   else
      *dims = VIDEO_SCALE_PACK(640, 480);

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
      LPDIRECT3D9 d3d9, bool rgb32, bool windowed)
{
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (IDirect3D9_GetAdapterDisplayMode(d3d9, 0, &display_mode))
         return display_mode.Format;
   }
   return D3DFMT_X8R8G8B8;
}
#endif

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp)
{
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
#ifdef _XBOX
   settings_t *settings           = config_get_ptr();
   int gamma_enable               = settings->uints.video_gamma;
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

      /* Four is the largest interval the presentation parameter can
       * carry, so anything above it presents at four. */
      switch (video_swap_interval)
      {
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         default:
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;
#ifndef _XBOX
   /* A window of a Direct3D9Ex device presents through the flip model
    * when asked to: the compositor takes its buffers as they are,
    * instead of a copy of each. Only a window, and only Ex, has it. */
   if (windowed_enable && d3d9_object_ex)
   {
      settings_t *settings        = config_get_ptr();
      if (settings && settings->bools.video_d3d9ex_present)
         d3dpp->SwapEffect        = D3D9_SWAPEFFECT_FLIPEX;
   }
#endif

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
         d3d->d3d9, info->rgb32, windowed_enable);
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      /* Xbox: query the actual display size, publish it to video_st
       * and track it in d3d->vp.full_dims so subsequent read sites
       * can pull from the local field instead of locking video_st. */
      unsigned dims = 0;
      d3d9_get_video_size(d3d, &dims);
      video_driver_set_output_dims(dims);
      d3d->vp.full_dims       = dims;
      d3dpp->BackBufferWidth  = VIDEO_SCALE_W(dims);
      d3dpp->BackBufferHeight = VIDEO_SCALE_H(dims);
#else
      /* Non-Xbox: by the time make_d3dpp runs, d3d9_*_init_internal
       * has already published the size and written d3d->vp.full_dims;
       * read from there. */
      d3dpp->BackBufferWidth  = VIDEO_SCALE_W(d3d->vp.full_dims);
      d3dpp->BackBufferHeight = VIDEO_SCALE_H(d3d->vp.full_dims);
#endif
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   if (!d3d->widescreen_mode)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   d3dpp->MultiSampleQuality      = 0;
#endif
}

void d3d9_present_init(d3d9_video_t *d3d, bool vsync)
{
#ifndef _XBOX
   D3DPRESENT_PARAMETERS pp;
   const d3d9_dev_ex_vtbl_t *vt = NULL;
   IDirect3DSwapChain9 *chain   = NULL;
   settings_t *settings         = config_get_ptr();
   void *ex                     = NULL;
   bool flip                    = false;
   int opt_latency              = 0;
   UINT max_latency             = 1;
   UINT cur_latency             = 0;

   d3d->dev_ex                  = NULL;
   d3d->chain_ex                = NULL;
   d3d->present_wait            = 0;
   d3d->wait_for_vblank         = 0;
   d3d->present_shown           = false;

   if (     !settings
         || !settings->bools.video_d3d9ex_present
         || !d3d->dev
         || FAILED(IDirect3DDevice9_QueryInterface(d3d->dev,
               &d3d9_iid_device_ex, &ex))
         || !ex)
      return;
   /* The device holds its own interface for as long as it lives */
   ((IUnknown*)ex)->lpVtbl->Release((IUnknown*)ex);
   d3d->dev_ex = (LPDIRECT3DDEVICE9)ex;
   vt          = (const d3d9_dev_ex_vtbl_t*)d3d->dev_ex->lpVtbl;

   if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(d3d->dev, 0, &chain))
         && chain)
   {
      void *chain_ex = NULL;
      if (SUCCEEDED(IDirect3DSwapChain9_GetPresentParameters(chain, &pp)))
         flip = (pp.SwapEffect == D3D9_SWAPEFFECT_FLIPEX);
      /* Its present statistics, for d3d9_last_present_time; the
       * device holds its swap chain as it holds itself */
      if (SUCCEEDED(IDirect3DSwapChain9_QueryInterface(chain,
                  &d3d9_iid_swap_chain_ex, &chain_ex)) && chain_ex)
      {
         ((IUnknown*)chain_ex)->lpVtbl->Release((IUnknown*)chain_ex);
         d3d->chain_ex = (LPDIRECT3DSWAPCHAIN9)chain_ex;
      }
      IDirect3DSwapChain9_Release(chain);
   }
   RARCH_LOG("[D3D9] Presenting %s.\n", flip
         ? "through the flip model" : "with the discard swap effect");

   /* The frame latency, as Direct3D 11 takes it */
   if (!vsync || !settings->bools.video_waitable_swapchains)
      return;
   opt_latency = settings->ints.video_max_frame_latency;
   if (opt_latency < 1)
      d3d->wait_for_vblank = (!opt_latency) ? 1 : -1;
   else
      max_latency          = (UINT)opt_latency;
   /* Waiting before Present wants a Present that does not wait
    * itself, which only the flip model makes */
   if (d3d->wait_for_vblank < 0 && !flip)
      d3d->wait_for_vblank = 1;

   vt->SetMaximumFrameLatency(d3d->dev_ex, max_latency);
   vt->GetMaximumFrameLatency(d3d->dev_ex, &cur_latency);
   if (d3d->wait_for_vblank && FAILED(vt->WaitForVBlank(d3d->dev_ex, 0)))
   {
      RARCH_WARN("[D3D9] The device cannot wait for the display; "
            "frames do not wait beside Present.\n");
      d3d->wait_for_vblank = 0;
   }
   RARCH_LOG("[D3D9] Requesting %u maximum frame latency, using %u%s.\n",
         max_latency, cur_latency,
         (d3d->wait_for_vblank < 0) ? " with WaitForVBlank before Present"
         : (d3d->wait_for_vblank > 0) ? " with WaitForVBlank after Present"
         : "");
#else
   (void)vsync;
   d3d->dev_ex          = NULL;
   d3d->chain_ex        = NULL;
   d3d->present_wait    = 0;
   d3d->wait_for_vblank = 0;
   d3d->present_shown   = false;
#endif
}

retro_time_t d3d9_last_present_time(d3d9_video_t *d3d)
{
#ifndef _XBOX
   static LARGE_INTEGER freq;
   d3d9_present_stats_t stats;
   UINT last = 0;
   if (d3d)
   {
      /* Only while the statistics describe that present: one made at
       * once, on no vblank, leaves them at the last that was */
      if (d3d->chain_ex)
      {
         const d3d9_chain_ex_vtbl_t *vt =
            (const d3d9_chain_ex_vtbl_t*)d3d->chain_ex->lpVtbl;
         stats.PresentCount         = 0;
         stats.PresentRefreshCount  = 0;
         stats.SyncRefreshCount     = 0;
         stats.SyncQPCTime.QuadPart = 0;
         stats.SyncGPUTime.QuadPart = 0;
         if (     SUCCEEDED(vt->GetPresentStats(d3d->chain_ex, &stats))
               && SUCCEEDED(vt->GetLastPresentCount(d3d->chain_ex, &last))
               && stats.PresentCount == last
               && stats.SyncQPCTime.QuadPart
               && (freq.QuadPart || QueryPerformanceFrequency(&freq)))
            return (retro_time_t)(
                  stats.SyncQPCTime.QuadPart / freq.QuadPart * 1000000
                  + stats.SyncQPCTime.QuadPart % freq.QuadPart * 1000000
                  / freq.QuadPart);
      }
#ifndef __WINRT__
      return win32_dwm_last_vblank_time();
#endif
   }
#endif
   return 0;
}

/* --- GPU-native BCn compressed-texture upload (shared by d3d9cg/d3d9hlsl) --- */
/* Direct3D 9 samples DXT1/DXT3/DXT5 == BC1/BC2/BC3; nothing above BC3
 * exists in D3D9 (BC7 is a D3D11 format). */
static D3DFORMAT d3d9_bc_to_d3dfmt(enum texture_gpu_format fmt)
{
   switch (fmt)
   {
      case TEXTURE_GPU_FORMAT_BC1: return D3DFMT_DXT1;
      case TEXTURE_GPU_FORMAT_BC2: return D3DFMT_DXT3;
      case TEXTURE_GPU_FORMAT_BC3: return D3DFMT_DXT5;
      default:                     break;
   }
   return D3DFMT_UNKNOWN;
}

bool d3d9_supports_texture_format(void *data, enum texture_gpu_format fmt)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   D3DFORMAT      f  = d3d9_bc_to_d3dfmt(fmt);
   if (!d3d || !d3d->d3d9 || f == D3DFMT_UNKNOWN)
      return false;
   return SUCCEEDED(IDirect3D9_CheckDeviceFormat(d3d->d3d9,
         D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
         0, D3DRTYPE_TEXTURE, f));
}

uintptr_t d3d9_load_texture_compressed(void *data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type)
{
   d3d9_video_t      *d3d   = (d3d9_video_t*)data;
   LPDIRECT3DTEXTURE9 tex   = NULL;
   void              *_tbuf = NULL;
   D3DFORMAT          f;
   unsigned           i;
   unsigned           block_bytes;

   /* Regular texture loads on this driver marshal to the video thread;
    * the compressed path does not yet, so under threading decline here
    * and let the CPU-decode fallback go through the marshalled path. */
   if (threaded)
      return 0;
   (void)filter_type; /* sampler filtering is a render state in D3D9 */

   if (!d3d || !d3d->dev || !tc || tc->num_mips == 0)
      return 0;
   if ((f = d3d9_bc_to_d3dfmt(tc->format)) == D3DFMT_UNKNOWN)
      return 0;
   block_bytes = (tc->format == TEXTURE_GPU_FORMAT_BC1) ? 8 : 16;

   if (FAILED(d3d9_tex_create(d3d->dev,
               tc->mips[0].width, tc->mips[0].height, tc->num_mips,
               0, f, (LPDIRECT3DTEXTURE9*)&_tbuf)))
      return 0;
   tex = (LPDIRECT3DTEXTURE9)_tbuf;

   for (i = 0; i < tc->num_mips; i++)
   {
      D3DLOCKED_RECT lr;
      if (SUCCEEDED(d3d9_tex_lock(tex, i, &lr, NULL, 0)))
      {
         unsigned       blocks_w  = (tc->mips[i].width  + 3) >> 2;
         unsigned       blocks_h  = (tc->mips[i].height + 3) >> 2;
         unsigned       row_bytes = blocks_w * block_bytes;
         const uint8_t *src       = (const uint8_t*)tc->mips[i].data;
         uint8_t       *dst       = (uint8_t*)lr.pBits;
         unsigned       r;
         /* lr.Pitch is the byte size of one row of 4x4 blocks and may be
          * padded, so copy row by row rather than in one shot. */
         for (r = 0; r < blocks_h; r++)
            memcpy(dst + r * lr.Pitch, src + r * row_bytes, row_bytes);
         d3d9_tex_unlock(tex, i, true);
      }
   }

   return (uintptr_t)tex;
}
