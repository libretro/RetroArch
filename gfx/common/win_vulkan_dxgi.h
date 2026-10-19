#ifndef RA_VULKAN_DXGI_H
#define RA_VULKAN_DXGI_H
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan_symbol_wrapper.h>
typedef struct ra_vulkan_dxgi ra_vulkan_dxgi_t;
ra_vulkan_dxgi_t *ra_vulkan_dxgi_create(VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, VkQueue queue, unsigned family,
      HWND window, unsigned width, unsigned height, VkFormat format, void *queue_lock);
int ra_vulkan_dxgi_resize(ra_vulkan_dxgi_t *presenter,
      unsigned width, unsigned height, VkFormat format);
int ra_vulkan_dxgi_presentable(ra_vulkan_dxgi_t *presenter);
void ra_vulkan_dxgi_destroy(ra_vulkan_dxgi_t *presenter);
void ra_vulkan_dxgi_hdr_metadata(ra_vulkan_dxgi_t *presenter, float peak);
int64_t ra_vulkan_dxgi_present_time(ra_vulkan_dxgi_t *presenter);
/* The ID3D12Device the presenter presents with, for NVAPI */
void *ra_vulkan_dxgi_device(ra_vulkan_dxgi_t *presenter);
VkImage ra_vulkan_dxgi_image(ra_vulkan_dxgi_t *presenter, unsigned index);
VkResult ra_vulkan_dxgi_acquire(ra_vulkan_dxgi_t *presenter,
      VkSemaphore semaphore, VkFence fence, unsigned *index, unsigned interval);
VkResult ra_vulkan_dxgi_present(ra_vulkan_dxgi_t *presenter, unsigned index,
      unsigned count, const VkSemaphore *wait, unsigned interval);
#endif
