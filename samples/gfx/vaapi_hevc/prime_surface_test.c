/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main format_unit_main
#include "prime_format_test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_vulkan.h"
static vaapi_hevc_export_t surface;
static unsigned layer, fail_uv;
static VKAPI_ATTR void VKAPI_CALL surface_format(VkPhysicalDevice gpu,
      VkFormat format, VkFormatProperties2 *properties)
{
   bool ten = surface.descriptor.fourcc == VA_FOURCC_P010;
   assert(layer < 2);
   expected_format = ten ? (layer ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16_UNORM)
      : (layer ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8_UNORM);
   format_properties(gpu, format, properties);
}
static VKAPI_ATTR VkResult VKAPI_CALL surface_image(VkPhysicalDevice gpu,
      const VkPhysicalDeviceImageFormatInfo2 *info, VkImageFormatProperties2 *out)
{
   VkResult status;
   if (fail_uv && layer == 1) return VK_ERROR_FORMAT_NOT_SUPPORTED;
   status = image_properties(gpu, info, out);
   layer++;
   return status;
}
static bool surface_query(vulkan_prime_format_t out[2])
{
   layer = 0;
   return vaapi_hevc_vulkan_query(out, &surface, (VkPhysicalDevice)(uintptr_t)1,
         true, surface_format, surface_image);
}
static void setup(unsigned ten, unsigned separate)
{
   VADRMPRIMESurfaceDescriptor *d = &surface.descriptor;
   unsigned i, j;
   memset(&surface, 0, sizeof(surface));
   surface.active = surface.ready = true;
   d->fourcc = ten ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   d->width = 64; d->height = 32; d->num_layers = 2;
   d->num_objects = separate ? 4 : 1;
   expected_planes = separate ? 2 : 1;
   expected_disjoint = !!separate;
   expected_modifier = separate ? ((uint64_t)1 << 56) : 0;
   for (i = 0; i < d->num_objects; i++)
   {
      d->objects[i].fd = (int)i;
      d->objects[i].size = 8192;
      d->objects[i].drm_format_modifier = expected_modifier;
   }
   for (i = 0; i < 2; i++)
   {
      d->layers[i].drm_format = ten
         ? (i ? VA_FOURCC('G','R','3','2') : VA_FOURCC('R','1','6',' '))
         : (i ? VA_FOURCC('G','R','8','8') : VA_FOURCC('R','8',' ',' '));
      d->layers[i].num_planes = expected_planes;
      for (j = 0; j < expected_planes; j++)
      {
         d->layers[i].object_index[j] = separate ? 2*i+j : 0;
         d->layers[i].offset[j] = separate ? 0 : i*4096;
         d->layers[i].pitch[j] = ten ? 128 : 64;
      }
   }
   fault = fail_uv = 0;
}
int main(void)
{
   vulkan_prime_format_t out[2], before[2];
   vaapi_hevc_export_t saved;
   unsigned ten, separate, bad;
   assert(!format_unit_main());
   memset(before, 0x5a, sizeof(before));
   for (ten = 0; ten < 2; ten++)
      for (separate = 0; separate < 2; separate++)
      {
         setup(ten, separate); saved = surface;
         assert(surface_query(out) && layer == 2);
         assert(out[0].width == 64 && out[0].height == 32
               && out[1].width == 32 && out[1].height == 16
               && out[0].memory_planes == expected_planes
               && out[1].memory_planes == expected_planes);
         assert(!memcmp(&saved, &surface, sizeof(saved)));
         memcpy(out, before, sizeof(out)); fail_uv = 1;
         assert(!surface_query(out) && !memcmp(out, before, sizeof(out))
               && !memcmp(&saved, &surface, sizeof(saved)));
      }
   for (bad = 0; bad < 16; bad++)
   {
      setup(0, 1);
      switch (bad)
      {
         case 0: surface.active = false; break;
         case 1: surface.ready = false; break;
         case 2: surface.descriptor.num_layers = 3; break;
         case 3: surface.descriptor.num_objects = 5; break;
         case 4: surface.descriptor.width = 0; break;
         case 5: surface.descriptor.height = 31; break;
         case 6: surface.descriptor.fourcc = 0; break;
         case 7: surface.descriptor.objects[3].fd = -1; break;
         case 8: surface.descriptor.layers[1].num_planes = 5; break;
         case 9: surface.descriptor.layers[1].drm_format = 0; break;
         case 10: surface.descriptor.layers[1].object_index[1] = 4; break;
         case 11: surface.descriptor.layers[1].pitch[1] = 0; break;
         case 12: surface.descriptor.layers[1].offset[1] = 8192; break;
         case 13: surface.descriptor.objects[3].drm_format_modifier++; break;
         case 14: surface.descriptor.layers[0].num_planes = 0; break;
         case 15: fault = 13; break;
      }
      saved = surface; memcpy(out, before, sizeof(out));
      assert(!surface_query(out) && !memcmp(out, before, sizeof(out))
            && !memcmp(&surface, &saved, sizeof(saved)));
   }
   setup(0, 0);
   assert(!vaapi_hevc_vulkan_query(out, &surface, (VkPhysicalDevice)(uintptr_t)1,
         false, surface_format, surface_image));
   assert(!vaapi_hevc_vulkan_query(NULL, &surface, (VkPhysicalDevice)(uintptr_t)1,
         true, surface_format, surface_image));
   assert(!vaapi_hevc_vulkan_query(out, NULL, (VkPhysicalDevice)(uintptr_t)1,
         true, surface_format, surface_image));
   puts("PASS: both VA NV12/P010 sampled layers admitted atomically before FD transfer; no native import");
   return 0;
}
