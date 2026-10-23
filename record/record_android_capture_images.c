/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_images.h"
#if defined(__ANDROID__) || defined(RECORD_ANDROID_CAPTURE_TEST)
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#ifdef RECORD_ANDROID_CAPTURE_TEST
#include "record_android_capture_test_types.h"
#else
#if !defined(__cplusplus)                                                      \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)            \
      && !defined(inline)
#define inline __inline__
#define RECORD_AC_UNDEF_INLINE
#endif
#include <android/hardware_buffer.h>
#ifdef RECORD_AC_UNDEF_INLINE
#undef inline
#undef RECORD_AC_UNDEF_INLINE
#endif
#endif

typedef int (*record_ac_allocate_fn)(
      const AHardwareBuffer_Desc *, AHardwareBuffer **);
typedef void (*record_ac_describe_fn)(
      const AHardwareBuffer *, AHardwareBuffer_Desc *);
typedef void (*record_ac_release_fn)(AHardwareBuffer *);
#if defined(__ANDROID__) && defined(__clang__) && __ANDROID_API__ >= 26
#define RECORD_AC_ABI(type, symbol)                                            \
   __extension__ typedef char symbol##_record_capture_abi                      \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
RECORD_AC_ABI(record_ac_allocate_fn, AHardwareBuffer_allocate);
RECORD_AC_ABI(record_ac_describe_fn, AHardwareBuffer_describe);
RECORD_AC_ABI(record_ac_release_fn, AHardwareBuffer_release);
#undef RECORD_AC_ABI
#endif

typedef struct record_ac_image
{
   VkImage image;
   VkDeviceMemory memory;
   AHardwareBuffer *buffer;
} record_ac_image_t;
struct record_android_capture_images
{
   record_ac_image_t slots[16];
   record_android_capture_image_ops_t ops;
   VkDevice device;
   dylib_t library;
   record_ac_release_fn release;
   unsigned count;
};

void record_android_capture_images_prepare_device_destroy(
      record_android_capture_images_t *c)
{
   unsigned i;
   if (!c)
      return;
   for (i = 0; i < c->count; i++)
   {
      record_ac_image_t *s = &c->slots[i];
      if (s->image)
         c->ops.destroy(c->device, s->image, NULL);
      if (s->memory)
         c->ops.free_memory(c->device, s->memory, NULL);
      s->image = VK_NULL_HANDLE;
      s->memory = VK_NULL_HANDLE;
   }
}

void record_android_capture_images_free(record_android_capture_images_t *c)
{
   unsigned i;
   if (!c)
      return;
   record_android_capture_images_prepare_device_destroy(c);
   for (i = 0; i < c->count; i++)
   {
      record_ac_image_t *s = &c->slots[i];
      if (s->buffer)
         c->release(s->buffer);
   }
   if (c->library)
      dylib_close(c->library);
   free(c);
}

static bool record_ac_import(record_android_capture_images_t *c,
      record_ac_image_t *s, unsigned width, unsigned height, VkFormat format,
      const VkPhysicalDeviceMemoryProperties *memory)
{
   VkAndroidHardwareBufferPropertiesANDROID properties;
   VkAndroidHardwareBufferFormatPropertiesANDROID native_format;
   VkExternalMemoryImageCreateInfo external;
   VkImageCreateInfo image;
   VkImportAndroidHardwareBufferInfoANDROID import;
   VkMemoryDedicatedAllocateInfo dedicated;
   VkMemoryAllocateInfo allocation;
   unsigned i;
   memset(&properties, 0, sizeof(properties));
   memset(&native_format, 0, sizeof(native_format));
   properties.sType
         = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
   properties.pNext = &native_format;
   native_format.sType
         = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
   if (c->ops.properties(c->device, s->buffer, &properties) != VK_SUCCESS
         || !properties.allocationSize || native_format.format != format)
      return false;
   for (i = 0; i < memory->memoryTypeCount; i++)
      if ((properties.memoryTypeBits & ((uint32_t)1 << i))
            && !(memory->memoryTypes[i].propertyFlags
                  & VK_MEMORY_PROPERTY_PROTECTED_BIT))
         break;
   if (i == memory->memoryTypeCount)
      return false;
   memset(&external, 0, sizeof(external));
   external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
   external.handleTypes
         = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
   memset(&image, 0, sizeof(image));
   image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext = &external;
   image.imageType = VK_IMAGE_TYPE_2D;
   image.format = format;
   image.extent.width = width;
   image.extent.height = height;
   image.extent.depth = 1;
   image.mipLevels = image.arrayLayers = 1;
   image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (c->ops.create(c->device, &image, NULL, &s->image) != VK_SUCCESS)
      return false;
   memset(&import, 0, sizeof(import));
   import.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
   import.buffer = s->buffer;
   memset(&dedicated, 0, sizeof(dedicated));
   dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
   dedicated.pNext = &import;
   dedicated.image = s->image;
   memset(&allocation, 0, sizeof(allocation));
   allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.pNext = &dedicated;
   allocation.allocationSize = properties.allocationSize;
   allocation.memoryTypeIndex = i;
   return c->ops.allocate(c->device, &allocation, NULL, &s->memory)
         == VK_SUCCESS
         && c->ops.bind(c->device, s->image, s->memory, 0) == VK_SUCCESS;
}

record_android_capture_images_t *record_android_capture_images_new(
      VkPhysicalDevice gpu, VkDevice device,
      const record_android_capture_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, unsigned count,
      unsigned width, unsigned height, VkFormat format)
{
   VkPhysicalDeviceExternalImageFormatInfo external;
   VkPhysicalDeviceImageFormatInfo2 input;
   VkExternalImageFormatProperties external_properties;
   VkAndroidHardwareBufferUsageANDROID usage;
   VkImageFormatProperties2 output;
   AHardwareBuffer_Desc desc, actual;
   record_android_capture_images_t *c;
   record_ac_allocate_fn allocate;
   record_ac_describe_fn describe;
   uint32_t native_format;
   unsigned i, j;
   if (!gpu || !device || !ops || !ops->query || !ops->properties
         || !ops->create || !ops->destroy || !ops->allocate || !ops->free_memory
         || !ops->bind || !memory || !memory->memoryTypeCount
         || memory->memoryTypeCount > 32 || !count || count > 16 || !width
         || !height || width > 16384 || height > 16384)
      return NULL;
   switch (format)
   {
      case VK_FORMAT_R8G8B8A8_UNORM:
         native_format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
         break;
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
         native_format = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
         break;
      case VK_FORMAT_R16G16B16A16_SFLOAT:
         native_format = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
         break;
      default:
         return NULL;
   }
   memset(&external, 0, sizeof(external));
   external.sType
         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
   external.handleType
         = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
   memset(&input, 0, sizeof(input));
   input.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
   input.pNext = &external;
   input.format = format;
   input.type = VK_IMAGE_TYPE_2D;
   input.tiling = VK_IMAGE_TILING_OPTIMAL;
   input.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   memset(&external_properties, 0, sizeof(external_properties));
   external_properties.sType
         = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
   external_properties.pNext = &usage;
   memset(&usage, 0, sizeof(usage));
   usage.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID;
   memset(&output, 0, sizeof(output));
   output.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
   output.pNext = &external_properties;
   /* Known RGB formats use the physical image query, not external-format flags.
    */
   if (ops->query(gpu, &input, &output) != VK_SUCCESS
         || !(external_properties.externalMemoryProperties
                     .externalMemoryFeatures
               & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)
         || !(external_properties.externalMemoryProperties.compatibleHandleTypes
               & external.handleType)
         || output.imageFormatProperties.maxExtent.width < width
         || output.imageFormatProperties.maxExtent.height < height
         || !output.imageFormatProperties.maxExtent.depth
         || !output.imageFormatProperties.maxMipLevels
         || !output.imageFormatProperties.maxArrayLayers
         || !(output.imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT)
         || !(usage.androidHardwareBufferUsage
               & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)
         || (usage.androidHardwareBufferUsage
               & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT))
      return NULL;
   c = (record_android_capture_images_t *)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->device = device;
   c->ops = *ops;
   c->count = count;
   c->library = dylib_load("libandroid.so");
   if (!c->library)
      goto failed;
   allocate = (record_ac_allocate_fn)dylib_proc(
         c->library, "AHardwareBuffer_allocate");
   describe = (record_ac_describe_fn)dylib_proc(
         c->library, "AHardwareBuffer_describe");
   c->release = (record_ac_release_fn)dylib_proc(
         c->library, "AHardwareBuffer_release");
   if (!allocate || !describe || !c->release)
      goto failed;
   memset(&desc, 0, sizeof(desc));
   desc.width = width;
   desc.height = height;
   desc.layers = 1;
   desc.format = native_format;
   desc.usage = usage.androidHardwareBufferUsage;
   for (i = 0; i < count; i++)
   {
      record_ac_image_t *s = &c->slots[i];
      if (allocate(&desc, &s->buffer) || !s->buffer)
         goto failed;
      for (j = 0; j < i; j++)
         if (c->slots[j].buffer == s->buffer)
            goto failed;
      memset(&actual, 0, sizeof(actual));
      describe(s->buffer, &actual);
      if (actual.width != width || actual.height != height || actual.layers != 1
            || actual.format != native_format
            || (actual.usage & desc.usage) != desc.usage
            || (actual.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT)
            || !record_ac_import(c, s, width, height, format, memory))
         goto failed;
   }
   return c;
failed:
   record_android_capture_images_free(c);
   return NULL;
}

VkImage record_android_capture_images_image(
      const record_android_capture_images_t *c, unsigned slot)
{
   return c && slot < c->count ? c->slots[slot].image : VK_NULL_HANDLE;
}
struct AHardwareBuffer *record_android_capture_images_buffer(
      const record_android_capture_images_t *c, unsigned slot)
{
   return c && slot < c->count ? c->slots[slot].buffer : NULL;
}
#else
record_android_capture_images_t *record_android_capture_images_new(
      VkPhysicalDevice gpu, VkDevice device,
      const record_android_capture_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, unsigned count,
      unsigned width, unsigned height, VkFormat format)
{
   (void)gpu;
   (void)device;
   (void)ops;
   (void)memory;
   (void)count;
   (void)width;
   (void)height;
   (void)format;
   return NULL;
}
VkImage record_android_capture_images_image(
      const record_android_capture_images_t *c, unsigned slot)
{
   (void)c;
   (void)slot;
   return VK_NULL_HANDLE;
}
struct AHardwareBuffer *record_android_capture_images_buffer(
      const record_android_capture_images_t *c, unsigned slot)
{
   (void)c;
   (void)slot;
   return NULL;
}
void record_android_capture_images_free(record_android_capture_images_t *c)
{
   (void)c;
}
void record_android_capture_images_prepare_device_destroy(
      record_android_capture_images_t *c)
{
   (void)c;
}
#endif
