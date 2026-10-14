/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <gfx/scaler/pixconv.h>
#include "formats.inc"
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef unsigned UINT;
typedef int HRESULT;
#define FAILED(hr) ((hr) < 0)
#define DXGI_ERROR_WAS_STILL_DRAWING (-2)
#define D3D10_MAP_WRITE 1
#define D3D10_MAP_FLAG_DO_NOT_WAIT 2
#define D3D10_RESOURCE_MISC_GENERATE_MIPS 1
#define RARCH_FILTER_LINEAR 0
#define RARCH_FILTER_NEAREST 1
#define RARCH_WRAP_EDGE 0
enum texture_filter_type { TEXTURE_FILTER_MIPMAP_LINEAR, TEXTURE_FILTER_LINEAR,
   TEXTURE_FILTER_MIPMAP_NEAREST, TEXTURE_FILTER_NEAREST };
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED, VIDEO_TEXTURE_UPDATE_DROPPED,
   VIDEO_TEXTURE_UPDATE_DONE };
typedef struct { void *pData; UINT RowPitch; } D3D10_MAPPED_TEXTURE2D;
typedef struct { UINT left, top, front, right, bottom, back; } D3D10_BOX;
struct resource;
typedef struct {
   HRESULT (*Map)(struct resource*, UINT, UINT, UINT, D3D10_MAPPED_TEXTURE2D*);
   void (*Unmap)(struct resource*, UINT);
} resource_vtbl_t;
typedef struct resource { resource_vtbl_t *lpVtbl; } resource_t;
typedef resource_t *D3D10Resource;
struct device;
typedef struct {
   void (*CopySubresourceRegion)(struct device*, D3D10Resource, UINT, UINT, UINT, UINT,
         D3D10Resource, UINT, const D3D10_BOX*);
   void (*GenerateMips)(struct device*, void*);
} device_vtbl_t;
typedef struct device { device_vtbl_t *lpVtbl; } device_t;
typedef device_t *D3D10Device;
typedef struct { struct { UINT Width, Height, MiscFlags; DXGI_FORMAT Format; } desc;
   resource_t *handle, *staging; void *sampler, *view; } d3d10_texture_t;
typedef struct { D3D10Device device; void *samplers[2][1]; } d3d10_video_t;
struct texture_image { unsigned width, height; uint32_t *pixels; bool pix10; };
static uint32_t staging_pixels[3], gpu_pixels[3];
static HRESULT map_result;
static unsigned copies, nonblocking, failures;
static DXGI_FORMAT fallback;

static HRESULT map(resource_t *r, UINT sub, UINT mode, UINT flags, D3D10_MAPPED_TEXTURE2D *m)
{
   (void)r; (void)sub; (void)mode;
   if (flags == D3D10_MAP_FLAG_DO_NOT_WAIT) nonblocking++;
   m->pData = staging_pixels; m->RowPitch = sizeof(staging_pixels);
   return map_result;
}
static void unmap(resource_t *r, UINT sub) { (void)r; (void)sub; }
static void copy_resource(device_t *d, D3D10Resource dst, UINT sub, UINT x, UINT y, UINT z,
      D3D10Resource src, UINT srcsub, const D3D10_BOX *box)
{
   (void)d; (void)dst; (void)sub; (void)x; (void)y; (void)z; (void)src; (void)srcsub; (void)box;
   memcpy(gpu_pixels, staging_pixels, sizeof(gpu_pixels)); copies++;
}
static void mipmaps(device_t *d, void *view) { (void)d; (void)view; }
static resource_vtbl_t resource_vtbl = { map, unmap };
static resource_t stage = { &resource_vtbl }, gpu = { &resource_vtbl };
static device_vtbl_t device_vtbl = { copy_resource, mipmaps };
static void d3d10_release_texture(d3d10_texture_t *t) { (void)t; }
static void d3d10_init_texture(D3D10Device d, d3d10_texture_t *t)
{
   (void)d; t->desc.Format = fallback; t->handle = &gpu; t->staging = &stage;
}
#include "copy.inc"
#include "driver.inc"

int main(void)
{
   device_t device = { &device_vtbl };
   d3d10_video_t video;
   struct texture_image image;
   d3d10_texture_t *texture;
   DXGI_FORMAT formats[2] = { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
   uint32_t pink[2] = { UINT32_C(0xfff07030), UINT32_C(0xffd09060) };
   unsigned i, before;
   memset(&video, 0, sizeof(video)); video.device = &device;
   memset(&image, 0, sizeof(image)); image.width = 2; image.height = 1; image.pixels = pink;
   for (i = 0; i < 2; i++)
   {
      uint32_t initial[3];
      fallback = formats[i]; map_result = 0;
      texture = (d3d10_texture_t*)d3d10_gfx_load_texture_internal(&video, &image, TEXTURE_FILTER_LINEAR);
      if (!texture) { failures++; continue; }
      memcpy(initial, gpu_pixels, sizeof(initial));
      memset(staging_pixels, 0, sizeof(staging_pixels));
      if (d3d10_gfx_update_texture_internal(&video, (uintptr_t)texture, &image)
            != VIDEO_TEXTURE_UPDATE_DONE || memcmp(initial, gpu_pixels, 8)) failures++;
      if (i == 1 && gpu_pixels[0] != UINT32_C(0xff3070f0)) failures++;
      before = copies; map_result = DXGI_ERROR_WAS_STILL_DRAWING;
      if (d3d10_gfx_update_texture_internal(&video, (uintptr_t)texture, &image)
            != VIDEO_TEXTURE_UPDATE_DROPPED || copies != before) failures++;
      map_result = -1;
      if (d3d10_gfx_update_texture_internal(&video, (uintptr_t)texture, &image)
            != VIDEO_TEXTURE_UPDATE_REFUSED || copies != before) failures++;
      free(texture);
   }
   if (nonblocking != 6) failures++;
   printf("D3D preview update formats: %u failures\n", failures);
   return failures ? 1 : 0;
}
