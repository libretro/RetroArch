/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
/* Reuse the submission operation recorder and its actual lease fault suite. */
#define main submission_fault_suite
#include "vulkan_submit_test.c"
#undef main
#include "../../../gfx/common/vulkan_android_cache_entry.h"
static int entry_calls, entry_fail, entry_live, entry_serial;
static VkResult entry_action(void)
{
   return ++entry_calls == entry_fail ? VK_ERROR_OUT_OF_DEVICE_MEMORY
                                      : VK_SUCCESS;
}
#define CREATE(name, info_type, handle_type) \
   static VKAPI_ATTR VkResult VKAPI_CALL name(VkDevice d, const info_type *p, \
         const VkAllocationCallbacks *a, handle_type *out) \
   { \
      assert(d && p && !a); \
      if (entry_action() != VK_SUCCESS) \
         return VK_ERROR_OUT_OF_DEVICE_MEMORY; \
      *out = H(handle_type, ++entry_serial); \
      entry_live++; \
      return VK_SUCCESS; \
   }
#define DELETE(name, handle_type) \
   static VKAPI_ATTR void VKAPI_CALL name( \
         VkDevice d, handle_type h, const VkAllocationCallbacks *a) \
   { \
      assert(d && h && !a && entry_live); \
      entry_live--; \
   }
CREATE(eimage, VkImageCreateInfo, VkImage)
CREATE(eallocate, VkMemoryAllocateInfo, VkDeviceMemory)
CREATE(
      econversion, VkSamplerYcbcrConversionCreateInfo, VkSamplerYcbcrConversion)
CREATE(esampler, VkSamplerCreateInfo, VkSampler)
CREATE(eview, VkImageViewCreateInfo, VkImageView)
CREATE(eset, VkDescriptorSetLayoutCreateInfo, VkDescriptorSetLayout)
CREATE(elayout, VkPipelineLayoutCreateInfo, VkPipelineLayout)
CREATE(epool, VkDescriptorPoolCreateInfo, VkDescriptorPool)
CREATE(eshader, VkShaderModuleCreateInfo, VkShaderModule)
CREATE(esemaphore, VkSemaphoreCreateInfo, VkSemaphore)
CREATE(efence, VkFenceCreateInfo, VkFence)
DELETE(edimage, VkImage)
DELETE(efree, VkDeviceMemory)
DELETE(edconversion, VkSamplerYcbcrConversion)
DELETE(edsampler, VkSampler)
DELETE(edview, VkImageView)
DELETE(edset, VkDescriptorSetLayout)
DELETE(edlayout, VkPipelineLayout)
DELETE(edpool, VkDescriptorPool)
DELETE(edshader, VkShaderModule)
DELETE(edsemaphore, VkSemaphore)
DELETE(edfence, VkFence)
DELETE(edpipeline, VkPipeline)
static VKAPI_ATTR VkResult VKAPI_CALL eproperties(VkDevice d,
      const struct AHardwareBuffer *b,
      VkAndroidHardwareBufferPropertiesANDROID *p)
{
   VkAndroidHardwareBufferFormatPropertiesANDROID *f
         = (VkAndroidHardwareBufferFormatPropertiesANDROID *)p->pNext;
   assert(d && b && f);
   if (entry_action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   p->allocationSize = 4096;
   p->memoryTypeBits = 1;
   f->externalFormat = 123;
   f->formatFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
         | VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL ebind(
      VkDevice d, VkImage im, VkDeviceMemory m, VkDeviceSize offset)
{
   assert(d && im && m && !offset);
   return entry_action();
}
static VKAPI_ATTR VkResult VKAPI_CALL esets(
      VkDevice d, const VkDescriptorSetAllocateInfo *p, VkDescriptorSet *out)
{
   assert(d && p->descriptorSetCount == 1);
   if (entry_action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = H(VkDescriptorSet, ++entry_serial);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL epipeline(VkDevice d,
      VkPipelineCache cache, uint32_t n, const VkComputePipelineCreateInfo *p,
      const VkAllocationCallbacks *a, VkPipeline *out)
{
   assert(d && !cache && n == 1 && p && !a);
   if (entry_action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = H(VkPipeline, ++entry_serial);
   entry_live++;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL ecommands(
      VkDevice d, const VkCommandBufferAllocateInfo *p, VkCommandBuffer *out)
{
   assert(d && p->commandBufferCount == 1);
   if (entry_action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = H(VkCommandBuffer, ++entry_serial);
   entry_live++;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL efreecommands(
      VkDevice d, VkCommandPool p, uint32_t n, const VkCommandBuffer *c)
{
   assert(d && p && n == 1 && c && entry_live);
   entry_live--;
}
static vulkan_android_entry_config_t entry_fixture_config;
#ifndef VULKAN_ENTRY_TEST_MAIN
#define VULKAN_ENTRY_TEST_MAIN main
#endif
int VULKAN_ENTRY_TEST_MAIN(void)
{
   static vulkan_android_entry_config_t c;
   static vulkan_android_cache_entry_t cache, empty;
   static VkPhysicalDeviceMemoryProperties m;
   android_frame_view_t v;
   int i;
   assert(!submission_fault_suite());
   memset(&v, 0, sizeof(v));
   v.image.buffer = &native;
   v.image.width = v.image.right = v.image.buffer_width = 64;
   v.image.height = v.image.bottom = v.image.buffer_height = 32;
   v.color.primaries = v.color.matrix = v.color.range = 1;
   v.color.transfer = 13;
   setup(&v);
   c.device = entry.device;
   c.queue = entry.queue;
   c.command_pool = H(VkCommandPool, 1);
   c.submit = entry.ops;
   m.memoryTypeCount = 1;
   c.memory = &m;
   c.sampler_descriptors = 3;
   c.entry.allocate_commands = ecommands;
   c.entry.free_commands = efreecommands;
   c.entry.create_fence = efence;
   c.entry.destroy_fence = edfence;
   c.image.properties = eproperties;
   c.image.create_image = eimage;
   c.image.destroy_image = edimage;
   c.image.allocate = eallocate;
   c.image.free_memory = efree;
   c.image.bind = ebind;
   c.image.create_conversion = econversion;
   c.image.destroy_conversion = edconversion;
   c.image.create_sampler = esampler;
   c.image.destroy_sampler = edsampler;
   c.image.create_view = eview;
   c.image.destroy_view = edview;
   c.pipeline.create_set_layout = eset;
   c.pipeline.destroy_set_layout = edset;
   c.pipeline.create_layout = elayout;
   c.pipeline.destroy_layout = edlayout;
   c.pipeline.create_pool = epool;
   c.pipeline.destroy_pool = edpool;
   c.pipeline.allocate_sets = esets;
   c.pipeline.update_sets = update;
   c.pipeline.create_shader = eshader;
   c.pipeline.destroy_shader = edshader;
   c.pipeline.create_pipeline = epipeline;
   c.pipeline.destroy_pipeline = edpipeline;
   c.sync.create = esemaphore;
   c.sync.destroy = edsemaphore;
   c.sync.import_fd = import_fd;
   c.sync.export_fd = export_fd;
   entry_fixture_config = c;
   for (i = 1; i <= 17; i++)
   {
      entry_fail = i;
      entry_calls = 0;
      assert(!vulkan_android_cache_entry_create(&c, &v, false, &cache));
      assert(!entry_live && !memcmp(&cache, &empty, sizeof(cache)));
   }
   entry_fail = 0;
   entry_calls = 0;
   assert(vulkan_android_cache_entry_create(&c, &v, true, &cache));
   assert(entry_calls == 17 && entry_live);
   cache.submission.inflight = 1;
   fence_status = VK_NOT_READY;
   assert(!vulkan_android_cache_entry_destroy(&cache) && entry_live);
   fence_status = VK_SUCCESS;
   assert(vulkan_android_cache_entry_destroy(&cache));
   assert(!entry_live);
   puts("PASS: cold Android Vulkan entry all 17 startup failures unwind and "
        "pending GPU work retains resources");
   return 0;
}
