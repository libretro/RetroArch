#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define VK_SUCCESS 0
#define VK_TRUE 1
#define HAVE_THREADS
#define VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 8
typedef int VkResult;
typedef unsigned VkFence;
typedef struct { unsigned sType, flags; const void *pNext; } VkFenceCreateInfo;
typedef unsigned slock_t;
typedef struct { unsigned device, queue; VkFence resize_fence; int submitted; void *queue_lock; } ra_vulkan_dxgi_t;
static unsigned locked;
static void slock_lock(slock_t *lock) { assert(lock && !locked); locked = 1; }
static void slock_unlock(slock_t *lock) { assert(lock && locked); locked = 0; }
static unsigned creates, submits, waits, resets, step;
static VkResult create_result, submit_result, wait_result, reset_result;
static VkResult vkCreateFence(unsigned device, const VkFenceCreateInfo *info,
      void *allocator, VkFence *fence)
{
   assert(device == 1 && info->sType == 8 && !info->flags && !allocator);
   ++creates;
   if (create_result == VK_SUCCESS) *fence = 9;
   return create_result;
}
static VkResult vkQueueSubmit(unsigned queue, unsigned count,
      const void *submit, VkFence fence)
{
   assert(queue == 2 && count == 0 && !submit && fence == 9 && step == 0 && locked);
   ++submits; step = 1; return submit_result;
}
static VkResult vkWaitForFences(unsigned device, unsigned count,
      const VkFence *fence, unsigned all, uint64_t timeout)
{
   assert(device == 1 && count == 1 && *fence == 9 && all && timeout == UINT64_MAX);
   assert(step == 1 && !locked); ++waits; step = 2; return wait_result;
}
static VkResult vkResetFences(unsigned device, unsigned count, const VkFence *fence)
{
   assert(device == 1 && count == 1 && *fence == 9 && step == 2);
   ++resets; step = 0; return reset_result;
}
#define ra_vulkan_dxgi_drain ra_vulkan_dxgi_wait_resize
#include "wait.inc"
int main(void)
{
   ra_vulkan_dxgi_t p;
   slock_t lock = 0;
   memset(&p, 0, sizeof(p)); p.device = 1; p.queue = 2;
   p.queue_lock = &lock;
   create_result = -1;
   assert(ra_vulkan_dxgi_wait_resize(&p) == -1 && !submits && !p.resize_fence);
   create_result = VK_SUCCESS;
   assert(ra_vulkan_dxgi_wait_resize(&p) == VK_SUCCESS);
   assert(ra_vulkan_dxgi_wait_resize(&p) == VK_SUCCESS);
   assert(creates == 2 && submits == 2 && waits == 2 && resets == 2 && p.submitted);
   submit_result = -2;
   assert(ra_vulkan_dxgi_wait_resize(&p) == -2 && waits == 2);
   step = 0; submit_result = VK_SUCCESS; wait_result = -3;
   assert(ra_vulkan_dxgi_wait_resize(&p) == -3 && resets == 2);
   step = 0; wait_result = VK_SUCCESS; reset_result = -4;
   assert(ra_vulkan_dxgi_wait_resize(&p) == -4);
   puts("PASS: Vulkan/DXGI resize fence reuse, ordering and failures");
   return 0;
}
