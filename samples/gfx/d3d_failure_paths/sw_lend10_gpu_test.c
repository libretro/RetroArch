/* The D3D10 software framebuffer lend, its shipping functions on a real
 * device: a core renders frame after frame into the lent staging slots,
 * each pushed frame reaches chain.history[0] through the GPU copy and
 * nothing else, the slot the cached frame may point into is never lent
 * straight back, every push retires the cached frame, a window pushed
 * from inside a larger loan reaches history[0] at its own size, and the
 * slots go unmapped. */
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d10.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <libretro.h>
#include "gfx/video_defines.h"
#define Release(p) do { if (p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while (0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)
typedef ID3D10Device *D3D10Device;
typedef ID3D10Texture2D *D3D10Texture2D;
typedef ID3D10Resource *D3D10Resource;
typedef struct { ID3D10Texture2D *handle; D3D10_TEXTURE2D_DESC desc; } d3d10_texture_t;
typedef struct
{
   D3D10Device device;
   DXGI_FORMAT format;
   struct { d3d10_texture_t history[1]; } chain;
#include "sw_lend_type.inc"
} d3d10_video_t;
static unsigned retired;
static void video_driver_cached_frame_retire(void) { retired++; }
#include "sw_lend.inc"

#define W 320
#define H 240

static bool verify(d3d10_video_t *v, ID3D10Texture2D *readback, unsigned seed)
{
   D3D10_MAPPED_TEXTURE2D m;
   unsigned x, y;
   bool ok = true;
   ID3D10Device_CopyResource(v->device, (ID3D10Resource*)readback,
         (ID3D10Resource*)v->chain.history[0].handle);
   if (FAILED(ID3D10Texture2D_Map(readback, 0, D3D10_MAP_READ, 0, &m)))
      return false;
   for (y = 0; y < H && ok; y++)
      for (x = 0; x < W && ok; x++)
      {
         uint32_t want = (seed * 2654435761u + y * W + x) & 0x00ffffffu;
         uint32_t got  = ((const uint32_t*)((const uint8_t*)m.pData
                  + (size_t)y * m.RowPitch))[x] & 0x00ffffffu;
         ok = (got == want);
      }
   ID3D10Texture2D_Unmap(readback, 0);
   return ok;
}

int main(void)
{
   D3D10_TEXTURE2D_DESC desc;
   d3d10_video_t v;
   ID3D10Texture2D *readback = NULL;
   struct retro_framebuffer fb;
   unsigned frame, lends = 0, declined = 0, last = (unsigned)-1;
   int result = 1;

   memset(&v, 0, sizeof(v));
   if (     FAILED(D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0,
                  D3D10_SDK_VERSION, &v.device))
         && FAILED(D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0,
                  D3D10_SDK_VERSION, &v.device)))
   {
      puts(getenv("REQUIRE_D3D10") ? "FAIL: no D3D10 device"
            : "SKIP: no D3D10 device");
      return getenv("REQUIRE_D3D10") ? 1 : 0;
   }
   v.format = DXGI_FORMAT_B8G8R8X8_UNORM;

   memset(&desc, 0, sizeof(desc));
   desc.Width = W; desc.Height = H; desc.MipLevels = 1; desc.ArraySize = 1;
   desc.Format = v.format; desc.SampleDesc.Count = 1;
   desc.Usage = D3D10_USAGE_DEFAULT; desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
   CHECK(SUCCEEDED(ID3D10Device_CreateTexture2D(v.device, &desc, NULL,
               &v.chain.history[0].handle)));
   v.chain.history[0].desc = desc;
   desc.Usage = D3D10_USAGE_STAGING; desc.BindFlags = 0;
   desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D10Device_CreateTexture2D(v.device, &desc, NULL,
               &readback)));

   for (frame = 0; frame < 120; frame++)
   {
      int slot;
      unsigned x, y, lx = 1, ly = 1;
      memset(&fb, 0, sizeof(fb));
      fb.width = W; fb.height = H;
      if (!d3d10_get_current_software_framebuffer(&v, &fb))
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
      slot = d3d10_sw_lend_find(&v, fb.data, W, H, (unsigned)fb.pitch,
            &lx, &ly);
      CHECK(slot >= 0 && !lx && !ly);
      /* the slot the cached frame may point into is not lent straight back */
      CHECK((unsigned)slot != last);
      last = (unsigned)slot;
      CHECK(d3d10_sw_lend_push(&v, slot, 0, 0, W, H));
      CHECK(!(v.sw_lend.mapped_bits & (1u << slot)));
      CHECK(retired == ++lends);
      if (!(frame % 10))
         CHECK(verify(&v, readback, frame));
   }
   /* Never waited for, so a busy copy may decline a frame now and then;
    * a lend that rarely happens is no lend */
   CHECK(lends >= 100);

   /* A frame pushed from anywhere else is not the lend's */
   {
      unsigned lx, ly;
      CHECK(d3d10_sw_lend_find(&v, readback, W, H, W * 4, &lx, &ly) < 0);
   }
   /* A loan larger than history[0], the frame pushed as a window inside
    * it, as a core cropping its overscan does: found at its offset and
    * copied into history[0] at the window's size */
   {
      unsigned tries, x, y, lx = 0, ly = 0;
      int slot = -1;
      bool got = false;
      for (tries = 0; tries < 50 && !got; tries++)
      {
         memset(&fb, 0, sizeof(fb));
         fb.width = W + 64; fb.height = H + 8;
         got = d3d10_get_current_software_framebuffer(&v, &fb);
      }
      CHECK(got && fb.pitch == (W + 64) * 4);
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
            ((uint32_t*)((uint8_t*)fb.data + (size_t)(y + 4) * fb.pitch))[x + 32] =
               (frame * 2654435761u + y * W + x) & 0x00ffffffu;
      slot = d3d10_sw_lend_find(&v, (uint8_t*)fb.data + 4 * fb.pitch + 32 * 4,
            W, H, (unsigned)fb.pitch, &lx, &ly);
      CHECK(slot >= 0 && lx == 32 && ly == 4);
      /* A window running past the loan is not the lend's */
      CHECK(d3d10_sw_lend_find(&v, (uint8_t*)fb.data + 4 * fb.pitch + 96 * 4,
            W, H, (unsigned)fb.pitch, &lx, &ly) < 0);
      CHECK(d3d10_sw_lend_push(&v, slot, lx, ly, W, H));
      CHECK(verify(&v, readback, frame));
   }
   memset(&fb, 0, sizeof(fb));
   fb.width = W; fb.height = H;

   /* A lent slot never pushed still goes unmapped, the cached frame
    * retired first */
   CHECK(d3d10_get_current_software_framebuffer(&v, &fb));
   CHECK(v.sw_lend.mapped_bits);
   frame = retired;
   d3d10_sw_lend_release(&v);
   CHECK(!v.sw_lend.mapped_bits && retired == frame + 1);
   for (frame = 0; frame < D3D10_SW_LEND_SLOTS; frame++)
      CHECK(!v.sw_lend.staging[frame]);

   printf("PASS: D3D10 software framebuffer lend, %u frames lent and copied "
         "on the GPU, %u declined while busy\n", lends, declined);
   result = 0;
done:
   d3d10_sw_lend_release(&v);
   Release(readback);
   Release(v.chain.history[0].handle);
   Release(v.device);
   return result;
}
#else
int main(void) { puts("SKIP: D3D10 GPU runtime is Windows-only"); return 0; }
#endif
