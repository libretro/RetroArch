/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../record/record_android_capture_images.h"
#include "record_android_capture_test_types.h"

#define GPU ((VkPhysicalDevice)(uintptr_t)1)
#define DEVICE ((VkDevice)(uintptr_t)2)
#define IMAGE(i) ((VkImage)(uintptr_t)(0x100u + (i)))
#define MEMORY(i) ((VkDeviceMemory)(uintptr_t)(0x200u + (i)))
static AHardwareBuffer buffers[16];
static bool images[16], memories[16];
static unsigned allocated, created, memory_count, bound, properties_count;
static unsigned library, missing, bad_query, bad_desc, bad_properties;
static unsigned fail_stage, fail_at, duplicate_at;
static bool library_fail;
static VkFormat selected;
static VkPhysicalDeviceMemoryProperties memory;
static uint64_t usage;

static VkResult VKAPI_CALL query(VkPhysicalDevice gpu,
      const VkPhysicalDeviceImageFormatInfo2 *in, VkImageFormatProperties2 *out)
{
   const VkPhysicalDeviceExternalImageFormatInfo *e = in->pNext;
   VkExternalImageFormatProperties *p = out->pNext;
   VkAndroidHardwareBufferUsageANDROID *u = p->pNext;
   assert(gpu == GPU
         && in->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2);
   assert(in->format == selected && in->type == VK_IMAGE_TYPE_2D
         && in->tiling == VK_IMAGE_TILING_OPTIMAL && !in->flags);
   assert(in->usage
         == (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
   assert(e->sType
               == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO
         && e->handleType
               == VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID);
   assert(out->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);
   assert(p->sType == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES
         && u->sType
               == VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID);
   if (bad_query == 1)
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
   p->externalMemoryProperties.externalMemoryFeatures
         = bad_query == 2 ? 0 : VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
   p->externalMemoryProperties.compatibleHandleTypes
         = bad_query == 3 ? 0 : e->handleType;
   out->imageFormatProperties.maxExtent.width = bad_query == 4 ? 127 : 128;
   out->imageFormatProperties.maxExtent.height = bad_query == 5 ? 95 : 96;
   out->imageFormatProperties.maxExtent.depth = bad_query == 6 ? 0 : 1;
   out->imageFormatProperties.maxMipLevels = bad_query == 7 ? 0 : 1;
   out->imageFormatProperties.maxArrayLayers = bad_query == 8 ? 0 : 1;
   out->imageFormatProperties.sampleCounts
         = bad_query == 9 ? 0 : VK_SAMPLE_COUNT_1_BIT;
   u->androidHardwareBufferUsage = bad_query == 10 ? 0 : usage;
   if (bad_query == 11)
      u->androidHardwareBufferUsage |= AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
   return VK_SUCCESS;
}
static int buffer_allocate(
      const AHardwareBuffer_Desc *desc, AHardwareBuffer **out)
{
   unsigned at = allocated++;
   assert(library && at < 16 && desc->width == 128 && desc->height == 96);
   assert(desc->layers == 1 && desc->usage == usage && !desc->stride
         && !desc->rfu0 && !desc->rfu1);
   assert(desc->format
         == (selected == VK_FORMAT_R8G8B8A8_UNORM
                     ? AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
                     : selected == VK_FORMAT_A2B10G10R10_UNORM_PACK32
                     ? AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM
                     : AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT));
   if (at + 1 == duplicate_at)
      at = 0;
   buffers[at].desc = *desc;
   buffers[at].refs++;
   *out = &buffers[at];
   return fail_stage == 1 && allocated == fail_at ? -1 : 0;
}
static void buffer_describe(const AHardwareBuffer *b, AHardwareBuffer_Desc *out)
{
   assert(b->refs && library);
   *out = b->desc;
   switch (bad_desc)
   {
      case 1:
         out->width++;
         break;
      case 2:
         out->height++;
         break;
      case 3:
         out->layers++;
         break;
      case 4:
         out->format = 0;
         break;
      case 5:
         out->usage = 0;
         break;
      case 6:
         out->usage |= AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
         break;
   }
}
static void buffer_release(AHardwareBuffer *b)
{
   assert(library && b->refs);
   b->refs--;
}
dylib_t dylib_load(const char *name)
{
   assert(!strcmp(name, "libandroid.so") && !library);
   if (library_fail)
      return NULL;
   library++;
   return (dylib_t)(uintptr_t)1;
}
function_t dylib_proc(dylib_t lib, const char *name)
{
   assert(lib && library);
   if (!strcmp(name, "AHardwareBuffer_allocate"))
      return missing == 1 ? NULL : (function_t)buffer_allocate;
   if (!strcmp(name, "AHardwareBuffer_describe"))
      return missing == 2 ? NULL : (function_t)buffer_describe;
   assert(!strcmp(name, "AHardwareBuffer_release"));
   return missing == 3 ? NULL : (function_t)buffer_release;
}
void dylib_close(dylib_t lib)
{
   unsigned i;
   assert(lib && library);
   for (i = 0; i < 16; i++)
      assert(!buffers[i].refs && !images[i] && !memories[i]);
   library--;
}
static VkResult VKAPI_CALL properties(VkDevice device,
      const struct AHardwareBuffer *b,
      VkAndroidHardwareBufferPropertiesANDROID *out)
{
   VkAndroidHardwareBufferFormatPropertiesANDROID *f = out->pNext;
   assert(device == DEVICE && b->refs && library);
   assert(out->sType
               == VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID
         && f->sType
               == VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID);
   properties_count++;
   if ((fail_stage == 2 && properties_count == fail_at) || bad_properties == 1)
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   out->allocationSize = bad_properties == 2 ? 0 : 65536;
   out->memoryTypeBits = bad_properties == 3 ? 0 : bad_properties == 4 ? 1 : 3;
   f->format = bad_properties == 5 ? VK_FORMAT_UNDEFINED : selected;
   f->externalFormat = 123;
   f->formatFeatures = 0;
   return VK_SUCCESS;
}
static VkResult VKAPI_CALL create_image(VkDevice device,
      const VkImageCreateInfo *in, const VkAllocationCallbacks *allocator,
      VkImage *out)
{
   const VkExternalMemoryImageCreateInfo *e = in->pNext;
   unsigned at = created++;
   assert(device == DEVICE && !allocator && at < 16);
   assert(in->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
         && e->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO
         && !e->pNext);
   assert(e->handleTypes
         == VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID);
   assert(in->imageType == VK_IMAGE_TYPE_2D && in->format == selected
         && in->extent.width == 128 && in->extent.height == 96
         && in->extent.depth == 1);
   assert(in->mipLevels == 1 && in->arrayLayers == 1
         && in->samples == VK_SAMPLE_COUNT_1_BIT
         && in->tiling == VK_IMAGE_TILING_OPTIMAL);
   assert(in->usage
         == (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
   assert(in->sharingMode == VK_SHARING_MODE_EXCLUSIVE
         && !in->queueFamilyIndexCount
         && in->initialLayout == VK_IMAGE_LAYOUT_UNDEFINED);
   images[at] = true;
   *out = IMAGE(at);
   return fail_stage == 3 && created == fail_at ? VK_ERROR_OUT_OF_DEVICE_MEMORY
                                                : VK_SUCCESS;
}
static void VKAPI_CALL destroy_image(
      VkDevice device, VkImage image, const VkAllocationCallbacks *allocator)
{
   unsigned at = (unsigned)((uintptr_t)image - 0x100u);
   assert(device == DEVICE && !allocator && at < 16 && images[at]);
   images[at] = false;
}
static VkResult VKAPI_CALL allocate_memory(VkDevice device,
      const VkMemoryAllocateInfo *in, const VkAllocationCallbacks *allocator,
      VkDeviceMemory *out)
{
   const VkMemoryDedicatedAllocateInfo *d = in->pNext;
   const VkImportAndroidHardwareBufferInfoANDROID *i = d->pNext;
   unsigned at = memory_count++;
   assert(device == DEVICE && !allocator && at < 16);
   assert(in->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
         && in->allocationSize == 65536 && in->memoryTypeIndex == 1);
   assert(d->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
         && d->image == IMAGE(at) && !d->buffer);
   assert(i->sType
               == VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID
         && !i->pNext && i->buffer == &buffers[at] && images[at]);
   buffers[at].refs++;
   memories[at] = true;
   *out = MEMORY(at);
   return fail_stage == 4 && memory_count == fail_at
         ? VK_ERROR_OUT_OF_DEVICE_MEMORY
         : VK_SUCCESS;
}
static void VKAPI_CALL free_memory(VkDevice device, VkDeviceMemory mem,
      const VkAllocationCallbacks *allocator)
{
   unsigned at = (unsigned)((uintptr_t)mem - 0x200u);
   assert(device == DEVICE && !allocator && at < 16 && !images[at]
         && memories[at]);
   assert(buffers[at].refs > 1);
   buffers[at].refs--;
   memories[at] = false;
}
static VkResult VKAPI_CALL bind_memory(
      VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize offset)
{
   unsigned at = bound++;
   assert(device == DEVICE && image == IMAGE(at) && mem == MEMORY(at) && !offset
         && images[at] && memories[at]);
   return fail_stage == 5 && bound == fail_at ? VK_ERROR_OUT_OF_DEVICE_MEMORY
                                              : VK_SUCCESS;
}
static const record_android_capture_image_ops_t ops = {query, properties,
      create_image, destroy_image, allocate_memory, free_memory, bind_memory};
static void reset(void)
{
   unsigned i;
   assert(!library);
   for (i = 0; i < 16; i++)
      assert(!buffers[i].refs && !images[i] && !memories[i]);
   allocated = created = memory_count = bound = properties_count = 0;
   missing = bad_query = bad_desc = bad_properties = 0;
   fail_stage = fail_at = duplicate_at = 0;
   library_fail = false;
   selected = VK_FORMAT_R8G8B8A8_UNORM;
   usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | ((uint64_t)1 << 48);
   memset(&memory, 0, sizeof(memory));
   memory.memoryTypeCount = 2;
   memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT;
   memory.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}
static record_android_capture_images_t *create(unsigned count)
{
   return record_android_capture_images_new(
         GPU, DEVICE, &ops, &memory, count, 128, 96, selected);
}
int main(void)
{
   record_android_capture_images_t *c;
   record_android_capture_image_ops_t missing_ops;
   unsigned i, stage, at;
   VkFormat formats[3] = {VK_FORMAT_R8G8B8A8_UNORM,
         VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16G16B16A16_SFLOAT};
   reset();
   assert(!create(0) && !create(17));
#define MISSING(member)                                                        \
   do                                                                          \
   {                                                                           \
      missing_ops = ops;                                                       \
      missing_ops.member = NULL;                                               \
      assert(!record_android_capture_images_new(                               \
            GPU, DEVICE, &missing_ops, &memory, 2, 128, 96, selected));        \
   } while (0)
   MISSING(query);
   MISSING(properties);
   MISSING(create);
   MISSING(destroy);
   MISSING(allocate);
   MISSING(free_memory);
   MISSING(bind);
#undef MISSING
   for (i = 1; i <= 11; i++)
   {
      reset();
      bad_query = i;
      assert(!create(2));
   }
   for (i = 1; i <= 6; i++)
   {
      reset();
      bad_desc = i;
      assert(!create(2));
   }
   for (i = 1; i <= 5; i++)
   {
      reset();
      bad_properties = i;
      assert(!create(2));
   }
   for (stage = 1; stage <= 5; stage++)
      for (at = 1; at <= 4; at++)
      {
         reset();
         fail_stage = stage;
         fail_at = at;
         assert(!create(4));
      }
   for (i = 1; i <= 3; i++)
   {
      reset();
      missing = i;
      assert(!create(2));
   }
   reset();
   duplicate_at = 2;
   assert(!create(2));
   reset();
   library_fail = true;
   assert(!create(2));
   for (i = 0; i < 3; i++)
   {
      reset();
      selected = formats[i];
      c = create(16);
      assert(c);
      for (at = 0; at < 16; at++)
      {
         assert(record_android_capture_images_image(c, at) == IMAGE(at));
         assert(record_android_capture_images_buffer(c, at) == &buffers[at]);
      }
      assert(!record_android_capture_images_image(c, 16));
      assert(!record_android_capture_images_buffer(c, 16));
      record_android_capture_images_free(c);
   }
   reset();
   c = create(2);
   assert(c);
   record_android_capture_images_prepare_device_destroy(c);
   assert(!record_android_capture_images_image(c, 0));
   assert(buffers[0].refs == 1 && buffers[1].refs == 1 && library);
   record_android_capture_images_prepare_device_destroy(c);
   record_android_capture_images_free(c);
   reset();
   selected = VK_FORMAT_UNDEFINED;
   assert(!create(2));
   assert(!record_android_capture_images_image(NULL, 0));
   assert(!record_android_capture_images_buffer(NULL, 0));
   record_android_capture_images_free(NULL);
   puts("PASS: fixed Android RGB8/RGB10/FP16 capture images, native import "
        "capabilities and partial cleanup; native APIs mocked");
   return 0;
}
