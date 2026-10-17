#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/video_hdr_metadata.h"

#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

typedef struct
{
   struct { float video_hdr_paper_white_nits; } floats;
   struct { unsigned video_hdr_expand_gamut, video_hdr_mode; } uints;
   struct { char video_driver[32]; } arrays;
} settings_t;
static settings_t settings;
static bool active;
static bool configured_10bit;
static settings_t *config_get_ptr(void) { return &settings; }
static void *video_driver_get_ptr(void) { return active ? &settings : NULL; }
static bool video_driver_supports_10bit_source(void) { return configured_10bit; }
#define HAVE_D3D11
#define HAVE_D3D12
#define HAVE_VULKAN
#define HAVE_OPENGL_CORE
static retro_atomic_int_t video_hdr_output_mode;
static bool string_is_equal(const char *a, const char *b)
{
   return a && b && strcmp(a, b) == 0;
}
#include "fp16.inc"
static float video_driver_get_hdr_max_nits(void) { return 600.0f; }

#include "provider.inc"

typedef unsigned UINT;
typedef unsigned short UINT16;
typedef int DXGI_COLOR_SPACE_TYPE;
enum dxgi_swapchain_bit_depth { DXGI_SWAPCHAIN_BIT_DEPTH_10 = 10 };
#define DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 12
#define DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 1
#define DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 0
#define DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT 1
#define DXGI_HDR_METADATA_TYPE_NONE 0
#define DXGI_HDR_METADATA_TYPE_HDR10 1
#ifndef FAILED
#define FAILED(x) ((x) < 0)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(x) ((x) >= 0)
#endif
#define RARCH_ERR(x) ((void)(x))
typedef struct
{
   float red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y;
} display_chromaticities_t;
typedef struct
{
   UINT16 RedPrimary[2], GreenPrimary[2], BluePrimary[2], WhitePoint[2];
   UINT MaxMasteringLuminance, MinMasteringLuminance;
   UINT16 MaxContentLightLevel, MaxFrameAverageLightLevel;
} DXGI_HDR_METADATA_HDR10;
struct chain;
typedef struct chain *DXGISwapChain;
struct vtable
{
   int (*SetHDRMetaData)(DXGISwapChain, unsigned, unsigned, const void *);
   int (*CheckColorSpaceSupport)(DXGISwapChain, int, unsigned *);
   int (*SetColorSpace1)(DXGISwapChain, int);
};
struct chain
{
   const struct vtable *lpVtbl;
   unsigned calls;
   unsigned type;
   bool fail;
   bool have_v3;
   bool support;
   bool color_fail;
   int color_space;
   DXGI_HDR_METADATA_HDR10 metadata;
};
static const int libretro_IID_IDXGISwapChain3 = 3;
static const int libretro_IID_IDXGISwapChain4 = 4;
static bool dxgi_swapchain_implements(DXGISwapChain chain, const int *iid)
{ return chain && (*iid == 4 || (*iid == 3 && chain->have_v3)); }
static int check_space(DXGISwapChain chain, int space, unsigned *support)
{
   (void)space;
   *support = chain->support ? DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT : 0;
   return 0;
}
static int set_space(DXGISwapChain chain, int space)
{
   if (chain->color_fail)
      return -1;
   chain->color_space = space;
   return 0;
}
static int submit(DXGISwapChain chain, unsigned type, unsigned size, const void *data)
{
   chain->calls++;
   if (chain->fail)
      return -1;
   chain->type = type;
   if (type == DXGI_HDR_METADATA_TYPE_HDR10)
   {
      CHECK(size == sizeof(chain->metadata));
      memcpy(&chain->metadata, data, size);
   }
   else
      CHECK(!size && !data);
   return 0;
}
#include "dxgi.inc"

static void publish(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < 100000; i++)
      video_driver_set_hdr_output_mode(i % 3);
}

static void submit_thread(void *data)
{
   unsigned i;
   for (i = 0; i < 10000; i++)
      dxgi_set_hdr_metadata((DXGISwapChain)data, true,
            DXGI_SWAPCHAIN_BIT_DEPTH_10, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
            1000.0f, 0.001f, 800.0f, 400.0f, 0.0f);
}

int main(void)
{
   struct retro_hdr_metadata hdr;
   static const struct vtable table = { submit, check_space, set_space };
   struct chain a, b;
   sthread_t *worker;
   unsigned i;
   int cached_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
   memset(&a, 0, sizeof(a));
   memset(&b, 0, sizeof(b));
   a.lpVtbl = b.lpVtbl = &table;
   a.have_v3 = a.support = true;
   settings.floats.video_hdr_paper_white_nits = 203.0f;
   settings.uints.video_hdr_expand_gamut = 2;
   configured_10bit = true;
   hdr.version = 1;
   hdr.size = sizeof(hdr);
   video_driver_set_hdr_output_mode(1);
   CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK(hdr.output_mode == 0 && (hdr.source_formats & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010));
   CHECK(hdr.source_formats == RETRO_HDR_SOURCE_FORMAT_HDR10_2101010);
   settings.uints.video_hdr_mode = 2;
   strcpy(settings.arrays.video_driver, "d3d12");
   CHECK(video_driver_supports_fp16_source());
   CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK(hdr.source_formats == (RETRO_HDR_SOURCE_FORMAT_HDR10_2101010
            | RETRO_HDR_SOURCE_FORMAT_SCRGB_RGBA16F));
   settings.uints.video_hdr_mode = 0;
   active = true;
   retro_atomic_store_release_int(&video_hdr_source_10bpc, 1);
   CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK(hdr.output_mode == 1 && !(hdr.source_formats & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010));
   retro_atomic_store_release_int(&video_hdr_source_10bpc, 2);
   worker = sthread_create(publish, NULL);
   CHECK(worker);
   for (i = 0; i < 100000; i++)
   {
      CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
      CHECK(hdr.output_mode <= 2 && (hdr.source_formats & RETRO_HDR_SOURCE_FORMAT_HDR10_2101010));
      CHECK(hdr.passthrough == (hdr.output_mode != 0));
      CHECK(hdr.transfer_function == (hdr.output_mode == 1 ? 16u
               : (hdr.output_mode == 2 ? 8u : 0u)));
   }
   sthread_join(worker);
   settings.uints.video_hdr_mode = 2;
   video_driver_set_hdr_output_mode(1);
   CHECK(!video_driver_supports_fp16_source());
   video_driver_set_hdr_output_mode(2);
   CHECK(video_driver_supports_fp16_source());
   strcpy(settings.arrays.video_driver, "d3d11");
   CHECK(video_driver_supports_fp16_source());
   strcpy(settings.arrays.video_driver, "vulkan");
   CHECK(video_driver_supports_fp16_source());
   strcpy(settings.arrays.video_driver, "glcore");
   CHECK(video_driver_supports_fp16_source());
   strcpy(settings.arrays.video_driver, "sdl2");
   CHECK(!video_driver_supports_fp16_source());
   settings.uints.video_hdr_mode = 0;

   video_driver_set_hdr_output_mode(0);
   CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr));
   CHECK(!hdr.output_mode && !hdr.passthrough);

   {
      struct retro_display_metadata display;
      memset(&display, 0, sizeof(display));
      display.version = 1;
      display.size    = sizeof(display);
      CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA, &display));
      CHECK(!display.geo.native_width && !display.geo.native_height);
      video_driver_set_display_geometry((1920u << 16) | 1080u, (527u << 16) | 296u);
      CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA, &display));
      CHECK(display.geo.native_width == 1920 && display.geo.native_height == 1080);
      CHECK(display.geo.width_mm == 527 && display.geo.height_mm == 296);
      /* One axis alone, or a half past the sign bit, is unknown */
      video_driver_set_display_geometry(1080u, (527u << 16));
      CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA, &display));
      CHECK(!display.geo.native_width && !display.geo.native_height);
      CHECK(!display.geo.width_mm && !display.geo.height_mm);
      video_driver_set_display_geometry((0x8000u << 16) | 1080u, 0);
      CHECK(video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_DISPLAY_METADATA, &display));
      CHECK(!display.geo.native_width && !display.geo.native_height);
      video_driver_set_display_geometry(0, 0);
   }

   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
   CHECK(a.color_space == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 1);
   a.color_fail = true;
   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 1);
   CHECK(cached_space == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
   a.color_fail = false;
   a.support = false;
   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 1);
   a.support = true;
   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 2);
   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 0);
   a.have_v3 = false;
   dxgi_swapchain_color_space(&a, &cached_space,
         DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
   CHECK(retro_atomic_load_acquire_int(&video_hdr_output_mode) == 0);

   /* Identical metadata must reach both swapchains, including after reset. */
   submit_thread(&a);
   submit_thread(&b);
   CHECK(a.calls > 0 && b.calls > 0);
   worker = sthread_create(submit_thread, &a);
   CHECK(worker);
   submit_thread(&b);
   sthread_join(worker);
   CHECK(a.calls > 0 && b.calls > 0);
   CHECK(a.metadata.MaxContentLightLevel == 800);
   CHECK(b.metadata.MaxFrameAverageLightLevel == 400);
   dxgi_set_hdr_metadata(&b, true, DXGI_SWAPCHAIN_BIT_DEPTH_10, 0,
         1000, 0, 800, 400, 0);
   CHECK(b.type == DXGI_HDR_METADATA_TYPE_NONE);
   dxgi_set_hdr_metadata(&b, true, DXGI_SWAPCHAIN_BIT_DEPTH_10,
         DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, 1000, 0, 800, 400, 600);
   CHECK(b.type == DXGI_HDR_METADATA_TYPE_HDR10);
   CHECK(b.metadata.MaxContentLightLevel == 600);
   CHECK(b.metadata.MaxFrameAverageLightLevel == 400);
   puts("PASS: shipping provider publication and independent DXGI metadata ownership");
   return 0;
}
