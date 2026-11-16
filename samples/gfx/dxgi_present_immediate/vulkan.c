/* vulkan_present_immediate() on Windows: vulkan.inc, cut by run.py. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <boolean.h>

#ifndef _WIN32
#define _WIN32 1
#endif
typedef void *HWND;
typedef size_t VkSwapchainKHR;
#define VK_NULL_HANDLE 0
#define VK_PRESENT_MODE_IMMEDIATE_KHR 0
#define VK_PRESENT_MODE_FIFO_KHR 2
typedef struct ra_vulkan_dxgi ra_vulkan_dxgi_t;

typedef struct
{
   int swap_interval;
   int swapchain_present_mode;
   void *dxgi_presenter;
   void *dxgi_window;
   VkSwapchainKHR swapchain;
} vulkan_context_t;
typedef vulkan_context_t gfx_ctx_vulkan_data_t;
#define VULKAN_CTX_DATA_FROM_CONTEXT(ctx) (ctx)

typedef struct
{
   vulkan_context_t *context;
   VkSwapchainKHR flips_asked;
   VkSwapchainKHR flips_for;
} vk_t;

static int asks, flips, bridge_immediate;
static HWND asked_window;

static int ra_vulkan_dxgi_immediate(ra_vulkan_dxgi_t *p)
{ (void)p; return bridge_immediate; }
static int ra_vulkan_dxgi_window_flips_fullscreen(HWND window)
{ ++asks; asked_window = window; return flips; }

#include "vulkan.inc"

int main(void)
{
   vulkan_context_t ctx;
   vk_t vk;
   ctx.swap_interval          = 0;
   ctx.swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
   ctx.dxgi_presenter         = NULL;
   ctx.dxgi_window            = &ctx;
   ctx.swapchain              = 7;
   vk.context                 = &ctx;
   vk.flips_asked             = VK_NULL_HANDLE;
   vk.flips_for               = VK_NULL_HANDLE;

   assert(!vulkan_present_immediate(NULL));

   /* a composed output: immediate mode alone is not a tearing present */
   assert(!vulkan_present_immediate(&vk) && asks == 1);
   assert(asked_window == (HWND)&ctx);
   /* asked once a swapchain */
   assert(!vulkan_present_immediate(&vk) && asks == 1);
   flips         = 1;
   ctx.swapchain = 8;
   assert(vulkan_present_immediate(&vk) && asks == 2);
   assert(vulkan_present_immediate(&vk) && asks == 2);
   /* the mode is still required, and costs no ask */
   ctx.swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   assert(!vulkan_present_immediate(&vk) && asks == 2);
   ctx.swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
   /* the next swapchain may be on another output */
   flips         = 0;
   ctx.swapchain = 9;
   assert(!vulkan_present_immediate(&vk) && asks == 3);
   /* none: nothing to say, and an earlier answer is not it */
   flips         = 1;
   ctx.swapchain = 8;
   assert(vulkan_present_immediate(&vk) && asks == 4);
   ctx.swapchain = VK_NULL_HANDLE;
   assert(!vulkan_present_immediate(&vk));

   /* through DXGI the presenter answers, and the window is not asked */
   asks               = 0;
   ctx.swapchain      = 8;
   ctx.dxgi_presenter = &ctx;
   bridge_immediate   = 0;
   assert(!vulkan_present_immediate(&vk) && asks == 0);
   bridge_immediate   = 1;
   assert(vulkan_present_immediate(&vk) && asks == 0);
   ctx.swap_interval  = 1;
   assert(!vulkan_present_immediate(&vk) && asks == 0);
   puts("PASS: Vulkan immediate present asks its window's output");
   return 0;
}
