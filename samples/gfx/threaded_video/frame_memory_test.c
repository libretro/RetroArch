/* Shipping mapping publication and cache retirement, with native D3D11 readback. */
#define CINTERFACE
#define COBJMACROS
#ifdef _WIN32
#include <windows.h>
#ifdef TEST_D3D10
#include <d3d10.h>
#else
#include <d3d11.h>
#endif
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memalign.h>
#include <rthreads/rthreads.h>
#include "gfx/video_driver.h"
#include "gfx/video_thread_wrapper.h"

#define FRAME_CACHE_HAZARDS 1
#define FRAME_CACHE_HAZARD_SLOTS 8
#define FRAME_CACHE_SEQ_TRIES 64
static video_driver_state_t video_driver_st;
static retro_atomic_ptr_t frame_cache_hazard[FRAME_CACHE_HAZARD_SLOTS];
static retro_atomic_int_t frame_cache_generation, frame_cache_dims;
static retro_atomic_ptr_t frame_cache_data;
static retro_atomic_size_t frame_cache_seq, frame_cache_pitch;
#include "cache.inc"

static INLINE int video_thread_ring_load(thread_video_t *thr)
{
   return retro_atomic_load_acquire_int(&thr->frame.state);
}
static bool on_video_thread;
static bool video_thread_is_self(thread_video_t *thr) { return on_video_thread; }
#include "memory.inc"

static struct
{
   video_frame_memory_t map[VIDEO_THREAD_FRAME_MEM_DRIVER];
   unsigned allocated, calls, offset;
   retro_atomic_int_t exposed[VIDEO_THREAD_FRAME_MEM_DRIVER];
   bool enabled, phase;
} backend;
static unsigned char *storage[2 * VIDEO_THREAD_FRAME_MEM_DRIVER + 2];
static void *mock_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *align)
{
   unsigned i = backend.allocated;
   assert(backend.phase && data && size == 512);
   backend.calls++;
   if (!backend.enabled || i == VIDEO_THREAD_FRAME_MEM_DRIVER)
      return NULL;
   backend.allocated++;
   retro_atomic_int_init(&backend.exposed[i], 0);
   backend.map[i].cache_exposed = &backend.exposed[i];
   backend.map[i].data = storage[2 + i] + backend.offset;
   backend.map[i].size = 1024 - backend.offset;
   backend.map[i].pitch = 32;
   backend.map[i].width = 16;
   backend.map[i].height = (unsigned)(backend.map[i].size / 32);
   backend.map[i].format = RETRO_PIXEL_FORMAT_RGB565;
   *cached = true; *align = 32; *id = i + 1;
   return backend.map[i].data;
}
static bool mock_ready(void *data, uintptr_t id)
{
   assert(backend.phase && data && id && id <= backend.allocated);
   backend.calls++;
   return true;
}
static bool mock_map(void *data, uintptr_t id, video_frame_memory_t *map)
{
   assert(backend.phase && data && id && id <= backend.allocated);
   backend.calls++;
   *map = backend.map[id - 1];
   return true;
}
static void mock_free(void *data, uintptr_t id)
{
   assert(backend.phase && data && id && id <= backend.allocated);
   backend.calls++;
}
static const video_poke_interface_t *mock_poke(void)
{
   static video_poke_interface_t poke;
   poke.frame_mem_alloc = mock_alloc;
   poke.frame_mem_ready = mock_ready;
   poke.frame_mem_free = mock_free;
   poke.frame_mem_map = mock_map;
   return &poke;
}
static void init_wrapper(video_thread_private_t *priv)
{
   thread_video_t *thr = &priv->base;
   unsigned i;
   memset(priv, 0, sizeof(*priv));
   memset(&backend, 0, sizeof(backend));
   for (i = 0; i < VIDEO_THREAD_FRAME_MEM; i++)
   {
      retro_atomic_int_init(&thr->frame.mem[i].busy, 0);
      retro_atomic_int_init(&priv->mapping[i].ready, 0);
   }
   for (i = 0; i < VIDEO_THREAD_FRAME_MEM_DRIVER; i++)
      retro_atomic_int_init(&priv->mapping_pending[i].ready, 0);
   for (i = 0; i < 2; i++)
   {
      thr->frame.mem[i].ptr = storage[i];
      thr->frame.mem[i].size = 512;
      thr->frame.mem[i].cached = true;
      thr->frame.slot[i].mem = i;
      thr->frame.slot[i].buffer = storage[i];
   }
   thr->driver_data = &backend;
   thr->frame.mem_poke = mock_poke();
   thr->frame.buffer_size = 512;
   thr->frame.pixel_capacity = 256;
   thr->frame.spare_mem = thr->frame.lent = -1;
   retro_atomic_int_init(&thr->frame.state, VIDEO_THREAD_RING_MAKE(0, 0, false));
}
static void remap_worker(void *data)
{
   video_thread_private_t *priv = (video_thread_private_t*)data;
   unsigned i;
   for (i = 0; i < VIDEO_THREAD_FRAME_MEM; i++)
      if (priv->base.frame.mem_busy & (1u << i))
      {
         unsigned id = (unsigned)priv->base.frame.mem[i].id - 1;
         backend.map[id].data = storage[2 + VIDEO_THREAD_FRAME_MEM_DRIVER + id];
      }
   video_thread_mem_poll_mapped(&priv->base);
}
static void mappings(void)
{
   static video_thread_private_t priv;
   thread_video_t *thr = &priv.base;
   struct retro_framebuffer fb;
   sthread_t *worker;
   unsigned i, calls;
   int index;
   uint8_t *old;
   init_wrapper(&priv);
   backend.enabled = backend.phase = true;
   video_thread_mem_provision(thr);
   backend.phase = false;
   assert(priv.mapping_mask && backend.allocated == VIDEO_THREAD_FRAME_MEM_DRIVER);
   memset(&fb, 0, sizeof(fb));
   fb.width = 8; fb.height = 16; fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
   calls = backend.calls;
   assert(thread_get_mapped_framebuffer(thr, &fb));
   assert(!thr->frame.mem[thr->frame.slot[0].mem].id && fb.pitch == 16);
   assert(backend.calls == calls);
   for (i = 0; i < backend.allocated; i++)
      assert(!retro_atomic_load_acquire_int(&backend.exposed[i]));
   fb.width = 16;
   assert(thread_get_mapped_framebuffer(thr, &fb));
   index = thr->frame.slot[0].mem;
   assert(thr->frame.mem[index].id);
   assert(retro_atomic_load_acquire_int(&backend.exposed[thr->frame.mem[index].id - 1]));
   old = thr->frame.mem[index].ptr;
   thr->frame.lent = -1;
   thr->frame.mem_busy = 1u << index;
   retro_atomic_store_release_int(&thr->frame.mem[index].busy, 1);
   backend.phase = true;
   worker = sthread_create(remap_worker, &priv);
   assert(worker);
   while (!retro_atomic_load_acquire_int(&priv.mapping[index].ready))
      assert(thr->frame.mem[index].ptr == old);
   assert(thr->frame.mem[index].ptr == old);
   assert(retro_atomic_load_acquire_int(&thr->frame.mem[index].busy));
   sthread_join(worker);
   backend.phase = false;
   calls = backend.calls;
   assert(thread_get_mapped_framebuffer(thr, &fb));
   assert(fb.data != old && fb.data == thr->frame.mem[index].ptr);
   assert(!retro_atomic_load_acquire_int(&thr->frame.mem[index].busy));
   assert(!retro_atomic_load_acquire_int(&priv.mapping[index].ready));
   assert(backend.calls == calls);
   /* An unchanged address needs no producer-side pointer replacement. */
   thr->frame.lent = -1;
   thr->frame.mem_busy = 1u << index;
   retro_atomic_store_release_int(&thr->frame.mem[index].busy, 1);
   backend.phase = true;
   video_thread_mem_poll_mapped(thr);
   assert(!retro_atomic_load_acquire_int(&priv.mapping[index].ready));
   assert(!retro_atomic_load_acquire_int(&thr->frame.mem[index].busy));
   video_thread_mem_unprovision(thr);
   assert(!priv.mapping_mask);
   for (i = 2; i < VIDEO_THREAD_FRAME_MEM; i++)
      assert(!thr->frame.mem[i].id);
   /* Async shader preparation cannot modify producer-owned pool entries. */
   init_wrapper(&priv);
   backend.phase = true;
   video_thread_mem_provision(thr);
   assert(!priv.mapping_mask);
   backend.phase = false;
   fb.width = 8;
   assert(thread_get_mapped_framebuffer(thr, &fb));
   assert(fb.data == storage[0] && thr->frame.lent == 0);
   backend.phase = true;
   backend.enabled = true;
   video_thread_mapping_prepare(thr);
   assert(!priv.mapping_mask && !thr->frame.mem[2].ptr);
   backend.phase = false;
   video_thread_mapping_refresh(thr);
   assert(priv.mapping_mask && thr->frame.mem[2].id);
   assert(thr->frame.slot[0].mem == 0 && thr->frame.slot[0].buffer == storage[0]);
   assert(thr->frame.lent == 0);
   for (i = 0; i < VIDEO_THREAD_FRAME_MEM_DRIVER; i++)
      assert(!retro_atomic_load_acquire_int(&priv.mapping_pending[i].ready));
   backend.phase = true;
   video_thread_mem_unprovision(thr);
   backend.phase = false;
   init_wrapper(&priv);
   backend.enabled = backend.phase = true;
   backend.offset = 16;
   video_thread_mem_provision(thr);
   backend.phase = false;
   fb.width = fb.height = 16;
   assert(thread_get_mapped_framebuffer(thr, &fb));
   assert(!((uintptr_t)fb.data & 63) && !thr->frame.mem[thr->frame.slot[0].mem].id);
   for (i = 0; i < backend.allocated; i++)
      assert(!retro_atomic_load_acquire_int(&backend.exposed[i]));
   backend.phase = true;
   video_thread_mem_unprovision(thr);
   backend.phase = false;
   puts("PASS: renewable pointers, producer publication, tight core loans and deferred preparation");
}
static unsigned char cache_pixels[256], cache_other[256];
static void pinned(void *data, const void *pixels, unsigned dims, size_t pitch)
{
   assert(data && pixels == cache_pixels + 32 && dims && pitch);
   assert(!video_driver_cached_frame_try_retire(cache_pixels, sizeof(cache_pixels)));
   assert(*(const unsigned char*)pixels == 0x5a);
   assert(video_driver_cached_frame_try_retire(cache_other, sizeof(cache_other)));
}
static void retirement(void)
{
   memset(cache_pixels, 0x5a, sizeof(cache_pixels));
   frame_cache_store(cache_pixels + 32, VIDEO_SCALE_PACK(16, 8), 32);
   video_driver_cached_frame_read(cache_pixels, pinned);
   assert(video_driver_cached_frame_try_retire(cache_pixels, sizeof(cache_pixels)));
   assert(!retro_atomic_load_acquire_ptr(&frame_cache_data));
   frame_cache_store(cache_pixels, VIDEO_SCALE_PACK(16, 8), 32);
   retro_atomic_store_release_size(&video_driver_st.frame_cache_keeper, VIDEO_CACHE_KEEPER);
   assert(!video_driver_cached_frame_try_retire(cache_pixels, sizeof(cache_pixels)));
   assert(retro_atomic_load_acquire_ptr(&frame_cache_data) == cache_pixels);
   assert(video_driver_cached_frame_try_retire(cache_other, sizeof(cache_other)));
   assert(retro_atomic_load_acquire_ptr(&frame_cache_data) == cache_pixels);
   retro_atomic_store_release_size(&video_driver_st.frame_cache_keeper, (uintptr_t)(cache_pixels + 1));
   assert(!video_driver_cached_frame_try_retire(cache_pixels, sizeof(cache_pixels)));
   retro_atomic_store_release_size(&video_driver_st.frame_cache_keeper, 0);
   assert(video_driver_cached_frame_try_retire(cache_pixels, sizeof(cache_pixels)));
   puts("PASS: bounded retirement refuses pinned readers and keepers without unmapping");
}

#if defined(_WIN32) && defined(TEST_D3D10)
#include "frame_memory_d3d10_test.h"
#elif defined(_WIN32)
typedef ID3D11Device *D3D11Device;
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11Texture2D *D3D11Texture2D;
typedef ID3D11Resource *D3D11Resource;
#define D3D11_FRAME_MEM_MAX 5
#define D3D11_ST_FLAG_HW_IFACE_ENABLE 1
#define Release(resource) do { if (resource) (resource)->lpVtbl->Release(resource); } while (0)
typedef struct { D3D11Texture2D handle; D3D11_TEXTURE2D_DESC desc; } d3d11_texture_t;
typedef struct { unsigned history_size; } test_preset_t;
typedef struct
{
   D3D11Device device;
   D3D11DeviceContext context;
   DXGI_FORMAT format;
   unsigned flags;
   struct { test_preset_t *shader_preset; d3d11_texture_t history[1]; } chain;
   struct {
      D3D11Texture2D texture;
      D3D11_MAPPED_SUBRESOURCE mapped;
      unsigned width, height;
      DXGI_FORMAT format;
      bool valid;
      retro_atomic_int_t cache_exposed;
   } frame_mem[D3D11_FRAME_MEM_MAX];
   struct {
      D3D11Texture2D staging[3];
      D3D11_MAPPED_SUBRESOURCE mapped[3];
      unsigned mapped_bits, dims;
      DXGI_FORMAT format;
   } sw_lend;
} d3d11_video_t;
#include "d3d.inc"
static void device_memory(void)
{
   d3d11_video_t d3d;
   D3D11_TEXTURE2D_DESC desc;
   D3D11Texture2D readback;
   D3D11_MAPPED_SUBRESOURCE read;
   test_preset_t preset;
   D3D_FEATURE_LEVEL feature;
   HRESULT hr;
   unsigned pass, i, x, y;
   memset(&d3d, 0, sizeof(d3d));
   hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
         NULL, 0, D3D11_SDK_VERSION, &d3d.device, &feature, &d3d.context);
   assert(SUCCEEDED(hr));
   for (pass = 0; pass < 2; pass++)
   {
      uintptr_t ids[D3D11_FRAME_MEM_MAX];
      video_frame_memory_t map;
      unsigned bpp = pass ? 2 : 4, align;
      bool cached;
      void *ptr;
      int hazard;
      d3d.format = pass ? DXGI_FORMAT_B5G6R5_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM;
      d3d.chain.shader_preset = NULL;
      assert(!d3d11_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[0], &cached, &align));
      for (i = 0; i < D3D11_FRAME_MEM_MAX; i++) assert(!d3d.frame_mem[i].texture);
      preset.history_size = 1; d3d.chain.shader_preset = &preset;
      d3d.chain.history[0].desc.Width = 320;
      d3d.chain.history[0].desc.Format = d3d.format;
      for (i = 0; i < D3D11_FRAME_MEM_MAX; i++)
      {
         ptr = d3d11_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[i], &cached, &align);
         assert(ptr && cached && align >= 320 * bpp);
      }
      assert(!d3d11_frame_mem_alloc(&d3d, 512 * 512 * bpp, &ids[0], &cached, &align));
      assert(d3d11_frame_mem_map(&d3d, ids[0], &map));
      for (y = 0; y < map.height; y++)
         for (x = 0; x < map.width; x++)
            if (bpp == 4)
               ((uint32_t*)((uint8_t*)map.data + y * map.pitch))[x] = 0xff000000u | (y * 513 + x);
            else
               ((uint16_t*)((uint8_t*)map.data + y * map.pitch))[x] = (uint16_t)(y * 513 + x);
      memset(&desc, 0, sizeof(desc));
      desc.Width = 256; desc.Height = 224;
      desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
      desc.Format = d3d.format; desc.Usage = D3D11_USAGE_DEFAULT;
      d3d.chain.history[0].desc = desc;
      assert(SUCCEEDED(d3d.device->lpVtbl->CreateTexture2D(d3d.device, &desc,
               NULL, &d3d.chain.history[0].handle)));
      ptr = (uint8_t*)map.data + 2 * map.pitch + 3 * bpp;
      retro_atomic_store_release_int(map.cache_exposed, 1);
      frame_cache_store(ptr, VIDEO_SCALE_PACK(256, 224), map.pitch);
      hazard = frame_cache_hazard_acquire(ptr);
      assert(hazard >= 0);
      assert(!d3d11_frame_mem_push(&d3d, d3d.context, ptr, 256, 224, (unsigned)map.pitch));
      assert(d3d.frame_mem[ids[0] - 1].valid);
      frame_cache_hazard_release(hazard);
      assert(d3d11_frame_mem_push(&d3d, d3d.context, ptr, 256, 224, (unsigned)map.pitch));
      assert(!d3d.frame_mem[ids[0] - 1].valid);
      assert(!retro_atomic_load_acquire_int(map.cache_exposed));
      desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      assert(SUCCEEDED(d3d.device->lpVtbl->CreateTexture2D(d3d.device, &desc, NULL, &readback)));
      d3d.context->lpVtbl->CopyResource(d3d.context, (ID3D11Resource*)readback,
            (ID3D11Resource*)d3d.chain.history[0].handle);
      assert(SUCCEEDED(d3d.context->lpVtbl->Map(d3d.context, (ID3D11Resource*)readback,
               0, D3D11_MAP_READ, 0, &read)));
      for (y = 0; y < 224; y++)
         for (x = 0; x < 256; x++)
            if (bpp == 4)
               assert((((uint32_t*)((uint8_t*)read.pData + y * read.RowPitch))[x] & 0xffffff)
                     == ((y + 2) * 513 + x + 3));
            else
               assert(((uint16_t*)((uint8_t*)read.pData + y * read.RowPitch))[x]
                     == (uint16_t)((y + 2) * 513 + x + 3));
      d3d.context->lpVtbl->Unmap(d3d.context, (ID3D11Resource*)readback, 0);
      Release(readback);
      assert(d3d11_frame_mem_ready(&d3d, ids[0]));
      assert(d3d11_frame_mem_map(&d3d, ids[0], &map));
      assert(map.data && map.pitch >= 320 * bpp);
      d3d.sw_lend.staging[0] = d3d.frame_mem[ids[0] - 1].texture;
      d3d.sw_lend.mapped[0] = d3d.frame_mem[ids[0] - 1].mapped;
      d3d.sw_lend.mapped_bits = 1;
      d3d.sw_lend.dims = VIDEO_SCALE_PACK(map.width, map.height);
      d3d.sw_lend.format = d3d.format;
      frame_cache_store(map.data, VIDEO_SCALE_PACK(256, 224), map.pitch);
      hazard = frame_cache_hazard_acquire(map.data);
      assert(hazard >= 0);
      assert(!d3d11_sw_lend_push(&d3d, d3d.context, 0, 0, 0, 256, 224));
      assert(d3d.sw_lend.mapped_bits == 1);
      frame_cache_hazard_release(hazard);
      assert(d3d11_sw_lend_push(&d3d, d3d.context, 0, 0, 0, 256, 224));
      d3d.frame_mem[ids[0] - 1].valid = false;
      d3d.context->lpVtbl->Flush(d3d.context);
      for (i = 0; i < 100000 && !d3d11_frame_mem_ready(&d3d, ids[0]); i++)
         Sleep(0);
      assert(i < 100000);
      assert(d3d11_frame_mem_map(&d3d, ids[0], &map));
      /* A switch back to stock must leave its direct renderer in charge. */
      preset.history_size = 0;
      assert(!d3d11_frame_mem_push(&d3d, d3d.context, map.data, 256, 224, (unsigned)map.pitch));
      assert(d3d.frame_mem[ids[0] - 1].valid);
      for (i = 0; i < D3D11_FRAME_MEM_MAX; i++) d3d11_frame_mem_free(&d3d, ids[i]);
      Release(d3d.chain.history[0].handle);
   }
   Release(d3d.context); Release(d3d.device);
   puts("PASS: native D3D11 RGB565/XRGB8888 cropped GPU copies, remapping and stock exclusion");
}
#endif
int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(storage) / sizeof(storage[0]); i++)
   {
      storage[i] = (unsigned char*)memalign_alloc(64, 1024);
      assert(storage[i]);
      memset(storage[i], (int)i, 1024);
   }
   retirement();
   mappings();
#ifdef _WIN32
   device_memory();
#endif
   for (i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) memalign_free(storage[i]);
   return 0;
}
