#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#define _WIN32
#endif
#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0
#define VULKAN_WSI_WIN32 1
#define VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN 1
#define VIDEO_SCALE_PACK(w, h) (((w) << 16) | (h))
#define true 1
typedef void ra_vulkan_dxgi_t;
typedef struct {
   unsigned device, swapchain_dims, swapchain_format, swap_interval;
   unsigned swapchain_images[2], swapchain_semaphores[2], swapchain_fences[2];
   unsigned acquire, stale, recycled;
} context;
typedef struct {
   context context;
   void *dxgi_presenter;
   unsigned flags, dxgi_failed, wsi_type;
} gfx_ctx_vulkan_data_t;
typedef struct { unsigned dxgi_present; } settings;
static settings config = {1};
static settings *ss = &config;
static unsigned waits, clears, destroys, creates, resized = 1, fail_create;
static void vulkan_context_wait_frames(gfx_ctx_vulkan_data_t *vk)
{ assert(vk); ++waits; }
static void vulkan_acquire_clear_fences(gfx_ctx_vulkan_data_t *vk)
{ memset(vk->context.swapchain_fences, 0, sizeof(vk->context.swapchain_fences)); ++clears; }
static void vulkan_destroy_swapchain(gfx_ctx_vulkan_data_t *vk)
{ memset(&vk->context, 0, sizeof(vk->context)); vk->dxgi_presenter = NULL; ++destroys; }
static unsigned ra_vulkan_dxgi_resize(ra_vulkan_dxgi_t *p,
      unsigned width, unsigned height, unsigned format)
{ assert(p && width == 640 && height == 480 && format == 28); return resized; }
static unsigned ra_vulkan_dxgi_image(ra_vulkan_dxgi_t *p, unsigned i)
{ assert(p); return 20 + i; }
static unsigned vkCreateSemaphore(unsigned device, const unsigned *info,
      void *allocator, unsigned *semaphore)
{
   assert(device == 1 && info && !allocator); ++creates;
   if (fail_create) return 1;
   *semaphore = 30 + creates; return VK_SUCCESS;
}
static int rebuild(gfx_ctx_vulkan_data_t *vk)
{
   struct { unsigned width, height; } swapchain_size = {640, 480};
   unsigned import_format = 28, swap_interval = 1;
   unsigned i, semaphore = 0;
#include "drain.inc"
#include "reuse.inc"
   if (!vk->dxgi_presenter) return 0;
#include "images.inc"
   return 1;
dxgi_unavailable:
   return 0;
}
static void init(gfx_ctx_vulkan_data_t *vk)
{
   memset(vk, 0, sizeof(*vk));
   vk->dxgi_presenter = vk; vk->wsi_type = VULKAN_WSI_WIN32;
   vk->context.device = 1; vk->context.swapchain_format = 28;
   vk->context.swapchain_semaphores[0] = 7; vk->context.swapchain_semaphores[1] = 8;
   vk->context.swapchain_fences[0] = 9; vk->context.swapchain_fences[1] = 10;
   vk->context.acquire = 11; vk->context.stale = 12; vk->context.recycled = 13;
}
int main(void)
{
   gfx_ctx_vulkan_data_t vk;
   context saved;
   init(&vk); saved = vk.context;
   assert(rebuild(&vk));
   assert(waits == 1 && !clears && !destroys && !creates);
   saved.swapchain_images[0] = 20; saved.swapchain_images[1] = 21;
   assert(!memcmp(&saved, &vk.context, sizeof(saved)));
   vk.context.swapchain_dims = VIDEO_SCALE_PACK(640, 480);
   assert(rebuild(&vk) && !clears && !destroys && !creates);
   assert(vk.context.swap_interval == 1);
   init(&vk); vk.context.swapchain_semaphores[1] = VK_NULL_HANDLE;
   assert(rebuild(&vk) && creates == 1 && vk.context.swapchain_semaphores[0] == 7);
   init(&vk); vk.context.swapchain_semaphores[1] = VK_NULL_HANDLE; fail_create = 1;
   assert(!rebuild(&vk) && destroys == 1 && !vk.dxgi_presenter);
   init(&vk); resized = 0;
   assert(!rebuild(&vk) && destroys == 2 && !vk.dxgi_presenter);
   init(&vk); vk.dxgi_failed = 1;
   assert(!rebuild(&vk) && clears == 1);
   puts("PASS: Vulkan/DXGI frontend sync retention and fallback cleanup");
   return 0;
}
