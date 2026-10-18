#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <formats/image.h>

#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#ifndef FAILED
#define FAILED(x) ((x) < 0)
#endif
#define D3D11_ST_FLAG_HDR_ENABLE (1u << 11)
#define D3D12_ST_FLAG_HDR_ENABLE (1u << 11)
#define D3D_FEATURE_LEVEL_11_0 11
#define D3D11_FORMAT_SUPPORT_TEXTURE2D 1u
#define D3D12_FORMAT_SUPPORT1_TEXTURE2D 1u
#define D3D12_FORMAT_SUPPORT1_NONE 0u
#define D3D12_FORMAT_SUPPORT2_NONE 0u
#define D3D12_FEATURE_FORMAT_SUPPORT 1u

typedef unsigned UINT;
typedef enum { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_BC1_UNORM,
   DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC3_UNORM,
   DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT,
   DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
   DXGI_FORMAT_B8G8R8X8_UNORM } DXGI_FORMAT;
typedef struct { DXGI_FORMAT Format; unsigned Support1, Support2; }
   D3D12_FEATURE_DATA_FORMAT_SUPPORT;
struct device;
struct vtable
{
   int (*CheckFormatSupport)(struct device*, DXGI_FORMAT, unsigned*);
   int (*CheckFeatureSupport)(struct device*, unsigned,
         D3D12_FEATURE_DATA_FORMAT_SUPPORT*, size_t);
};
struct device { const struct vtable *lpVtbl; };
typedef struct
{
   struct device *device;
   unsigned flags;
   unsigned supportedFeatureLevel;
   void *planar_pipe;
   retro_atomic_int_t planar_failed;
   retro_atomic_int_t texture_hdr_enabled;
   struct { bool failed; retro_atomic_int_t allowed; } views;
} test_video_t;
typedef test_video_t d3d11_video_t;
typedef test_video_t d3d12_video_t;

static unsigned queries;
static int check_format(struct device *d, DXGI_FORMAT f, unsigned *support)
{
   (void)d;
   queries++;
   *support = f == DXGI_FORMAT_BC1_UNORM ? 1 : 0;
   return 0;
}
static int check_feature(struct device *d, unsigned feature,
      D3D12_FEATURE_DATA_FORMAT_SUPPORT *support, size_t size)
{
   (void)feature;
   (void)size;
   return check_format(d, support->Format, &support->Support1);
}
#include "d3d.inc"
#if TEST_D3D == 11
#define query d3d11_gfx_supports_texture_format
#define publish d3d11_views_publish
#else
#define query d3d12_gfx_supports_texture_format
#define publish d3d12_views_publish
#endif

static void rebuild(void *data)
{
   test_video_t *v = (test_video_t*)data;
   unsigned i;
   for (i = 0; i < 100000; i++)
   {
      v->flags = (i & 1) ? D3D11_ST_FLAG_HDR_ENABLE : 0;
      retro_atomic_store_release_int(&v->planar_failed, (i & 2) != 0);
      publish(v);
   }
}

int main(void)
{
   static const struct vtable table = { check_format, check_feature };
   struct device device;
   test_video_t v;
   sthread_t *worker;
   unsigned i;
   volatile unsigned observed = 0;
#if TEST_D3D == 12
   CHECK(d3d12_frame_bpp(DXGI_FORMAT_R16G16B16A16_FLOAT) == 8);
   CHECK(d3d12_frame_bpp(DXGI_FORMAT_R10G10B10A2_UNORM) == 4);
   CHECK(d3d12_frame_bpp(DXGI_FORMAT_B8G8R8X8_UNORM) == 4);
   CHECK(d3d12_frame_bpp(DXGI_FORMAT_B5G6R5_UNORM) == 2);
#endif
   memset(&v, 0, sizeof(v));
   device.lpVtbl = &table;
   CHECK(!query(NULL, TEXTURE_GPU_FORMAT_SCRGB));
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_YUV420));
   v.device = &device;
   v.supportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
   v.planar_pipe = &device;
   CHECK(query(&v, TEXTURE_GPU_FORMAT_YUV420));
   retro_atomic_store_release_int(&v.planar_failed, 1);
#if TEST_D3D == 11
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_YUV420));
   retro_atomic_store_release_int(&v.planar_failed, 0);
   v.supportedFeatureLevel = 10;
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_YUV420));
   v.supportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
#else
   v.planar_pipe = NULL;
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_YUV420));
   v.planar_pipe = &device;
#endif
   publish(&v);
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_SCRGB));
   v.flags = D3D11_ST_FLAG_HDR_ENABLE;
   publish(&v);
#ifdef HAVE_DXGI_HDR
   CHECK(query(&v, TEXTURE_GPU_FORMAT_SCRGB));
#else
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_SCRGB));
#endif
   CHECK(query(&v, TEXTURE_GPU_FORMAT_RGBA16F));
   CHECK(query(&v, TEXTURE_GPU_FORMAT_RGB10A2));
   CHECK(queries == 0);
   CHECK(query(&v, TEXTURE_GPU_FORMAT_BC1));
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_BC7));
   worker = sthread_create(rebuild, &v);
   CHECK(worker);
   for (i = 0; i < 200000; i++)
   {
      observed += query(&v, TEXTURE_GPU_FORMAT_SCRGB);
      observed += query(&v, TEXTURE_GPU_FORMAT_YUV420);
   }
   sthread_join(worker);
   v.flags = 0;
   publish(&v);
   CHECK(!query(&v, TEXTURE_GPU_FORMAT_SCRGB));
   printf("PASS: D3D%d published capabilities; %u observations\n",
         TEST_D3D, observed);
   return 0;
}
