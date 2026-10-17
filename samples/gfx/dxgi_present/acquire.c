#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int VkResult;
typedef uint64_t VkSemaphore;
typedef uint64_t VkFence;
typedef uint64_t VkCommandBuffer;
typedef unsigned VkPipelineStageFlags;
#define VK_SUCCESS 0
#define VK_NOT_READY 1
#define VK_ERROR_OUT_OF_DATE_KHR (-1)
#define VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR 1
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 2
#define VK_PIPELINE_STAGE_ALL_COMMANDS_BIT 1
#define VK_NULL_HANDLE 0
struct fence_values {
   unsigned sType, waitSemaphoreValuesCount, signalSemaphoreValuesCount;
   const void *pNext;
   const uint64_t *pWaitSemaphoreValues, *pSignalSemaphoreValues;
};
typedef struct fence_values VkD3D12FenceSubmitInfoKHR;
struct submit {
   unsigned sType, waitSemaphoreCount, commandBufferCount, signalSemaphoreCount;
   const void *pNext;
   const VkSemaphore *pWaitSemaphores, *pSignalSemaphores;
   const VkPipelineStageFlags *pWaitDstStageMask;
   const VkCommandBuffer *pCommandBuffers;
};
typedef struct submit VkSubmitInfo;
typedef struct presenter {
   void *bridge;
   unsigned pool;
   VkSemaphore done;
   VkCommandBuffer acquire[2];
} ra_vulkan_dxgi_t;
static unsigned next_index, submissions;
static int failed;
static VkResult submit_result;
static unsigned ra_dxgi_bridge_index(void *bridge, unsigned interval)
{ (void)bridge; assert(interval == 0); return next_index; }
static int ra_dxgi_bridge_failed(void *bridge)
{ (void)bridge; return failed; }
static uint64_t ra_dxgi_bridge_last_use(void *bridge, unsigned index)
{ (void)bridge; assert(index < 2); return 7; }
static VkResult ra_vulkan_dxgi_submit(ra_vulkan_dxgi_t *p,
      const VkSubmitInfo *submit, VkFence fence)
{
   const VkD3D12FenceSubmitInfoKHR *values =
      (const VkD3D12FenceSubmitInfoKHR *)submit->pNext;
   assert(submit->waitSemaphoreCount == 1 && submit->pWaitSemaphores == &p->done);
   assert(values->waitSemaphoreValuesCount == 1 && *values->pWaitSemaphoreValues == 7);
   assert(submit->pCommandBuffers == &p->acquire[next_index] && fence == VK_NULL_HANDLE);
   ++submissions;
   return submit_result;
}
#include "acquire.inc"
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 3
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
typedef struct command_info {
   unsigned sType, commandPool, level, commandBufferCount;
   const void *pNext;
} VkCommandBufferAllocateInfo;
static unsigned allocations;
static VkResult allocation_result;
static VkResult vkAllocateCommandBuffers(unsigned device,
      const VkCommandBufferAllocateInfo *info, VkCommandBuffer *buffer)
{
   assert(device == 1 && info->commandPool == 7 && info->commandBufferCount == 1);
   ++allocations;
   if (allocation_result != VK_SUCCESS) return allocation_result;
   *buffer = allocations;
   return VK_SUCCESS;
}
static int record_command(ra_vulkan_dxgi_t *p, unsigned device,
      VkCommandBuffer *buffer)
{
   VkCommandBufferAllocateInfo command;
#define CHECK_VK(expr) do { if ((expr) != VK_SUCCESS) goto error; } while (0)
#include "commands.inc"
#undef CHECK_VK
   return 1;
error:
   return 0;
}
int main(void)
{
   unsigned index = 0, i;
   VkCommandBuffer buffers[4] = {0, 0, 0, 0};
   VkCommandBuffer failed_buffer = VK_NULL_HANDLE;
   ra_vulkan_dxgi_t p;
   memset(&p, 0, sizeof(p));
   next_index = ~0u;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index, 0) == VK_NOT_READY);
   assert(index == 0 && submissions == 0);
   failed = 1;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index, 0) == VK_ERROR_OUT_OF_DATE_KHR);
   assert(index == 0 && submissions == 0);
   failed = 0; next_index = 1;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index, 0) == VK_SUCCESS);
   assert(index == 1 && submissions == 1);
   p.pool = 7;
   for (i = 0; i < 4; i++) assert(record_command(&p, 1, &buffers[i]));
   assert(allocations == 4);
   for (i = 0; i < 4; i++)
   {
      assert(record_command(&p, 1, &buffers[i]));
      assert(buffers[i] == i + 1);
   }
   assert(allocations == 4);
   allocation_result = -2;
   assert(!record_command(&p, 1, &failed_buffer));
   assert(failed_buffer == VK_NULL_HANDLE && allocations == 5);
   puts("PASS: Vulkan/DXGI acquire safety and command buffer reuse");
   return 0;
}
