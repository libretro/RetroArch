/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_image.h"
#include <string.h>
bool vulkan_android_image_barrier(VkCommandBuffer command,
      PFN_vkCmdPipelineBarrier barrier, VkImage image, unsigned graphics_family,
      VkPipelineStageFlags sample_stage, bool acquire)
{
   VkImageMemoryBarrier info;
   if (!command || !barrier || !image
         || graphics_family >= VK_QUEUE_FAMILY_FOREIGN_EXT
         || (sample_stage != VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
               && sample_stage != VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))
      return false;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   info.image = image;
   info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   info.subresourceRange.levelCount = info.subresourceRange.layerCount = 1;
   info.srcQueueFamilyIndex
         = acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : graphics_family;
   info.dstQueueFamilyIndex
         = acquire ? graphics_family : VK_QUEUE_FAMILY_FOREIGN_EXT;
   info.oldLayout = acquire ? VK_IMAGE_LAYOUT_GENERAL
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   info.newLayout = acquire ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                            : VK_IMAGE_LAYOUT_GENERAL;
   info.srcAccessMask = acquire ? 0 : VK_ACCESS_SHADER_READ_BIT;
   info.dstAccessMask = acquire ? VK_ACCESS_SHADER_READ_BIT : 0;
   barrier(command, acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : sample_stage,
         acquire ? sample_stage : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
         NULL, 0, NULL, 1, &info);
   return true;
}
void vulkan_android_image_destroy(VkDevice device,
      const vulkan_android_image_ops_t *ops, vulkan_android_image_t *image)
{
   if (!image || !ops)
      return;
   if (image->view)
      ops->destroy_view(device, image->view, NULL);
   if (image->sampler)
      ops->destroy_sampler(device, image->sampler, NULL);
   if (image->conversion)
      ops->destroy_conversion(device, image->conversion, NULL);
   if (image->image)
      ops->destroy_image(device, image->image, NULL);
   if (image->memory)
      ops->free_memory(device, image->memory, NULL);
   memset(image, 0, sizeof(*image));
}
bool vulkan_android_image_create(VkDevice device,
      const vulkan_android_image_ops_t *ops, struct AHardwareBuffer *buffer,
      unsigned width, unsigned height,
      const VkPhysicalDeviceMemoryProperties *memory,
      VkSamplerYcbcrModelConversion model, VkSamplerYcbcrRange range,
      vulkan_android_image_t *out)
{
   VkAndroidHardwareBufferPropertiesANDROID properties;
   VkAndroidHardwareBufferFormatPropertiesANDROID format;
   VkExternalFormatANDROID external;
   VkExternalMemoryImageCreateInfo external_image;
   VkImageCreateInfo image;
   VkImportAndroidHardwareBufferInfoANDROID import;
   VkMemoryDedicatedAllocateInfo dedicated;
   VkMemoryAllocateInfo allocation;
   VkSamplerYcbcrConversionCreateInfo conversion;
   VkSamplerYcbcrConversionInfo conversion_info;
   VkSamplerCreateInfo sampler;
   VkImageViewCreateInfo view;
   unsigned i;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!device || !buffer || !width || !height || width > 16384
         || height > 16384 || !memory || !memory->memoryTypeCount
         || memory->memoryTypeCount > 32 || !ops || !ops->properties
         || !ops->create_image || !ops->destroy_image || !ops->allocate
         || !ops->free_memory || !ops->bind || !ops->create_conversion
         || !ops->destroy_conversion || !ops->create_sampler
         || !ops->destroy_sampler || !ops->create_view || !ops->destroy_view
         || (model != VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601
               && model != VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
               && model != VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020)
         || (range != VK_SAMPLER_YCBCR_RANGE_ITU_FULL
               && range != VK_SAMPLER_YCBCR_RANGE_ITU_NARROW))
      return false;
   memset(&properties, 0, sizeof(properties));
   memset(&format, 0, sizeof(format));
   properties.sType
         = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
   properties.pNext = &format;
   format.sType
         = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
   if (ops->properties(device, buffer, &properties) != VK_SUCCESS
         || !properties.allocationSize || !format.externalFormat
         || !(format.formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
      return false;
   /* Protected memory must never be silently imported into an unprotected path.
    */
   for (i = 0; i < memory->memoryTypeCount; i++)
      if ((properties.memoryTypeBits & ((uint32_t)1 << i))
            && !(memory->memoryTypes[i].propertyFlags
                  & VK_MEMORY_PROPERTY_PROTECTED_BIT))
         break;
   if (i == memory->memoryTypeCount)
      return false;
   if ((format.suggestedXChromaOffset != VK_CHROMA_LOCATION_MIDPOINT
             && format.suggestedXChromaOffset
                   != VK_CHROMA_LOCATION_COSITED_EVEN)
         || (format.suggestedYChromaOffset != VK_CHROMA_LOCATION_MIDPOINT
               && format.suggestedYChromaOffset
                     != VK_CHROMA_LOCATION_COSITED_EVEN))
      return false;
   if (((format.suggestedXChromaOffset == VK_CHROMA_LOCATION_MIDPOINT
              || format.suggestedYChromaOffset == VK_CHROMA_LOCATION_MIDPOINT)
             && !(format.formatFeatures
                   & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT))
         || ((format.suggestedXChromaOffset == VK_CHROMA_LOCATION_COSITED_EVEN
                   || format.suggestedYChromaOffset
                         == VK_CHROMA_LOCATION_COSITED_EVEN)
               && !(format.formatFeatures
                     & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)))
      return false;
   memset(&external, 0, sizeof(external));
   external.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
   external.externalFormat = format.externalFormat;
   memset(&external_image, 0, sizeof(external_image));
   external_image.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
   external_image.pNext = &external;
   external_image.handleTypes
         = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
   memset(&image, 0, sizeof(image));
   image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext = &external_image;
   image.imageType = VK_IMAGE_TYPE_2D;
   image.format = VK_FORMAT_UNDEFINED;
   image.extent.width = width;
   image.extent.height = height;
   image.extent.depth = 1;
   image.mipLevels = image.arrayLayers = 1;
   image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (ops->create_image(device, &image, NULL, &out->image) != VK_SUCCESS)
      goto fail;
   memset(&import, 0, sizeof(import));
   import.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
   import.buffer = buffer;
   memset(&dedicated, 0, sizeof(dedicated));
   dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
   dedicated.pNext = &import;
   dedicated.image = out->image;
   memset(&allocation, 0, sizeof(allocation));
   allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.pNext = &dedicated;
   allocation.allocationSize = properties.allocationSize;
   allocation.memoryTypeIndex = i;
   if (ops->allocate(device, &allocation, NULL, &out->memory) != VK_SUCCESS
         || ops->bind(device, out->image, out->memory, 0) != VK_SUCCESS)
      goto fail;
   memset(&conversion, 0, sizeof(conversion));
   conversion.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
   conversion.pNext = &external;
   conversion.format = VK_FORMAT_UNDEFINED;
   conversion.ycbcrModel = model;
   conversion.ycbcrRange = range;
   conversion.components = format.samplerYcbcrConversionComponents;
   conversion.xChromaOffset = format.suggestedXChromaOffset;
   conversion.yChromaOffset = format.suggestedYChromaOffset;
   conversion.chromaFilter = VK_FILTER_NEAREST;
   if (ops->create_conversion(device, &conversion, NULL, &out->conversion)
         != VK_SUCCESS)
      goto fail;
   memset(&conversion_info, 0, sizeof(conversion_info));
   conversion_info.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
   conversion_info.conversion = out->conversion;
   memset(&sampler, 0, sizeof(sampler));
   sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler.pNext = &conversion_info;
   sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
   sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   sampler.addressModeU = sampler.addressModeV = sampler.addressModeW
         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   if (ops->create_sampler(device, &sampler, NULL, &out->sampler) != VK_SUCCESS)
      goto fail;
   memset(&view, 0, sizeof(view));
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext = &conversion_info;
   view.image = out->image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = VK_FORMAT_UNDEFINED;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   if (ops->create_view(device, &view, NULL, &out->view) != VK_SUCCESS)
      goto fail;
   out->external_format = format.externalFormat;
   return true;
fail:
   vulkan_android_image_destroy(device, ops, out);
   return false;
}
