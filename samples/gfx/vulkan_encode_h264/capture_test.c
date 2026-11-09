/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Capture acceptance requires assertions
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/vulkan_encode_capture.h"
static unsigned allocations;
static bool fail_allocation;
static void *test_calloc(size_t count, size_t bytes)
{
   allocations++;
   return fail_allocation ? NULL : calloc(count, bytes);
}
#define calloc test_calloc
#include "../../../gfx/common/vulkan_encode_capture.c"
#undef calloc
#define HANDLE(type, n) ((type)(uintptr_t)(n))
static vulkan_encode_pool_t pool;
static vulkan_encode_capture_ops_t ops;
static unsigned calls, fail_at, live, barriers, copies, binds, pushes,
    dispatches, updates;
static bool direct, initialized;
static VkImageLayout source_layout;
static VkResult startup(unsigned bit, uintptr_t *handle)
{
   calls++;
   if (calls == fail_at)
   {
      *handle = 999;
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   }
   assert(!(live & bit));
   live |= bit;
   *handle = bit;
   return VK_SUCCESS;
}
static void destroy(unsigned bit, uintptr_t handle)
{
   assert(handle == bit && (live & bit));
   live &= ~bit;
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_set(VkDevice d, const VkDescriptorSetLayoutCreateInfo *p,
           const VkAllocationCallbacks *a, VkDescriptorSetLayout *out)
{
   uintptr_t h;
   unsigned i;
   VkResult r;
   assert(d == pool.device && !a &&
          p->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO &&
          !p->pNext && !p->flags);
   assert(p->bindingCount == (direct ? 3u : 2u));
   for (i = 0; i < p->bindingCount; i++)
   {
      const VkDescriptorSetLayoutBinding *b = &p->pBindings[i];
      assert(b->binding == i && b->descriptorCount == 1 &&
             b->stageFlags == VK_SHADER_STAGE_COMPUTE_BIT &&
             !b->pImmutableSamplers);
      assert(b->descriptorType ==
             (!i       ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
              : direct ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                       : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
   }
   r = startup(1, &h);
   *out = HANDLE(VkDescriptorSetLayout, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL destroy_set(VkDevice d,
                                              VkDescriptorSetLayout h,
                                              const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(1, (uintptr_t)h);
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_pool(VkDevice d, const VkDescriptorPoolCreateInfo *p,
            const VkAllocationCallbacks *a, VkDescriptorPool *out)
{
   uintptr_t h;
   VkResult r;
   assert(d == pool.device && !a &&
          p->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO &&
          !p->pNext && !p->flags);
   assert(p->maxSets == pool.count && p->poolSizeCount == 2 &&
          p->pPoolSizes[0].descriptorCount == pool.count &&
          p->pPoolSizes[1].descriptorCount == pool.count * (direct ? 2u : 1u));
   r = startup(2, &h);
   *out = HANDLE(VkDescriptorPool, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL destroy_pool(VkDevice d, VkDescriptorPool h,
                                               const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(2, (uintptr_t)h);
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate_sets(
    VkDevice d, const VkDescriptorSetAllocateInfo *p, VkDescriptorSet *out)
{
   unsigned i;
   assert(d == pool.device &&
          p->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO &&
          !p->pNext && p->descriptorPool == HANDLE(VkDescriptorPool, 2) &&
          p->descriptorSetCount == pool.count);
   for (i = 0; i < pool.count; i++)
   {
      assert(p->pSetLayouts[i] == HANDLE(VkDescriptorSetLayout, 1));
      out[i] = HANDLE(VkDescriptorSet, 100 + i);
   }
   calls++;
   return calls == fail_at ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_sampler(VkDevice d, const VkSamplerCreateInfo *p,
               const VkAllocationCallbacks *a, VkSampler *out)
{
   uintptr_t h;
   VkResult r;
   assert(d == pool.device && !a &&
          p->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO && !p->pNext &&
          !p->flags);
   assert(p->magFilter == VK_FILTER_NEAREST &&
          p->minFilter == VK_FILTER_NEAREST &&
          p->mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST);
   assert(!p->anisotropyEnable && !p->compareEnable &&
          !p->unnormalizedCoordinates && !p->mipLodBias && !p->minLod &&
          !p->maxLod);
   assert(p->addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE &&
          p->addressModeV == p->addressModeU &&
          p->addressModeW == p->addressModeU);
   r = startup(4, &h);
   *out = HANDLE(VkSampler, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL
destroy_sampler(VkDevice d, VkSampler h, const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(4, (uintptr_t)h);
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_layout(VkDevice d, const VkPipelineLayoutCreateInfo *p,
              const VkAllocationCallbacks *a, VkPipelineLayout *out)
{
   uintptr_t h;
   VkResult r;
   assert(d == pool.device && !a &&
          p->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO &&
          !p->pNext && !p->flags);
   assert(p->setLayoutCount == 1 &&
          p->pSetLayouts[0] == HANDLE(VkDescriptorSetLayout, 1) &&
          p->pushConstantRangeCount == 1);
   assert(p->pPushConstantRanges[0].size == 32 &&
          !p->pPushConstantRanges[0].offset &&
          p->pPushConstantRanges[0].stageFlags == VK_SHADER_STAGE_COMPUTE_BIT);
   r = startup(8, &h);
   *out = HANDLE(VkPipelineLayout, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL destroy_layout(VkDevice d, VkPipelineLayout h,
                                                 const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(8, (uintptr_t)h);
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_shader(VkDevice d, const VkShaderModuleCreateInfo *p,
              const VkAllocationCallbacks *a, VkShaderModule *out)
{
   uintptr_t h;
   VkResult r;
   assert(d == pool.device && !a &&
          p->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO &&
          !p->pNext && !p->flags && p->codeSize == 20 &&
          p->pCode[0] == 0x07230203);
   r = startup(16, &h);
   *out = HANDLE(VkShaderModule, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL destroy_shader(VkDevice d, VkShaderModule h,
                                                 const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(16, (uintptr_t)h);
}
static VKAPI_ATTR VkResult VKAPI_CALL
create_pipeline(VkDevice d, VkPipelineCache cache, uint32_t count,
                const VkComputePipelineCreateInfo *p,
                const VkAllocationCallbacks *a, VkPipeline *out)
{
   uintptr_t h;
   VkResult r;
   assert(d == pool.device && !cache && count == 1 && !a &&
          p->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO &&
          !p->pNext && !p->flags);
   assert(p->layout == HANDLE(VkPipelineLayout, 8) && !p->basePipelineHandle &&
          p->basePipelineIndex == -1);
   assert(p->stage.sType ==
              VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO &&
          !p->stage.pNext && !p->stage.flags &&
          p->stage.stage == VK_SHADER_STAGE_COMPUTE_BIT &&
          p->stage.module == HANDLE(VkShaderModule, 16) &&
          !strcmp(p->stage.pName, "main") && !p->stage.pSpecializationInfo);
   r = startup(32, &h);
   if (r != VK_SUCCESS)
   {
      assert(!(live & 32));
      live |= 32;
      h = 32;
   }
   *out = HANDLE(VkPipeline, h);
   return r;
}
static VKAPI_ATTR void VKAPI_CALL
destroy_pipeline(VkDevice d, VkPipeline h, const VkAllocationCallbacks *a)
{
   assert(d == pool.device && !a);
   destroy(32, (uintptr_t)h);
}
static VKAPI_ATTR void VKAPI_CALL update_sets(VkDevice d, uint32_t count,
                                              const VkWriteDescriptorSet *p,
                                              uint32_t copies_count,
                                              const VkCopyDescriptorSet *copy)
{
   unsigned i;
   assert(d == pool.device && count == (direct ? 3u : 2u) && !copies_count &&
          !copy);
   for (i = 0; i < count; i++)
   {
      assert(p[i].sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET &&
             !p[i].pNext && p[i].dstSet == HANDLE(VkDescriptorSet, 115) &&
             p[i].dstBinding == i && !p[i].dstArrayElement &&
             p[i].descriptorCount == 1 && !p[i].pTexelBufferView);
      if (!i || direct)
      {
         assert(p[i].pImageInfo && !p[i].pBufferInfo);
         assert(p[i].pImageInfo->imageView ==
                (!i ? HANDLE(VkImageView, 600) : pool.slot[15].planes[i - 1]));
         assert(p[i].pImageInfo->sampler ==
                (!i ? HANDLE(VkSampler, 4) : VK_NULL_HANDLE));
         assert(p[i].pImageInfo->imageLayout ==
                (!i && source_layout != VK_IMAGE_LAYOUT_GENERAL
                     ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                     : VK_IMAGE_LAYOUT_GENERAL));
      }
      else
      {
         assert(!p[i].pImageInfo && p[i].pBufferInfo &&
                p[i].pBufferInfo->buffer == pool.slot[15].conversion.buffer &&
                !p[i].pBufferInfo->offset &&
                p[i].pBufferInfo->range == pool.slot[15].conversion.size);
      }
   }
   updates++;
}
static VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer cmd,
                                          const VkDependencyInfo *p)
{
   assert(cmd == HANDLE(VkCommandBuffer, 500) &&
          p->sType == VK_STRUCTURE_TYPE_DEPENDENCY_INFO && !p->pNext &&
          !p->dependencyFlags && !p->memoryBarrierCount && !p->pMemoryBarriers);
   if (barriers == 0 || barriers == (direct ? 1u : 2u))
   {
      const VkImageMemoryBarrier2 *b = p->pImageMemoryBarriers;
      unsigned i;
      assert(p->imageMemoryBarrierCount == 2 && !p->bufferMemoryBarrierCount &&
             !p->pBufferMemoryBarriers);
      for (i = 0; i < 2; i++)
         assert(b[i].sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 &&
                !b[i].pNext &&
                b[i].srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                b[i].dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
                b[i].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT &&
                !b[i].subresourceRange.baseMipLevel &&
                b[i].subresourceRange.levelCount == 1 &&
                !b[i].subresourceRange.baseArrayLayer &&
                b[i].subresourceRange.layerCount == 1);
      assert(b[0].image == HANDLE(VkImage, 599) &&
             b[1].image == pool.slot[15].image);
      if (!barriers)
      {
         assert(b[0].oldLayout == source_layout &&
                b[0].dstStageMask == VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT &&
                b[0].dstAccessMask == VK_ACCESS_2_SHADER_READ_BIT);
         assert(b[1].oldLayout == (initialized ? VK_IMAGE_LAYOUT_GENERAL
                                               : VK_IMAGE_LAYOUT_UNDEFINED));
         assert(b[1].newLayout == (direct
                                       ? VK_IMAGE_LAYOUT_GENERAL
                                       : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
      }
      else
         assert(b[0].newLayout == source_layout &&
                b[1].newLayout == VK_IMAGE_LAYOUT_GENERAL &&
                b[1].dstStageMask == VK_PIPELINE_STAGE_2_NONE &&
                !b[1].dstAccessMask);
   }
   else
   {
      const VkBufferMemoryBarrier2 *b = p->pBufferMemoryBarriers;
      assert(!direct && barriers == 1 && !p->imageMemoryBarrierCount &&
             p->bufferMemoryBarrierCount == 1);
      assert(b->sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 &&
             !b->pNext &&
             b->srcStageMask == VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT &&
             b->srcAccessMask == VK_ACCESS_2_SHADER_WRITE_BIT &&
             b->dstStageMask == VK_PIPELINE_STAGE_2_TRANSFER_BIT &&
             b->dstAccessMask == VK_ACCESS_2_TRANSFER_READ_BIT &&
             b->buffer == pool.slot[15].conversion.buffer && !b->offset &&
             b->size == pool.slot[15].conversion.size);
   }
   barriers++;
}
static VKAPI_ATTR void VKAPI_CALL bind_pipeline(VkCommandBuffer cmd,
                                                VkPipelineBindPoint point,
                                                VkPipeline p)
{
   assert(cmd == HANDLE(VkCommandBuffer, 500) &&
          point == VK_PIPELINE_BIND_POINT_COMPUTE &&
          p == HANDLE(VkPipeline, 32) && barriers == 1);
   binds++;
}
static VKAPI_ATTR void VKAPI_CALL bind_sets(
    VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipelineLayout layout,
    uint32_t first, uint32_t count, const VkDescriptorSet *sets,
    uint32_t dynamic, const uint32_t *offsets)
{
   assert(cmd == HANDLE(VkCommandBuffer, 500) &&
          point == VK_PIPELINE_BIND_POINT_COMPUTE &&
          layout == HANDLE(VkPipelineLayout, 8) && !first && count == 1 &&
          sets[0] == HANDLE(VkDescriptorSet, 115) && !dynamic && !offsets);
   binds++;
}
static VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer cmd,
                                       VkPipelineLayout layout,
                                       VkShaderStageFlags flags,
                                       uint32_t offset, uint32_t size,
                                       const void *data)
{
   const uint32_t *p = (const uint32_t *)data;
   assert(cmd == HANDLE(VkCommandBuffer, 500) &&
          layout == HANDLE(VkPipelineLayout, 8) &&
          flags == VK_SHADER_STAGE_COMPUTE_BIT && !offset && size == 32);
   assert(p[0] == 192 && p[1] == 128 && p[2] == 3 && p[3] == 2 && p[4] == 185 &&
          p[5] == 123 && p[6] == 1 && p[7] == 1);
   pushes++;
}
static VKAPI_ATTR void VKAPI_CALL dispatch(VkCommandBuffer cmd, uint32_t x,
                                           uint32_t y, uint32_t z)
{
   assert(cmd == HANDLE(VkCommandBuffer, 500) && x == (direct ? 12u : 6u) &&
          y == 8 && z == 1 && pushes == 1 && binds == 2);
   dispatches++;
}
static VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer cmd, VkBuffer buffer,
                                       VkImage image, VkImageLayout layout,
                                       uint32_t count,
                                       const VkBufferImageCopy *p)
{
   unsigned i;
   assert(!direct && cmd == HANDLE(VkCommandBuffer, 500) &&
          buffer == pool.slot[15].conversion.buffer &&
          image == pool.slot[15].image &&
          layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && count == 2 &&
          barriers == 2 && dispatches == 1);
   for (i = 0; i < 2; i++)
      assert(
          p[i].bufferOffset == (i ? 192u * 128u : 0u) &&
          !p[i].bufferRowLength && !p[i].bufferImageHeight &&
          p[i].imageSubresource.aspectMask ==
              (i ? VK_IMAGE_ASPECT_PLANE_1_BIT : VK_IMAGE_ASPECT_PLANE_0_BIT) &&
          !p[i].imageSubresource.mipLevel &&
          !p[i].imageSubresource.baseArrayLayer &&
          p[i].imageSubresource.layerCount == 1 && !p[i].imageOffset.x &&
          !p[i].imageOffset.y && !p[i].imageOffset.z &&
          p[i].imageExtent.width == (192u >> i) &&
          p[i].imageExtent.height == (128u >> i) &&
          p[i].imageExtent.depth == 1);
   copies++;
}
static void setup(bool storage)
{
   unsigned i;
   assert(!live);
   memset(&pool, 0, sizeof(pool));
   memset(&ops, 0, sizeof(ops));
   direct = storage;
   pool.device = HANDLE(VkDevice, 1);
   pool.count = 16;
   pool.extent.width = 192;
   pool.extent.height = 128;
   pool.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   pool.storage_images = storage;
   for (i = 0; i < 16; i++)
   {
      pool.slot[i].image = HANDLE(VkImage, 200 + i);
      pool.slot[i].planes[0] = HANDLE(VkImageView, 300 + i);
      pool.slot[i].planes[1] = HANDLE(VkImageView, 400 + i);
      pool.slot[i].conversion.buffer = HANDLE(VkBuffer, 700 + i);
      pool.slot[i].conversion.size = 192 * 128 * 3 / 2;
   }
   ops.create_set_layout = create_set;
   ops.destroy_set_layout = destroy_set;
   ops.create_descriptor_pool = create_pool;
   ops.destroy_descriptor_pool = destroy_pool;
   ops.allocate_sets = allocate_sets;
   ops.update_sets = update_sets;
   ops.create_sampler = create_sampler;
   ops.destroy_sampler = destroy_sampler;
   ops.create_pipeline_layout = create_layout;
   ops.destroy_pipeline_layout = destroy_layout;
   ops.create_shader = create_shader;
   ops.destroy_shader = destroy_shader;
   ops.create_pipelines = create_pipeline;
   ops.destroy_pipeline = destroy_pipeline;
   ops.barrier = barrier;
   ops.bind_pipeline = bind_pipeline;
   ops.bind_sets = bind_sets;
   ops.push = push;
   ops.dispatch = dispatch;
   ops.copy = copy;
   calls = fail_at = 0;
}
int main(void)
{
   static const uint32_t code[5] = {0x07230203, 0, 0, 0, 0};
   vulkan_encode_capture_config_t config;
   vulkan_encode_capture_source_t source, bad;
   vulkan_encode_capture_ops_t missing;
   vulkan_encode_capture_t *c;
   unsigned mode, fault, layout, iteration, allocated, missing_count = 0;
   config.pool = &pool;
   config.code = code;
   config.code_bytes = sizeof(code);
   for (mode = 0; mode < 2; mode++)
   {
      setup(mode != 0);
      fail_allocation = true;
      assert(!vulkan_encode_capture_new(&config, &ops) && !calls && !live);
      fail_allocation = false;
      for (fault = 1; fault <= 7; fault++)
      {
         calls = 0;
         fail_at = fault;
         assert(!vulkan_encode_capture_new(&config, &ops) && !live);
      }
      fail_at = 0;
#define MISSING(field)                                                         \
   do                                                                          \
   {                                                                           \
      missing = ops;                                                           \
      missing.field = NULL;                                                    \
      calls = 0;                                                               \
      assert(!vulkan_encode_capture_new(&config, &missing) && !calls &&        \
             !live);                                                           \
      missing_count++;                                                         \
   } while (0)
      MISSING(create_set_layout);
      MISSING(destroy_set_layout);
      MISSING(create_descriptor_pool);
      MISSING(destroy_descriptor_pool);
      MISSING(allocate_sets);
      MISSING(update_sets);
      MISSING(create_sampler);
      MISSING(destroy_sampler);
      MISSING(create_pipeline_layout);
      MISSING(destroy_pipeline_layout);
      MISSING(create_shader);
      MISSING(destroy_shader);
      MISSING(create_pipelines);
      MISSING(destroy_pipeline);
      MISSING(barrier);
      MISSING(bind_pipeline);
      MISSING(bind_sets);
      MISSING(push);
      MISSING(dispatch);
      if (!direct)
         MISSING(copy);
#undef MISSING
      calls = 0;
      c = vulkan_encode_capture_new(&config, &ops);
      assert(c && calls == 7 && live == 47);
      allocated = allocations;
      memset(&source, 0, sizeof(source));
      source.image = HANDLE(VkImage, 599);
      source.view = HANDLE(VkImageView, 600);
      source.format = VK_FORMAT_B8G8R8A8_UNORM;
      source.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      source.access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      source.width = 256;
      source.height = 192;
      source.crop_x = 3;
      source.crop_y = 2;
      source.valid_width = 185;
      source.valid_height = 123;
      source.limited_range = source.flip_y = true;
      for (layout = 0; layout < 3; layout++)
         for (iteration = 0; iteration < 2; iteration++)
         {
            source_layout =
                layout == 0   ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : layout == 1 ? VK_IMAGE_LAYOUT_GENERAL
                              : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            source.layout = source_layout;
            initialized = iteration != 0;
            barriers = copies = binds = pushes = dispatches = updates = 0;
            assert(vulkan_encode_capture_record(
                c, 15, HANDLE(VkCommandBuffer, 500), &source, initialized));
            assert(barriers == (direct ? 2u : 3u) &&
                   copies == (direct ? 0u : 1u) && binds == 2 && pushes == 1 &&
                   dispatches == 1 && updates == 1 && calls == 7 && allocations == allocated);
         }
#define BAD(field, value)                                                      \
   do                                                                          \
   {                                                                           \
      bad = source;                                                            \
      bad.field = value;                                                       \
      barriers = updates = 0;                                                  \
      assert(!vulkan_encode_capture_record(                                    \
                 c, 15, HANDLE(VkCommandBuffer, 500), &bad, false) &&          \
             !barriers && !updates);                                           \
   } while (0)
      BAD(image, VK_NULL_HANDLE);
      BAD(view, VK_NULL_HANDLE);
      BAD(stage, 0);
      BAD(image, pool.slot[15].image);
      BAD(format, VK_FORMAT_B8G8R8A8_SRGB);
      BAD(format, VK_FORMAT_R16G16B16A16_SFLOAT);
      BAD(layout, VK_IMAGE_LAYOUT_UNDEFINED);
      BAD(width, 0);
      BAD(height, 0);
      BAD(width, UINT32_MAX);
      BAD(height, UINT32_MAX);
      BAD(crop_x, UINT32_MAX);
      BAD(crop_y, UINT32_MAX);
      BAD(valid_width, 0);
      BAD(valid_height, 0);
      BAD(valid_width, 193);
      BAD(valid_height, 129);
      BAD(width, 187);
      BAD(height, 124);
#undef BAD
      assert(!vulkan_encode_capture_record(c, 16, HANDLE(VkCommandBuffer, 500),
                                           &source, false));
      assert(
          !vulkan_encode_capture_record(c, 15, VK_NULL_HANDLE, &source, false));
      assert(!vulkan_encode_capture_record(c, 15, HANDLE(VkCommandBuffer, 500),
                                           NULL, false));
      vulkan_encode_capture_free(c);
      assert(!live);
      config.code_bytes = 19;
      assert(!vulkan_encode_capture_new(&config, &ops));
      config.code_bytes = sizeof(code);
      pool.count = 17;
      assert(!vulkan_encode_capture_new(&config, &ops));
      pool.count = 16;
      pool.extent.width = 193;
      assert(!vulkan_encode_capture_new(&config, &ops));
      pool.extent.width = 192;
      if (!direct)
      {
         pool.slot[15].conversion.size--;
         assert(!vulkan_encode_capture_new(&config, &ops));
      }
   }
   assert(missing_count == 39);
   vulkan_encode_capture_free(NULL);
   puts("Vulkan capture direct/fallback barriers, descriptors, bounds and "
        "startup retirement PASS");
   return 0;
}
