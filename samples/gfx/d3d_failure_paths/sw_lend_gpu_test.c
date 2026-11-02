/* The D3D11 software framebuffer lend, its shipping functions on a real
 * device: a core renders frame after frame into the lent staging slots,
 * each pushed frame reaches chain.history[0] through the GPU copy and
 * nothing else, the slot the cached frame may point into is never lent
 * straight back, every push retires the cached frame, a frame at a size
 * history[0] does not have is declined, and the slots go unmapped. */
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <libretro.h>
#include "gfx/video_defines.h"
#define Release(p) do { if (p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while (0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)
typedef ID3D11Device *D3D11Device;
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11Texture2D *D3D11Texture2D;
typedef ID3D11Resource *D3D11Resource;
enum { D3D11_ST_FLAG_HW_IFACE_ENABLE = 1 << 9 };
typedef struct { ID3D11Texture2D *handle; D3D11_TEXTURE2D_DESC desc; } d3d11_texture_t;
typedef struct
{
   D3D11Device device;
   D3D11DeviceContext context;
   DXGI_FORMAT format;
   unsigned flags;
   struct { d3d11_texture_t history[1]; } chain;
#include "sw_lend_type.inc"
} d3d11_video_t;
static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define W 320
#define H 240

static bool verify(d3d11_video_t *v, ID3D11Texture2D *readback, unsigned seed)
{
   D3D11_MAPPED_SUBRESOURCE m;
   unsigned x, y;
   bool ok = true;
   ID3D11DeviceContext_CopyResource(v->context, (ID3D11Resource*)readback,
         (ID3D11Resource*)v->chain.history[0].handle);
   if (FAILED(ID3D11DeviceContext_Map(v->context, (ID3D11Resource*)readback,
               0, D3D11_MAP_READ, 0, &m)))
      return false;
   for (y = 0; y < H && ok; y++)
      for (x = 0; x < W && ok; x++)
      {
         uint32_t want = (seed * 2654435761u + y * W + x) & 0x00ffffffu;
         uint32_t got  = ((const uint32_t*)((const uint8_t*)m.pData
                  + (size_t)y * m.RowPitch))[x] & 0x00ffffffu;
         ok = (got == want);
      }
   ID3D11DeviceContext_Unmap(v->context, (ID3D11Resource*)readback, 0);
   return ok;
}

int main(void)
{
   D3D_FEATURE_LEVEL level;
   D3D11_TEXTURE2D_DESC desc;
   d3d11_video_t v;
   ID3D11Texture2D *readback = NULL;
   struct retro_framebuffer fb;
   unsigned frame, lends = 0, declined = 0, last = (unsigned)-1;
   int result = 1;

   memset(&v, 0, sizeof(v));
   if (     FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                  NULL, 0, D3D11_SDK_VERSION, &v.device, &level, &v.context))
         && FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
                  NULL, 0, D3D11_SDK_VERSION, &v.device, &level, &v.context)))
   {
      puts(getenv("REQUIRE_D3D11") ? "FAIL: no D3D11 device"
            : "SKIP: no D3D11 device");
      return getenv("REQUIRE_D3D11") ? 1 : 0;
   }
   v.format = DXGI_FORMAT_B8G8R8X8_UNORM;

   memset(&desc, 0, sizeof(desc));
   desc.Width = W; desc.Height = H; desc.MipLevels = 1; desc.ArraySize = 1;
   desc.Format = v.format; desc.SampleDesc.Count = 1;
   desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(v.device, &desc, NULL,
               &v.chain.history[0].handle)));
   v.chain.history[0].desc = desc;
   desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
   desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(v.device, &desc, NULL,
               &readback)));

   for (frame = 0; frame < 120; frame++)
   {
      int slot;
      unsigned x, y;
      memset(&fb, 0, sizeof(fb));
      fb.width = W; fb.height = H;
      if (!d3d11_get_current_software_framebuffer(&v, &fb))
      {
         declined++;
         continue;
      }
      CHECK(fb.data && fb.pitch == W * 4);
      CHECK(fb.format == RETRO_PIXEL_FORMAT_XRGB8888);
      CHECK(fb.memory_flags == RETRO_MEMORY_TYPE_CACHED);
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
            ((uint32_t*)((uint8_t*)fb.data + (size_t)y * fb.pitch))[x] =
               (frame * 2654435761u + y * W + x) & 0x00ffffffu;
      slot = d3d11_sw_lend_find(&v, fb.data, W, H, (unsigned)fb.pitch);
      CHECK(slot >= 0);
      /* the slot the cached frame may point into is not lent straight back */
      CHECK((unsigned)slot != last);
      last = (unsigned)slot;
      CHECK(d3d11_sw_lend_push(&v, v.context, slot, W, H));
      CHECK(!(v.sw_lend.mapped_bits & (1u << slot)));
      CHECK(retired == ++lends);
      if (!(frame % 10))
         CHECK(verify(&v, readback, frame));
   }
   /* Never waited for, so a busy copy may decline a frame now and then;
    * a lend that rarely happens is no lend */
   CHECK(lends >= 100);

   /* A frame pushed from anywhere else is not the lend's */
   CHECK(d3d11_sw_lend_find(&v, readback, W, H, W * 4) < 0);
   /* A size history[0] does not have yet is declined */
   memset(&fb, 0, sizeof(fb));
   fb.width = W + 16; fb.height = H;
   CHECK(!d3d11_get_current_software_framebuffer(&v, &fb));
   /* So is a hardware-rendering core */
   v.flags = D3D11_ST_FLAG_HW_IFACE_ENABLE;
   fb.width = W;
   CHECK(!d3d11_get_current_software_framebuffer(&v, &fb));
   v.flags = 0;

   /* A lent slot never pushed still goes unmapped, the cached frame
    * retired first */
   CHECK(d3d11_get_current_software_framebuffer(&v, &fb));
   CHECK(v.sw_lend.mapped_bits);
   frame = retired;
   d3d11_sw_lend_release(&v);
   CHECK(!v.sw_lend.mapped_bits && retired == frame + 1);
   for (frame = 0; frame < D3D11_SW_LEND_SLOTS; frame++)
      CHECK(!v.sw_lend.staging[frame]);

   printf("PASS: D3D11 software framebuffer lend, %u frames lent and copied "
         "on the GPU, %u declined while busy\n", lends, declined);
   result = 0;
done:
   d3d11_sw_lend_release(&v);
   Release(readback);
   Release(v.chain.history[0].handle);
   Release(v.context);
   Release(v.device);
   return result;
}
#else
int main(void) { puts("SKIP: D3D11 GPU runtime is Windows-only"); return 0; }
#endif
