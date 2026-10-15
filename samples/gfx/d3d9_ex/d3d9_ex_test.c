/* The D3D9 drivers' textures and frame memory on a real device of each
 * kind: one Direct3D9Ex makes (no managed pool; textures with a
 * system-memory twin, frames read by the GPU where they lie) and one
 * plain Direct3D 9 makes (managed textures, as before Ex). What reached
 * the GPU is read back and checked byte for byte. Then Direct3D 9Ex
 * Presentation: a device in the flip model, its frame latency set
 * through the Ex table d3d9_tex.h lays out, and d3d9_present in each
 * of its ways. Needs a display: run it under Wine on Xvfb, or on
 * Windows. */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/common/d3d9_common.h"
#include "../../../gfx/common/d3d9_common.c"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

static int fails;
static unsigned retires;

void video_driver_cached_frame_retire(void)     { retires++; }

/* d3d9_present times itself on this clock */
retro_time_t cpu_features_get_time_usec(void)
{
   LARGE_INTEGER c, f;
   QueryPerformanceCounter(&c);
   QueryPerformanceFrequency(&f);
   return (retro_time_t)(c.QuadPart / f.QuadPart * 1000000
         + c.QuadPart % f.QuadPart * 1000000 / f.QuadPart);
}
void video_driver_cached_frame_invalidate(void) { }

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

#define TEX_W 256
#define TEX_H 128

typedef HRESULT (WINAPI *create_ex_t)(UINT, IDirect3D9**);
typedef IDirect3D9 *(WINAPI *create_t)(UINT);

static LPDIRECT3DDEVICE9 make_device(HWND w, bool ex, D3DSWAPEFFECT swap)
{
   HMODULE           dll = LoadLibraryA("d3d9.dll");
   IDirect3D9       *d3d = NULL;
   LPDIRECT3DDEVICE9 dev = NULL;
   D3DPRESENT_PARAMETERS pp;
   D3DDEVICE_CREATION_PARAMETERS cp;

   if (!dll)
      return NULL;
   memset(&pp, 0, sizeof(pp));
   pp.Windowed         = TRUE;
   pp.SwapEffect       = swap;
   pp.BackBufferCount  = 2;
   pp.hDeviceWindow    = w;
   pp.BackBufferFormat = D3DFMT_X8R8G8B8;
   if (ex)
   {
      create_ex_t cex = (create_ex_t)(void*)GetProcAddress(dll,
            "Direct3DCreate9Ex");
      if (!cex || FAILED(cex(D3D_SDK_VERSION, &d3d)) || !d3d)
         return NULL;
   }
   else
   {
      create_t c = (create_t)(void*)GetProcAddress(dll, "Direct3DCreate9");
      if (!c || !(d3d = c(D3D_SDK_VERSION)))
         return NULL;
   }
   d3d9_object_ex = ex;
   if (!d3d9_create_device_internal(&dev, &pp, d3d, w, 0,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      dev = NULL;
   IDirect3D9_Release(d3d);
   if (dev)
      CHECK(SUCCEEDED(IDirect3DDevice9_GetCreationParameters(dev, &cp))
            && !!(cp.BehaviorFlags & D3D9_CREATE_ENABLE_PRESENTSTATS)
               == (ex && pp.SwapEffect == D3D9_SWAPEFFECT_FLIPEX),
            "only flip-model Ex devices enable presentation statistics");
   return dev;
}

/* What the GPU holds in @tex's top level, into @out (TEX_W x TEX_H) */
static bool read_back(LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 tex,
      uint32_t *out)
{
   D3DLOCKED_RECT lr;
   unsigned y;
   if (!d3d9_tex_twin(tex))
   {
      /* A managed texture: the runtime's copy is what it uploads */
      if (FAILED(IDirect3DTexture9_LockRect(tex, 0, &lr, NULL,
                  D3DLOCK_READONLY)))
         return false;
      for (y = 0; y < TEX_H; y++)
         memcpy(out + y * TEX_W, (uint8_t*)lr.pBits + y * lr.Pitch,
               TEX_W * 4);
      IDirect3DTexture9_UnlockRect(tex, 0);
      return true;
   }
   {
      LPDIRECT3DTEXTURE9 rt = NULL, sys = NULL;
      LPDIRECT3DSURFACE9 s_tex = NULL, s_rt = NULL, s_sys = NULL;
      bool ok = false;
      if (     SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, TEX_W, TEX_H, 1,
                  D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
                  &rt, NULL))
            && SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, TEX_W, TEX_H, 1,
                  0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL)))
      {
         IDirect3DTexture9_GetSurfaceLevel(tex, 0, &s_tex);
         IDirect3DTexture9_GetSurfaceLevel(rt, 0, &s_rt);
         IDirect3DTexture9_GetSurfaceLevel(sys, 0, &s_sys);
         if (     SUCCEEDED(IDirect3DDevice9_StretchRect(dev, s_tex, NULL,
                     s_rt, NULL, D3DTEXF_NONE))
               && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, s_rt,
                     s_sys))
               && SUCCEEDED(IDirect3DTexture9_LockRect(sys, 0, &lr, NULL,
                     D3DLOCK_READONLY)))
         {
            for (y = 0; y < TEX_H; y++)
               memcpy(out + y * TEX_W, (uint8_t*)lr.pBits + y * lr.Pitch,
                     TEX_W * 4);
            IDirect3DTexture9_UnlockRect(sys, 0);
            ok = true;
         }
         IDirect3DSurface9_Release(s_tex);
         IDirect3DSurface9_Release(s_rt);
         IDirect3DSurface9_Release(s_sys);
      }
      if (rt)
         IDirect3DTexture9_Release(rt);
      if (sys)
         IDirect3DTexture9_Release(sys);
      return ok;
   }
}

static uint32_t px(unsigned n, unsigned x, unsigned y)
{
   return (n * 0x010203u + x * 0x0101u + y * 0x010000u) & 0xffffff;
}

static void write_frame(uint8_t *base, size_t pitch, unsigned n,
      unsigned w, unsigned h)
{
   unsigned x, y;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         ((uint32_t*)(base + y * pitch))[x] = 0xff000000u | px(n, x, y);
}

/* The GPU's copy holds frame @n's @w x @h window from (@ox, @oy) at its
 * corner; with @zeros, nothing past it */
static bool holds(LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 tex,
      unsigned n, unsigned ox, unsigned oy, unsigned w, unsigned h,
      bool zeros)
{
   static uint32_t buf[TEX_W * TEX_H];
   unsigned x, y;
   if (!read_back(dev, tex, buf))
      return false;
   for (y = 0; y < TEX_H; y++)
      for (x = 0; x < TEX_W; x++)
      {
         uint32_t v = buf[y * TEX_W + x] & 0xffffff;
         if (x < w && y < h)
         {
            if (v != px(n, ox + x, oy + y))
               return false;
         }
         else if (zeros && v)
            return false;
      }
   return true;
}

static void run(HWND w, bool ex)
{
   static uint8_t own[TEX_W * 4 * TEX_H];
   LPDIRECT3DDEVICE9 dev = make_device(w, ex, D3DSWAPEFFECT_DISCARD);
   LPDIRECT3DTEXTURE9 tex = NULL, mip = NULL;
   LPDIRECT3DVERTEXBUFFER9 vb = NULL;
   d3d9_sw_fb_t fb;
   unsigned dims = VIDEO_SCALE_PACK(TEX_W, TEX_H);

   printf("%s device:\n", ex ? "Direct3D9Ex" : "Direct3D 9");
   if (!dev)
   {
      printf("  [FAIL] the device is made\n");
      fails++;
      return;
   }
   memset(&fb, 0, sizeof(fb));
   CHECK(d3d9_dev_is_ex(dev) == ex, "it is told for what it is");

   CHECK(SUCCEEDED(d3d9_tex_create(dev, TEX_W, TEX_H, 1, 0,
               D3DFMT_X8R8G8B8, &tex)) && tex,
         "a texture the CPU writes is made");
   CHECK(!d3d9_tex_twin(tex) == !ex,
         ex ? "in the default pool, a system-memory twin with it"
            : "in the managed pool, no twin");

   write_frame(own, TEX_W * 4, 1, 200, 100);
   d3d9_frame_upload(tex, dims, 0, own, TEX_W * 4, 200, 100, 4);
   CHECK(holds(dev, tex, 1, 0, 0, 200, 100, true),
         "a frame at a new size reaches the GPU, past it cleared");
   write_frame(own, TEX_W * 4, 2, 200, 100);
   d3d9_frame_upload(tex, dims, VIDEO_SCALE_PACK(200, 100), own,
         TEX_W * 4, 200, 100, 4);
   CHECK(holds(dev, tex, 2, 0, 0, 200, 100, true),
         "and the next at the same size, its rectangle alone sent");

   {
      struct retro_framebuffer out;
      memset(&out, 0, sizeof(out));
      out.width  = 160;
      out.height = 90;
      CHECK(d3d9_sw_fb_lend(&fb, tex, dims, 4, &out) && out.data,
            "the texture is lent");
      if (out.data)
      {
         write_frame((uint8_t*)out.data, out.pitch, 3, 160, 90);
         d3d9_sw_fb_settle(&fb, out.data, 160, 90, (unsigned)out.pitch, 4,
               VIDEO_SCALE_PACK(200, 100));
         CHECK(holds(dev, tex, 3, 0, 0, 160, 90, true),
               "a frame written into the loan reaches the GPU");
      }
   }

   CHECK(SUCCEEDED(d3d9_tex_create(dev, 64, 64, 0, D3DUSAGE_AUTOGENMIPMAP,
               D3DFMT_A8R8G8B8, &mip)) && mip,
         "a texture making its own mipmaps is made");
   if (mip)
   {
      D3DLOCKED_RECT lr;
      CHECK(SUCCEEDED(d3d9_tex_lock(mip, 0, &lr, NULL, 0)),
            "and its top level written");
      d3d9_tex_unlock(mip, 0, true);
      IDirect3DTexture9_Release(mip);
   }

   CHECK(SUCCEEDED(d3d9_vbuf_create(dev, 4 * sizeof(Vertex),
               D3DUSAGE_WRITEONLY, &vb)) && vb
         && (d3d9_vbuf_rewrite_flags(vb) == (ex ? D3DLOCK_DISCARD : 0)),
         ex ? "an overlay's quad buffer is dynamic, rewritten with DISCARD"
            : "an overlay's quad buffer is managed");
   if (vb)
      IDirect3DVertexBuffer9_Release(vb);

   {
      uintptr_t id = 0;
      bool cached  = false;
      unsigned align = 0;
      uint8_t *mem = (uint8_t*)d3d9_frame_mem_alloc(&fb, dev,
            TEX_W * 4 * TEX_H, &id, &cached, &align);
      if (!ex)
         CHECK(!mem, "frame memory is not offered");
      else
      {
         unsigned polls = 0;
         CHECK(mem && id && cached && align,
               "frame memory is offered, at a pitch it reads in place");
         if (mem)
         {
            write_frame(mem, TEX_W * 4, 4, TEX_W, TEX_H);
            CHECK(d3d9_frame_mem_upload(&fb, dev, tex, dims,
                     VIDEO_SCALE_PACK(160, 90), mem + 8 * TEX_W * 4 + 16 * 4,
                     TEX_W * 4, 120, 64, 4),
                  "a window inside it goes up without a copy");
            CHECK(holds(dev, tex, 4, 16, 8, 120, 64, true),
                  "and reaches the GPU as it lies, past it cleared");
            while (!d3d9_frame_mem_ready(&fb, id) && polls < 10000000)
               polls++;
            CHECK(d3d9_frame_mem_ready(&fb, id),
                  "the block is the core's again once the GPU is done");
            write_frame(mem, TEX_W * 4, 5, TEX_W, TEX_H);
            d3d9_frame_put(&fb, dev, tex, dims, VIDEO_SCALE_PACK(120, 64),
                  mem + 8 * TEX_W * 4 + 16 * 4, TEX_W * 4, 120, 64, 4);
            CHECK(holds(dev, tex, 5, 16, 8, 120, 64, true),
                  "the next frame from the same place goes the same way");
            CHECK(!d3d9_frame_mem_upload(&fb, dev, tex, dims,
                     VIDEO_SCALE_PACK(120, 64), own, TEX_W * 4, 120, 64, 4),
                  "a frame from elsewhere is left to the copy");
            d3d9_frame_mem_free(&fb, id);
            d3d9_frame_mem_collect(&fb);
            {
               unsigned i;
               for (i = 0; i < D3D9_FRAME_MEM_MAX; i++)
                  d3d9_frame_mem_blk_release(&fb.mem[i], true);
            }
         }
      }
   }

   IDirect3DTexture9_Release(tex);
   d3d9_sw_fb_collect(&fb);
   IDirect3DDevice9_Release(dev);
}

/* The Ex tables and statistics d3d9_tex.h lays out by hand, against
 * the SDK's own where it has them */
static void layout_run(void)
{
   printf("Ex layouts against the SDK's:\n");
#ifndef D3D_DISABLE_9EX
   CHECK(D3D9_CREATE_ENABLE_PRESENTSTATS == D3DCREATE_ENABLE_PRESENTSTATS,
         "the presentation statistics creation flag");
   CHECK(     offsetof(d3d9_dev_ex_vtbl_t, PresentEx)
           == offsetof(IDirect3DDevice9ExVtbl, PresentEx)
         &&   offsetof(d3d9_dev_ex_vtbl_t, WaitForVBlank)
           == offsetof(IDirect3DDevice9ExVtbl, WaitForVBlank)
         &&   offsetof(d3d9_dev_ex_vtbl_t, SetMaximumFrameLatency)
           == offsetof(IDirect3DDevice9ExVtbl, SetMaximumFrameLatency)
         &&   offsetof(d3d9_dev_ex_vtbl_t, GetMaximumFrameLatency)
           == offsetof(IDirect3DDevice9ExVtbl, GetMaximumFrameLatency),
         "the device's Ex table");
   CHECK(     offsetof(d3d9_ex_vtbl_t, CreateDeviceEx)
           == offsetof(IDirect3D9ExVtbl, CreateDeviceEx),
         "the object's Ex table");
   CHECK(     offsetof(d3d9_chain_ex_vtbl_t, GetLastPresentCount)
           == offsetof(IDirect3DSwapChain9ExVtbl, GetLastPresentCount)
         &&   offsetof(d3d9_chain_ex_vtbl_t, GetPresentStats)
           == offsetof(IDirect3DSwapChain9ExVtbl, GetPresentStats),
         "the swap chain's Ex table");
#endif
   /* As Microsoft's d3d9types.h lays them out (gfx/include/d3d9 has
    * it): packed to 4 bytes on 32-bit x86, which mingw-w64's copy
    * leaves out, natural elsewhere */
#if defined(_X86_) || defined(_M_IX86) || defined(__i386__)
   CHECK(     sizeof(d3d9_present_stats_t) == 28
         &&   offsetof(d3d9_present_stats_t, SyncQPCTime) == 12
         &&   offsetof(d3d9_present_stats_t, SyncGPUTime) == 20,
         "the present statistics");
#else
   CHECK(     sizeof(d3d9_present_stats_t) == 32
         &&   offsetof(d3d9_present_stats_t, SyncQPCTime) == 16
         &&   offsetof(d3d9_present_stats_t, SyncGPUTime) == 24,
         "the present statistics");
#endif
}

/* Direct3D 9Ex Presentation (d3d9_present_init, d3d9_present): a
 * window in the flip model, the frame latency through the device's Ex
 * table, and a frame presented in each way it can wait for the display */
static void present_run(HWND w)
{
   static const int waits[3] = { 0, 1, -1 };
   static const char *names[3] = {
      "a frame is presented without waiting for the display",
      "a frame is presented, then waits for the display",
      "a frame waits for the display, then is presented at once" };
   d3d9_video_t d3d;
   D3DPRESENT_PARAMETERS pp;
   LPDIRECT3DDEVICE9 dev      = make_device(w, true, D3D9_SWAPEFFECT_FLIPEX);
   IDirect3DSwapChain9 *chain = NULL;
   const d3d9_dev_ex_vtbl_t *vt;
   void *ex                   = NULL;
   void *chain_ex             = NULL;
   UINT lat                   = 0;
   unsigned i;

   printf("Direct3D9Ex device in the flip model:\n");
   if (!dev)
   {
      printf("  [FAIL] the device is made\n");
      fails++;
      return;
   }
   memset(&pp, 0, sizeof(pp));
   CHECK(SUCCEEDED(IDirect3DDevice9_GetSwapChain(dev, 0, &chain)) && chain
         && SUCCEEDED(IDirect3DSwapChain9_GetPresentParameters(chain, &pp))
         && pp.SwapEffect == D3D9_SWAPEFFECT_FLIPEX,
         "its swap chain says it presents through the flip model");
   if (chain)
      IDirect3DSwapChain9_Release(chain);
   if (FAILED(IDirect3DDevice9_QueryInterface(dev, &d3d9_iid_device_ex,
               &ex)) || !ex)
   {
      printf("  [FAIL] it has its Ex interface\n");
      fails++;
      IDirect3DDevice9_Release(dev);
      return;
   }
   ((IUnknown*)ex)->lpVtbl->Release((IUnknown*)ex);
   vt = (const d3d9_dev_ex_vtbl_t*)((LPDIRECT3DDEVICE9)ex)->lpVtbl;
   CHECK(SUCCEEDED(vt->SetMaximumFrameLatency((LPDIRECT3DDEVICE9)ex, 3))
         && SUCCEEDED(vt->GetMaximumFrameLatency((LPDIRECT3DDEVICE9)ex,
               &lat)) && lat == 3,
         "a frame latency of 3 is set and read back through its table");
   CHECK(SUCCEEDED(vt->SetMaximumFrameLatency((LPDIRECT3DDEVICE9)ex, 1))
         && SUCCEEDED(vt->GetMaximumFrameLatency((LPDIRECT3DDEVICE9)ex,
               &lat)) && lat == 1,
         "and one of 1");

   memset(&d3d, 0, sizeof(d3d));
   d3d.dev              = dev;
   d3d.dev_ex           = (LPDIRECT3DDEVICE9)ex;
   d3d.video_info.vsync = true;
   for (i = 0; i < 3; i++)
   {
      d3d.wait_for_vblank = waits[i];
      d3d.present_wait    = -1;
      IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET,
            0xff000000 | (i * 0x40), 1.0f, 0);
      CHECK(SUCCEEDED(d3d9_present(&d3d)), names[i]);
      CHECK(d3d.present_wait >= 0
            && d3d.present_shown == (waits[i] > 0),
            "  and is timed, out as it returns only after the wait");
   }
   d3d.wait_for_vblank  = 0;
   d3d.video_info.vsync = false;
   d3d9_present(&d3d);
   CHECK(d3d.present_wait == 0 && !d3d.present_shown,
         "without V-Sync a present waits for nothing");

   /* The present statistics, through the swap chain's Ex table */
   chain = NULL;
   if (     SUCCEEDED(IDirect3DDevice9_GetSwapChain(dev, 0, &chain))
         && chain
         && SUCCEEDED(IDirect3DSwapChain9_QueryInterface(chain,
               &d3d9_iid_swap_chain_ex, &chain_ex))
         && chain_ex)
   {
      const d3d9_chain_ex_vtbl_t *cvt = (const d3d9_chain_ex_vtbl_t*)
            ((LPDIRECT3DSWAPCHAIN9)chain_ex)->lpVtbl;
      d3d9_present_stats_t stats;
      UINT count = 0;
      HRESULT shr;
      /* What they hold is the runtime's to fill (Wine leaves them
       * zero, and d3d9_last_present_time goes to the compositor) */
      CHECK(SUCCEEDED(cvt->GetLastPresentCount(
                  (LPDIRECT3DSWAPCHAIN9)chain_ex, &count)),
            "its swap chain's present count is read through its table");
      memset(&stats, 0, sizeof(stats));
      shr = cvt->GetPresentStats((LPDIRECT3DSWAPCHAIN9)chain_ex, &stats);
      printf("  (present statistics: %08lx, count %u of %u, "
            "qpc %08lx%08lx)\n", (unsigned long)shr, stats.PresentCount,
            count, (unsigned long)stats.SyncQPCTime.HighPart,
            (unsigned long)stats.SyncQPCTime.LowPart);
      ((IUnknown*)chain_ex)->lpVtbl->Release((IUnknown*)chain_ex);
   }
   else
   {
      printf("  [FAIL] its swap chain has its Ex interface\n");
      fails++;
   }
   if (chain)
      IDirect3DSwapChain9_Release(chain);
   IDirect3DDevice9_Release(dev);
}

int main(void)
{
   HWND w = CreateWindowA("STATIC", "d3d9_ex_test", WS_OVERLAPPEDWINDOW,
         0, 0, 64, 64, NULL, NULL, NULL, NULL);
   if (!w)
   {
      printf("[FAIL] no window\n");
      return 1;
   }
   run(w, false);
   run(w, true);
   present_run(w);
   layout_run();
   DestroyWindow(w);
   return fails ? 1 : 0;
}
