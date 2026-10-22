/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_H
#define RARCH_VAAPI_HEVC_VULKAN_H
#include "vaapi_hevc_export.h"
#include "vulkan_prime_format.h"

/* Both sampled layers must pass before the importer takes any object FD. */
static INLINE bool vaapi_hevc_vulkan_query(vulkan_prime_format_t out[2],
      const vaapi_hevc_export_t *frame, VkPhysicalDevice gpu, bool prime_device,
      PFN_vkGetPhysicalDeviceFormatProperties2 get_format,
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image)
{
   vulkan_prime_format_t layers[2];
   const VADRMPRIMESurfaceDescriptor *d;
   unsigned i, j;
   bool ten;
   if (!out || !frame || !frame->active || !frame->ready || !prime_device)
      return false;
   d = &frame->descriptor;
   if (d->num_layers != 2 || !d->num_objects || d->num_objects > 4
         || !d->width || !d->height || (d->width & 1) || (d->height & 1)
         || (d->fourcc != VA_FOURCC_NV12 && d->fourcc != VA_FOURCC_P010))
      return false;
   ten = d->fourcc == VA_FOURCC_P010;
   for (i = 0; i < d->num_objects; i++)
      if (d->objects[i].fd < 0)
         return false;
   for (i = 0; i < 2; i++)
   {
      uint32_t first = d->layers[i].object_index[0];
      uint32_t planes = d->layers[i].num_planes;
      uint32_t drm_format = ten
         ? (i ? VA_FOURCC('G','R','3','2') : VA_FOURCC('R','1','6',' '))
         : (i ? VA_FOURCC('G','R','8','8') : VA_FOURCC('R','8',' ',' '));
      VkFormat format = ten ? (i ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16_UNORM)
         : (i ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8_UNORM);
      bool disjoint = false;
      if (!planes || planes > 4 || first >= d->num_objects
            || d->layers[i].drm_format != drm_format)
         return false;
      for (j = 0; j < planes; j++)
      {
         uint32_t object = d->layers[i].object_index[j];
         if (object >= d->num_objects || !d->layers[i].pitch[j]
               || d->layers[i].offset[j] >= d->objects[object].size
               || d->objects[object].drm_format_modifier
                  != d->objects[first].drm_format_modifier)
            return false;
         if (object != first)
            disjoint = true;
      }
      if (!vulkan_prime_format_query(&layers[i], gpu, format,
               i ? d->width / 2 : d->width, i ? d->height / 2 : d->height,
               d->objects[first].drm_format_modifier, planes, disjoint,
               get_format, get_image))
         return false;
   }
   memcpy(out, layers, sizeof(layers));
   return true;
}
#endif
