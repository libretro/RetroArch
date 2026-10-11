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
   VkSemaphore done;
   VkCommandBuffer acquire[2];
} ra_vulkan_dxgi_t;
static unsigned next_index, submissions;
static int failed;
static VkResult submit_result;
static unsigned ra_dxgi_bridge_index(void *bridge)
{ (void)bridge; return next_index; }
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
int main(void)
{
   unsigned index = 0;
   ra_vulkan_dxgi_t p;
   memset(&p, 0, sizeof(p));
   next_index = ~0u;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index) == VK_NOT_READY);
   assert(index == 0 && submissions == 0);
   failed = 1;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index) == VK_ERROR_OUT_OF_DATE_KHR);
   assert(index == 0 && submissions == 0);
   failed = 0; next_index = 1;
   assert(ra_vulkan_dxgi_acquire(&p, 1, VK_NULL_HANDLE, &index) == VK_SUCCESS);
   assert(index == 1 && submissions == 1);
   puts("PASS: Vulkan/DXGI unavailable frames preserve a valid image index");
   return 0;
}
