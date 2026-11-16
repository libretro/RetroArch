#ifndef FRAME_MEMORY_D3D10_TEST_H
#define FRAME_MEMORY_D3D10_TEST_H

#include <gfx/scaler/pixconv.h>
#define DXGI_FORMAT_EX_A4R4G4B4_UNORM 1000

typedef ID3D10Device *D3D10Device;
typedef ID3D10Texture2D *D3D10Texture2D;
typedef ID3D10Resource *D3D10Resource;
#define D3D10_FRAME_MEM_MAX 5
#define Release(resource) do { if (resource) (resource)->lpVtbl->Release(resource); } while (0)
typedef struct { D3D10Texture2D handle; D3D10_TEXTURE2D_DESC desc; ID3D10ShaderResourceView *view; } d3d10_texture_t;
typedef struct { unsigned history_size; } test_preset_t;
typedef struct
{
   D3D10Device device;
   DXGI_FORMAT format;
   unsigned flags;
   struct { test_preset_t *shader_preset; d3d10_texture_t history[1]; } chain;
   struct {
      D3D10Texture2D texture;
      D3D10_MAPPED_TEXTURE2D mapped;
      unsigned width, height;
      DXGI_FORMAT format;
      bool valid;
      retro_atomic_int_t cache_exposed;
   } frame_mem[D3D10_FRAME_MEM_MAX];
   struct {
      D3D10Texture2D staging[3];
      D3D10_MAPPED_TEXTURE2D mapped[3];
      unsigned mapped_bits, dims;
      DXGI_FORMAT format;
   } sw_lend;
} d3d10_video_t;
#include "d3d.inc"
static void device_memory(void)
{
   d3d10_video_t d3d;
   D3D10_TEXTURE2D_DESC desc;
   D3D10Texture2D readback;
   D3D10_MAPPED_TEXTURE2D read;
   test_preset_t preset;
   HRESULT hr;
   unsigned pass, i, x, y;
   memset(&d3d, 0, sizeof(d3d));
   hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0,
         D3D10_SDK_VERSION, &d3d.device);
   assert(SUCCEEDED(hr));
   for (pass = 0; pass < 2; pass++)
   {
      uintptr_t ids[D3D10_FRAME_MEM_MAX];
      video_frame_memory_t map;
      unsigned bpp = 4, align;
      bool cached;
      void *ptr;
      int hazard;
      preset.history_size = pass ? 0 : 1; d3d.chain.shader_preset = &preset;
      d3d.format = DXGI_FORMAT_B5G6R5_UNORM;
      assert(!d3d10_frame_mem_alloc(&d3d, 512 * 512 * 2, &ids[0], &cached, &align));
      d3d.format = DXGI_FORMAT_B8G8R8X8_UNORM;
      d3d.chain.shader_preset = NULL;
      assert(!d3d10_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[0], &cached, &align));
      for (i = 0; i < D3D10_FRAME_MEM_MAX; i++) assert(!d3d.frame_mem[i].texture);
      preset.history_size = pass ? 0 : 1; d3d.chain.shader_preset = &preset;
      d3d.chain.history[0].desc.Width = pass ? 321 : 320;
      d3d.chain.history[0].desc.Format = d3d.format;
      for (i = 0; i < D3D10_FRAME_MEM_MAX; i++)
      {
         ptr = d3d10_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[i], &cached, &align);
         assert(ptr && cached && align >= 320 * bpp);
      }
      assert(!d3d10_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[0], &cached, &align));
      assert(d3d10_frame_mem_map(&d3d, ids[0], &map));
      for (y = 0; y < map.height; y++)
         for (x = 0; x < map.width; x++)
            ((uint32_t*)((uint8_t*)map.data + y * map.pitch))[x]
               = 0x35000000u | (y * 513 + x);
      memset(&desc, 0, sizeof(desc));
      desc.Width = 256; desc.Height = 224;
      desc.MipLevels = 2; desc.ArraySize = desc.SampleDesc.Count = 1;
      desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
      desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
      desc.Format = d3d.frame_mem[ids[0] - 1].format; desc.Usage = D3D10_USAGE_DEFAULT;
      d3d.chain.history[0].desc = desc;
      assert(SUCCEEDED(d3d.device->lpVtbl->CreateTexture2D(d3d.device, &desc,
               NULL, &d3d.chain.history[0].handle)));
      assert(SUCCEEDED(d3d.device->lpVtbl->CreateShaderResourceView(d3d.device,
               (D3D10Resource)d3d.chain.history[0].handle, NULL, &d3d.chain.history[0].view)));
      {
         ID3D10RenderTargetView *mip;
         D3D10_RENDER_TARGET_VIEW_DESC target;
         float clear[4] = {0, 0, 0, 0};
         memset(&target, 0, sizeof(target));
         target.Format = desc.Format;
         target.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
         target.Texture2D.MipSlice = 1;
         assert(SUCCEEDED(d3d.device->lpVtbl->CreateRenderTargetView(d3d.device,
                  (D3D10Resource)d3d.chain.history[0].handle, &target, &mip)));
         d3d.device->lpVtbl->ClearRenderTargetView(d3d.device, mip, clear);
         Release(mip);
      }
      ptr = (uint8_t*)map.data + 2 * map.pitch + 3 * bpp;
      retro_atomic_store_release_int(map.cache_exposed, 1);
      frame_cache_store(ptr, VIDEO_SCALE_PACK(256, 224), map.pitch);
      hazard = frame_cache_hazard_acquire(ptr);
      assert(hazard >= 0);
      assert(!d3d10_frame_mem_push(&d3d, ptr, 256, 224, (unsigned)map.pitch));
      assert(d3d.frame_mem[ids[0] - 1].valid);
      frame_cache_hazard_release(hazard);
      assert(d3d10_frame_mem_push(&d3d, ptr, 256, 224, (unsigned)map.pitch));
      assert(!d3d.frame_mem[ids[0] - 1].valid);
      assert(!retro_atomic_load_acquire_int(map.cache_exposed));
      desc.Usage = D3D10_USAGE_STAGING; desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
      desc.BindFlags = desc.MiscFlags = 0;
      assert(SUCCEEDED(d3d.device->lpVtbl->CreateTexture2D(d3d.device, &desc, NULL, &readback)));
      d3d.device->lpVtbl->CopyResource(d3d.device, (ID3D10Resource*)readback,
            (ID3D10Resource*)d3d.chain.history[0].handle);
      assert(SUCCEEDED(readback->lpVtbl->Map(readback, 0, D3D10_MAP_READ, 0, &read)));
      for (y = 0; y < 224; y++)
         for (x = 0; x < 256; x++)
         {
            uint32_t expected = (y + 2) * 513 + x + 3;
            uint32_t actual = ((uint32_t*)((uint8_t*)read.pData + y * read.RowPitch))[x];
            if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
            {
               expected = ((expected << 16) & 0xff0000)
                  | (expected & 0xff00) | ((expected >> 16) & 0xff);
               assert((actual >> 24) == 0xff);
            }
            assert((actual & 0xffffff) == expected);
         }
      readback->lpVtbl->Unmap(readback, 0);
      assert(SUCCEEDED(readback->lpVtbl->Map(readback, 1, D3D10_MAP_READ, 0, &read)));
      assert((*(const uint32_t*)read.pData & 0xffffff) != 0);
      readback->lpVtbl->Unmap(readback, 1);
      Release(readback);
      assert(d3d10_frame_mem_ready(&d3d, ids[0]));
      assert(d3d10_frame_mem_map(&d3d, ids[0], &map));
      assert(map.data && map.pitch >= 320 * bpp);
      d3d.sw_lend.staging[0] = d3d.frame_mem[ids[0] - 1].texture;
      d3d.sw_lend.mapped[0] = d3d.frame_mem[ids[0] - 1].mapped;
      d3d.sw_lend.mapped_bits = 1;
      d3d.sw_lend.dims = VIDEO_SCALE_PACK(map.width, map.height);
      /* The legacy core-loan path requires matching texture formats. */
      d3d.sw_lend.format = desc.Format;
      frame_cache_store(map.data, VIDEO_SCALE_PACK(256, 224), map.pitch);
      hazard = frame_cache_hazard_acquire(map.data);
      assert(hazard >= 0);
      assert(!d3d10_sw_lend_push(&d3d, 0, 0, 0, 256, 224));
      assert(d3d.sw_lend.mapped_bits == 1);
      frame_cache_hazard_release(hazard);
      assert(d3d10_sw_lend_push(&d3d, 0, 0, 0, 256, 224));
      d3d.frame_mem[ids[0] - 1].valid = false;
      d3d.device->lpVtbl->Flush(d3d.device);
      for (i = 0; i < 100000 && !d3d10_frame_mem_ready(&d3d, ids[0]); i++)
         Sleep(0);
      assert(i < 100000);
      assert(d3d10_frame_mem_map(&d3d, ids[0], &map));
      /* A switch back to stock must leave its direct renderer in charge. */
      d3d.chain.shader_preset = NULL;
      assert(!d3d10_frame_mem_push(&d3d, map.data, 256, 224, (unsigned)map.pitch));
      assert(d3d.frame_mem[ids[0] - 1].valid);
      for (i = 0; i < D3D10_FRAME_MEM_MAX; i++) d3d10_frame_mem_free(&d3d, ids[i]);
      Release(d3d.chain.history[0].view);
      Release(d3d.chain.history[0].handle);
   }
   Release(d3d.device);
   puts("PASS: native D3D10 XRGB8888 conversion, cropped GPU copies, remapping and 16-bit/stock exclusion");
}

#endif
