/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../../gfx/common/vulkan_encode_pool.h"
static unsigned allocations, fail_allocation;
static void *test_calloc(size_t count, size_t size)
{
   return ++allocations == fail_allocation ? NULL : calloc(count, size);
}
#define calloc test_calloc
#include "../../../gfx/common/vulkan_encode_pool.c"
#undef calloc
#define HANDLE(type,id) ((type)(uintptr_t)(id))
#define ID(handle) ((unsigned)(uintptr_t)(handle))
enum { IMAGE=1, VIEW, BUFFER, MEMORY, COMMAND_POOL, COMMAND, FENCE, QUERY_POOL, SEMAPHORE };
struct object
{
   VkDeviceSize bytes;
   unsigned type, parent, alive, mapped, input, storage, host;
};
static struct object objects[4096];
static unsigned used, live, calls, fail_call, memory_mode, format_mode;
static VkVideoProfileInfoKHR profile;
static VkVideoEncodeH264ProfileInfoKHR codec;
static VkVideoCapabilitiesKHR capabilities;
static vulkan_encode_pool_config_t config;
static VkResult result(void)
{
   return ++calls == fail_call ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static uintptr_t make(unsigned type, unsigned parent, VkDeviceSize bytes, VkResult *status)
{
   *status = result();
   if (*status != VK_SUCCESS)
      return 99999;
   assert(++used < 4096);
   objects[used].type = type; objects[used].parent = parent;
   objects[used].bytes = bytes; objects[used].alive = 1; live++;
   return used;
}
static struct object *object(unsigned id, unsigned type)
{
   assert(id && id <= used && objects[id].alive && objects[id].type == type);
   return &objects[id];
}
static void destroy(unsigned id, unsigned type)
{
   object(id,type)->alive = 0;
   assert(live); live--;
}
static void device(VkDevice value)
{
   assert(value == HANDLE(VkDevice,9001));
}
static VKAPI_ATTR void VKAPI_CALL memory_properties(VkPhysicalDevice physical,
      VkPhysicalDeviceMemoryProperties *properties)
{
   assert(physical == config.physical);
   memset(properties,0,sizeof(*properties));
   properties->memoryTypeCount = memory_mode == 4 ? 33 : 5;
   properties->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   properties->memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
         (memory_mode == 3 ? 0 : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   properties->memoryTypes[3].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
         (memory_mode == 3 ? 0 : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) |
         (memory_mode == 1 ? 0 : VK_MEMORY_PROPERTY_HOST_CACHED_BIT) |
         (memory_mode == 2 ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0);
   properties->memoryTypes[4].propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT;
}
static VKAPI_ATTR VkResult VKAPI_CALL video_formats(VkPhysicalDevice physical,
      const VkPhysicalDeviceVideoFormatInfoKHR *info, uint32_t *count,
      VkVideoFormatPropertiesKHR *formats)
{
   const VkVideoProfileListInfoKHR *profiles = (const VkVideoProfileListInfoKHR*)info->pNext;
   VkResult status = result();
   assert(physical == config.physical && profiles->pProfiles == &profile && profiles->profileCount == 1);
   if (status != VK_SUCCESS) return status;
   if (!formats) *count = 1;
   else
   {
      assert(*count == 1);
      formats->format = format_mode == 3 ? VK_FORMAT_R8_UNORM : config.format;
      formats->imageType = VK_IMAGE_TYPE_2D; formats->imageTiling = VK_IMAGE_TILING_OPTIMAL;
      formats->imageUsageFlags = info->imageUsage;
      formats->imageCreateFlags = format_mode == 1 ? 0 : VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
   }
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL image_properties(VkPhysicalDevice physical,
      const VkPhysicalDeviceImageFormatInfo2 *info, VkImageFormatProperties2 *properties)
{
   VkResult status = result();
   assert(physical == config.physical && info->format == config.format && info->pNext);
   if (status != VK_SUCCESS) return status;
   properties->imageFormatProperties.maxExtent.width = format_mode == 4 ? 16 : 8192;
   properties->imageFormatProperties.maxExtent.height = 8192;
   properties->imageFormatProperties.maxExtent.depth = 1;
   properties->imageFormatProperties.maxMipLevels = properties->imageFormatProperties.maxArrayLayers = 1;
   properties->imageFormatProperties.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL format_properties(VkPhysicalDevice physical,
      VkFormat format, VkFormatProperties2 *properties)
{
   assert(physical == config.physical && (format == VK_FORMAT_R8_UNORM || format == VK_FORMAT_R8G8_UNORM));
   properties->formatProperties.optimalTilingFeatures = format_mode == 2 ? 0 : VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_image(VkDevice d,
      const VkImageCreateInfo *info, const VkAllocationCallbacks *allocator, VkImage *image)
{
   VkResult status;
   uintptr_t id;
   device(d); assert(!allocator && info->format == config.format && info->pNext);
   assert(info->extent.width == config.extent.width && info->extent.height == config.extent.height);
   assert(info->initialLayout == VK_IMAGE_LAYOUT_UNDEFINED);
   id = make(IMAGE,0,(VkDeviceSize)info->extent.width * info->extent.height * 3 / 2,&status);
   *image = HANDLE(VkImage,id);
   if (status == VK_SUCCESS)
   {
      objects[id].input = !!(info->usage & VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR);
      objects[id].storage = !!(info->usage & VK_IMAGE_USAGE_STORAGE_BIT);
      if (objects[id].input)
      {
         assert(info->sharingMode == VK_SHARING_MODE_CONCURRENT && info->queueFamilyIndexCount == 2);
         assert(info->pQueueFamilyIndices[0] == config.encode_family && info->pQueueFamilyIndices[1] == config.producer_family);
         assert(objects[id].storage ?
               info->flags == (VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) :
               !!(info->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT));
      }
      else assert(info->usage == VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR && info->sharingMode == VK_SHARING_MODE_EXCLUSIVE);
   }
   return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_image(VkDevice d, VkImage image,
      const VkAllocationCallbacks *allocator)
{
   unsigned i;
   device(d); assert(!allocator);
   for (i=1;i<=used;i++) assert(!objects[i].alive || objects[i].type != VIEW || objects[i].parent != ID(image));
   destroy(ID(image),IMAGE);
}
static VKAPI_ATTR void VKAPI_CALL image_memory(VkDevice d, VkImage image, VkMemoryRequirements *requirements)
{
   device(d); requirements->size = (object(ID(image),IMAGE)->bytes + 4095) / 4096 * 4096;
   requirements->alignment = 256; requirements->memoryTypeBits = 2;
}
static VKAPI_ATTR VkResult VKAPI_CALL bind_image(VkDevice d, VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
{
   device(d); object(ID(image),IMAGE); assert(!offset && object(ID(memory),MEMORY)->parent == ID(image));
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL create_view(VkDevice d,
      const VkImageViewCreateInfo *info, const VkAllocationCallbacks *allocator, VkImageView *view)
{
   const VkImageViewUsageCreateInfo *usage = (const VkImageViewUsageCreateInfo*)info->pNext;
   struct object *image;
   VkResult status;
   device(d); assert(!allocator && usage); image = object(ID(info->image),IMAGE);
   if (info->subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
      assert(info->format == config.format && usage->usage == (image->input ?
               VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR : VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR));
   else assert(image->storage && usage->usage == VK_IMAGE_USAGE_STORAGE_BIT &&
         (info->format == VK_FORMAT_R8_UNORM || info->format == VK_FORMAT_R8G8_UNORM));
   *view = HANDLE(VkImageView,make(VIEW,ID(info->image),0,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_view(VkDevice d, VkImageView view, const VkAllocationCallbacks *allocator)
{
   device(d); assert(!allocator); destroy(ID(view),VIEW);
}
static VKAPI_ATTR VkResult VKAPI_CALL create_buffer(VkDevice d,
      const VkBufferCreateInfo *info, const VkAllocationCallbacks *allocator, VkBuffer *buffer)
{
   VkResult status;
   uintptr_t id;
   device(d); assert(!allocator && info->size);
   id = make(BUFFER,0,info->size,&status); *buffer = HANDLE(VkBuffer,id);
   if (status == VK_SUCCESS)
   {
      objects[id].host = !!(info->usage & VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR);
      assert(objects[id].host ? info->pNext != NULL :
            info->usage == (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
   }
   return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_buffer(VkDevice d, VkBuffer buffer, const VkAllocationCallbacks *allocator)
{
   device(d); assert(!allocator); destroy(ID(buffer),BUFFER);
}
static VKAPI_ATTR void VKAPI_CALL buffer_memory(VkDevice d, VkBuffer buffer, VkMemoryRequirements *requirements)
{
   struct object *entry;
   device(d); entry = object(ID(buffer),BUFFER);
   requirements->size = (entry->bytes + 4095) / 4096 * 4096;
   requirements->alignment = 256; requirements->memoryTypeBits = entry->host ? 12 : 2;
}
static VKAPI_ATTR VkResult VKAPI_CALL bind_buffer(VkDevice d, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset)
{
   device(d); object(ID(buffer),BUFFER); assert(!offset && object(ID(memory),MEMORY)->parent == ID(buffer));
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice d, const VkMemoryAllocateInfo *info,
      const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
   const VkMemoryDedicatedAllocateInfo *dedicated = (const VkMemoryDedicatedAllocateInfo*)info->pNext;
   VkResult status;
   unsigned parent;
   device(d); assert(!allocator && dedicated && (!!dedicated->image != !!dedicated->buffer));
   parent = dedicated->image ? ID(dedicated->image) : ID(dedicated->buffer);
   object(parent,dedicated->image ? IMAGE : BUFFER);
   assert(info->memoryTypeIndex == (objects[parent].host ? (memory_mode ? 2 : 3) : 1));
   *memory = HANDLE(VkDeviceMemory,make(MEMORY,parent,info->allocationSize,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice d, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
   struct object *entry;
   device(d); assert(!allocator); entry = object(ID(memory),MEMORY);
   assert(!entry->mapped && !objects[entry->parent].alive); destroy(ID(memory),MEMORY);
}
static VKAPI_ATTR VkResult VKAPI_CALL map(VkDevice d, VkDeviceMemory memory,
      VkDeviceSize offset, VkDeviceSize bytes, VkMemoryMapFlags flags, void **data)
{
   VkResult status = result();
   struct object *entry;
   device(d); entry = object(ID(memory),MEMORY); assert(!offset && !flags && bytes <= entry->bytes && !entry->mapped);
   *data = (void*)(uintptr_t)(status == VK_SUCCESS ? 100000 + ID(memory) : 99999);
   if (status == VK_SUCCESS) entry->mapped = 1;
   return status;
}
static VKAPI_ATTR void VKAPI_CALL unmap(VkDevice d, VkDeviceMemory memory)
{
   struct object *entry;
   device(d); entry = object(ID(memory),MEMORY); assert(entry->mapped); entry->mapped = 0;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_commands(VkDevice d,
      const VkCommandPoolCreateInfo *info, const VkAllocationCallbacks *allocator, VkCommandPool *pool)
{
   VkResult status;
   device(d); assert(!allocator && info->queueFamilyIndex == config.encode_family);
   assert(info->flags == VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
   *pool = HANDLE(VkCommandPool,make(COMMAND_POOL,0,0,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_commands(VkDevice d, VkCommandPool pool, const VkAllocationCallbacks *allocator)
{
   unsigned i;
   device(d); assert(!allocator); object(ID(pool),COMMAND_POOL);
   for (i=1;i<=used;i++) if (objects[i].alive && objects[i].type == COMMAND && objects[i].parent == ID(pool)) destroy(i,COMMAND);
   destroy(ID(pool),COMMAND_POOL);
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate_command(VkDevice d,
      const VkCommandBufferAllocateInfo *info, VkCommandBuffer *command)
{
   VkResult status;
   device(d); object(ID(info->commandPool),COMMAND_POOL);
   assert(info->commandBufferCount == 1 && info->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
   *command = HANDLE(VkCommandBuffer,make(COMMAND,ID(info->commandPool),0,&status)); return status;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_fence(VkDevice d, const VkFenceCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkFence *fence)
{
   VkResult status;
   device(d); assert(!allocator && !info->flags); *fence = HANDLE(VkFence,make(FENCE,0,0,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice d, VkFence fence, const VkAllocationCallbacks *allocator)
{
   device(d); assert(!allocator); destroy(ID(fence),FENCE);
}
static VKAPI_ATTR VkResult VKAPI_CALL create_queries(VkDevice d, const VkQueryPoolCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkQueryPool *queries)
{
   const VkQueryPoolVideoEncodeFeedbackCreateInfoKHR *feedback = (const VkQueryPoolVideoEncodeFeedbackCreateInfoKHR*)info->pNext;
   VkResult status;
   device(d); assert(!allocator && info->queryCount == config.slots && info->queryType == VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR);
   assert(feedback->pNext == &profile && feedback->encodeFeedbackFlags ==
         (VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR | VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR));
   *queries = HANDLE(VkQueryPool,make(QUERY_POOL,0,0,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_queries(VkDevice d, VkQueryPool queries, const VkAllocationCallbacks *allocator)
{
   device(d); assert(!allocator); destroy(ID(queries),QUERY_POOL);
}
static VKAPI_ATTR VkResult VKAPI_CALL create_semaphore(VkDevice d, const VkSemaphoreCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkSemaphore *semaphore)
{
   const VkSemaphoreTypeCreateInfo *type = (const VkSemaphoreTypeCreateInfo*)info->pNext;
   VkResult status;
   device(d); assert(!allocator && type->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE && !type->initialValue);
   *semaphore = HANDLE(VkSemaphore,make(SEMAPHORE,0,0,&status)); return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_semaphore(VkDevice d, VkSemaphore semaphore, const VkAllocationCallbacks *allocator)
{
   device(d); assert(!allocator); destroy(ID(semaphore),SEMAPHORE);
}
static const vulkan_encode_pool_ops_t ops = {
   memory_properties, video_formats, image_properties, format_properties,
   create_image, destroy_image, image_memory, bind_image, create_view, destroy_view,
   create_buffer, destroy_buffer, buffer_memory, bind_buffer, map, unmap, allocate, free_memory,
   create_commands, destroy_commands, allocate_command, create_fence, destroy_fence,
   create_queries, destroy_queries, create_semaphore, destroy_semaphore
};
static void setup(bool storage)
{
   assert(!live); memset(objects,0,sizeof(objects));
   used = calls = fail_call = allocations = fail_allocation = memory_mode = format_mode = 0;
   memset(&config,0,sizeof(config)); memset(&capabilities,0,sizeof(capabilities));
   memset(&profile,0,sizeof(profile)); memset(&codec,0,sizeof(codec));
   codec.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR; codec.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
   profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR; profile.pNext = &codec;
   profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
   profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   profile.lumaBitDepth = profile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   capabilities.minBitstreamBufferSizeAlignment = 64;
   capabilities.pictureAccessGranularity.width = capabilities.pictureAccessGranularity.height = 16;
   capabilities.minCodedExtent.width = 160; capabilities.minCodedExtent.height = 64;
   capabilities.maxCodedExtent.width = capabilities.maxCodedExtent.height = 8192;
   capabilities.maxDpbSlots = 2; capabilities.maxActiveReferencePictures = 1;
   config.physical = HANDLE(VkPhysicalDevice,9000); config.device = HANDLE(VkDevice,9001);
   config.profile = &profile; config.capabilities = &capabilities;
   config.extent.width = 192; config.extent.height = 128;
   config.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM; config.packet_size = 1000;
   config.slots = 4; config.encode_family = 4; config.producer_family = 0;
   config.storage_image_extended_formats = storage;
}
int main(void)
{
   vulkan_encode_pool_t *pool;
   vulkan_encode_pool_ops_t missing;
   unsigned mode, i, j, total, heaps, faults = 0;
   for (mode=0;mode<2;mode++)
   {
      setup(mode != 0); pool = vulkan_encode_pool_new(&config,&ops);
      assert(pool && pool->count == 4 && pool->storage_images == (mode != 0));
      total = calls; heaps = allocations;
      for (i=0;i<pool->count;i++)
      {
         assert(pool->slot[i].output.mapped && pool->slot[i].output.size == 1024);
         assert(pool->slot[i].output.properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
         assert(mode ? pool->slot[i].planes[0] && !pool->slot[i].conversion.buffer :
               !pool->slot[i].planes[0] && pool->slot[i].conversion.buffer);
      }
      vulkan_encode_pool_free(pool); assert(!live);
      for (i=1;i<=total;i++)
      {
         setup(mode != 0); fail_call = i;
         pool = vulkan_encode_pool_new(&config,&ops);
         vulkan_encode_pool_free(pool); assert(!live); faults++;
      }
      for (i=1;i<=heaps;i++)
      {
         setup(mode != 0); fail_allocation = i;
         pool = vulkan_encode_pool_new(&config,&ops);
         vulkan_encode_pool_free(pool); assert(!live); faults++;
      }
   }
   for (i=1;i<=2;i++)
   {
      setup(true); memory_mode = i; pool = vulkan_encode_pool_new(&config,&ops);
      assert(pool && !(pool->slot[0].output.properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT));
      vulkan_encode_pool_free(pool); assert(!live);
      setup(true); format_mode = i; pool = vulkan_encode_pool_new(&config,&ops);
      assert(pool && !pool->storage_images); vulkan_encode_pool_free(pool); assert(!live);
   }
   for (i=3;i<=4;i++)
   {
      setup(true); memory_mode = i; assert(!vulkan_encode_pool_new(&config,&ops) && !live);
      setup(true); format_mode = i; assert(!vulkan_encode_pool_new(&config,&ops) && !live);
   }
   for (i=0;i<24;i++)
   {
      setup(true);
      if (!i) config.slots = 0;
      if (i == 1) config.slots = 17;
      if (i == 2) config.packet_size = 64u * 1024u * 1024u + 1;
      if (i == 3) capabilities.minBitstreamBufferSizeAlignment = 0;
      if (i == 4) config.extent.width = 191;
      if (i == 5) config.encode_family = UINT32_MAX;
      if (i == 6) config.producer_family = UINT32_MAX;
      if (i == 7) config.device = VK_NULL_HANDLE;
      if (i == 8) config.physical = VK_NULL_HANDLE;
      if (i == 9) config.profile = NULL;
      if (i == 10) config.capabilities = NULL;
      if (i == 11) config.extent.height = 0;
      if (i == 12) config.extent.width = 8194;
      if (i == 13) config.extent.height = 127;
      if (i == 14) config.packet_size = 0;
      if (i == 15) config.format = VK_FORMAT_R8_UNORM;
      if (i == 16) capabilities.pictureAccessGranularity.width = 0;
      if (i == 17) capabilities.pictureAccessGranularity.height = 0;
      if (i == 18) capabilities.pictureAccessGranularity.width = 128;
      if (i == 19) capabilities.maxDpbSlots = 1;
      if (i == 20) capabilities.maxActiveReferencePictures = 0;
      if (i == 21) capabilities.minCodedExtent.width = 208;
      if (i == 22) capabilities.maxCodedExtent.height = 112;
      if (i == 23) capabilities.minBitstreamBufferSizeAlignment = 64u * 1024u * 1024u + 1;
      assert(!vulkan_encode_pool_new(&config,&ops) && !live && !calls);
   }
   setup(true);
   assert(!vulkan_encode_pool_new(NULL,&ops));
   assert(!vulkan_encode_pool_new(&config,NULL));
#define MISSING(member) do { missing = ops; missing.member = NULL; \
   assert(!vulkan_encode_pool_new(&config,&missing) && !live && !calls); } while (0)
   MISSING(memory_properties);
   MISSING(video_formats);
   MISSING(image_properties);
   MISSING(format_properties);
   MISSING(create_image);
   MISSING(destroy_image);
   MISSING(image_memory);
   MISSING(bind_image);
   MISSING(create_view);
   MISSING(destroy_view);
   MISSING(create_buffer);
   MISSING(destroy_buffer);
   MISSING(buffer_memory);
   MISSING(bind_buffer);
   MISSING(map);
   MISSING(unmap);
   MISSING(allocate);
   MISSING(free_memory);
   MISSING(create_commands);
   MISSING(destroy_commands);
   MISSING(allocate_command);
   MISSING(create_fence);
   MISSING(destroy_fence);
   MISSING(create_queries);
   MISSING(destroy_queries);
   MISSING(create_semaphore);
   MISSING(destroy_semaphore);
#undef MISSING
   vulkan_encode_pool_free(NULL);
   for (j=1;j<=16;j+=15)
   {
      setup(true); config.slots = j; pool = vulkan_encode_pool_new(&config,&ops);
      assert(pool && pool->count == j); vulkan_encode_pool_free(pool); assert(!live);
   }
   printf("Vulkan encoder pool ownership/fallback/cache %u injected faults PASS\n",faults);
   return 0;
}
