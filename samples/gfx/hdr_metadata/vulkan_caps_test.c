#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <formats/image.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define VK_CTX_FLAG_HDR_SUPPORT 1u
#define VK_CTX_FLAG_HDR_ENABLE 2u
#define VK_CTX_FLAG_HDR_SCRGB 4u

typedef struct
{
   VkPhysicalDevice gpu;
   VkFormat swapchain_format;
   uint32_t flags;
} test_context_t;
typedef struct
{
   test_context_t *context;
   VkFormat built_format;
   uint32_t built_hdr_flags;
   retro_atomic_int_t built_hdr_output;
   retro_atomic_int_t texture_format_support;
   bool built;
   struct { VkPipeline yuv420_to_rgba8888; } pipelines;
   struct { VkPipeline pipeline_linear_sdr; } display;
} vk_t;

static unsigned property_queries;
static void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice gpu,
      VkFormat format, VkFormatProperties *properties)
{
   (void)gpu;
   property_queries++;
   memset(properties, 0, sizeof(*properties));
   if (format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
      properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
}

#include "vulkan.inc"

static void rebuild(void *data)
{
   vk_t *vk = (vk_t*)data;
   unsigned i;
   for (i = 0; i < 50000; i++)
   {
      vk->context->flags = (i & 1) ? VK_CTX_FLAG_HDR_ENABLE : 0;
      vk->pipelines.yuv420_to_rgba8888 = (i & 2)
         ? (VkPipeline)(uintptr_t)1 : VK_NULL_HANDLE;
      vk->display.pipeline_linear_sdr = (i & 4)
         ? (VkPipeline)(uintptr_t)1 : VK_NULL_HANDLE;
      vulkan_note_built(vk);
   }
}

int main(void)
{
   test_context_t context;
   vk_t vk;
   sthread_t *worker;
   unsigned i;
   volatile unsigned observed = 0;

   memset(&vk, 0, sizeof(vk));
   memset(&context, 0, sizeof(context));
   vk.context = &context;
   context.gpu = (VkPhysicalDevice)(uintptr_t)1;
   context.swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
   vulkan_note_built(&vk);
   CHECK(!vulkan_supports_texture_format(NULL, TEXTURE_GPU_FORMAT_SCRGB));
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_YUV420));
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB));
   vk.pipelines.yuv420_to_rgba8888 = (VkPipeline)(uintptr_t)1;
   vk.display.pipeline_linear_sdr = (VkPipeline)(uintptr_t)1;
   vulkan_note_built(&vk);
   CHECK(vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_YUV420));
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB));
   context.flags = VK_CTX_FLAG_HDR_ENABLE;
   vulkan_note_built(&vk);
#ifdef VULKAN_HDR_SWAPCHAIN
   CHECK(vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB));
#else
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB));
#endif
   vk.display.pipeline_linear_sdr = VK_NULL_HANDLE;
   vk.pipelines.yuv420_to_rgba8888 = VK_NULL_HANDLE;
   vulkan_note_built(&vk);
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB));
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_YUV420));
   CHECK(vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_RGBA16F));
   CHECK(vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_RGB10A2));
   CHECK(property_queries == 0);

   worker = sthread_create(rebuild, &vk);
   CHECK(worker);
   for (i = 0; i < 200000; i++)
   {
      observed += vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_SCRGB);
      observed += vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_YUV420);
   }
   sthread_join(worker);
   CHECK(property_queries == 0);
   CHECK(vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_BC1));
   CHECK(!vulkan_supports_texture_format(&vk, TEXTURE_GPU_FORMAT_BC2));
   CHECK(!vulkan_supports_texture_format(&vk, (enum texture_gpu_format)999));
   CHECK(property_queries == 2);
   printf("PASS: published Vulkan format capabilities; %u concurrent observations\n",
         (unsigned)observed);
   return 0;
}
