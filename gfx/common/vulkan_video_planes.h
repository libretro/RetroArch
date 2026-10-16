/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GFX_VULKAN_VIDEO_PLANES_H
#define GFX_VULKAN_VIDEO_PLANES_H

/* The image needs MUTABLE_FORMAT and SAMPLED usage. */
static bool vulkan_video_plane_views_layer(VkDevice device, VkImage image,
      unsigned pixfmt, unsigned layer, VkImageView views[2])
{
   VkImageViewCreateInfo info;
   VkImageViewUsageCreateInfo usage;
   unsigned i;
   memset(&info, 0, sizeof(info));
   usage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
   usage.pNext = NULL;
   usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   info.pNext = &usage;
   info.image = image;
   info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   info.subresourceRange.baseArrayLayer = layer;
   info.subresourceRange.levelCount = info.subresourceRange.layerCount = 1;
   for (i = 0; i < 2; i++)
   {
      info.format = pixfmt == IMAGE_PIXFMT_P010
         ? (i ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16_UNORM)
         : (i ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8_UNORM);
      info.subresourceRange.aspectMask = i ? VK_IMAGE_ASPECT_PLANE_1_BIT
         : VK_IMAGE_ASPECT_PLANE_0_BIT;
      if (vkCreateImageView(device, &info, NULL, &views[i]) != VK_SUCCESS)
      {
         if (i) vkDestroyImageView(device, views[0], NULL);
         views[0] = views[1] = VK_NULL_HANDLE;
         return false;
      }
   }
   return true;
}
static bool vulkan_video_plane_views(VkDevice device, VkImage image,
      unsigned pixfmt, VkImageView views[2])
{
   return vulkan_video_plane_views_layer(device, image, pixfmt, 0, views);
}
#endif
