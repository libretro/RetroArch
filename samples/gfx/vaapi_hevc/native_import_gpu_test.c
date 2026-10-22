/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Native import oracle checks require assertions
#endif
#define main synthetic_conversion_main
#include "prime_gpu_test.c"
#undef main
#include "../../../gfx/common/vaapi_frame.h"
#include <va/va.h>
#define import_device device
#define import_memory memory
#define import_queue queue
#define import_errors errors
#include "native_import_gpu.h"

static void check(unsigned depth, unsigned full, unsigned bt709)
{
   static vaapi_frame_view_t view;
   static VAImage image;
   static uint8_t data[256];
   static vaapi_hevc_export_t returned;
   memset(&frame, 0, sizeof(frame));
   memset(&view, 0, sizeof(view));
   memset(&image, 0, sizeof(image));
   make_image(depth == 10 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM, 8, 8,
         true, &frame.layers[0].image, &frame.layers[0].memory[0],
         &frame.layers[0].view);
   make_image(depth == 10 ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R8G8_UNORM, 4, 4,
         true, &frame.layers[1].image, &frame.layers[1].memory[0],
         &frame.layers[1].view);
   upload(depth, full);
   memcpy(data, mapped, sizeof(data));
#ifdef NATIVE_IMPORT_NEGATIVE_PIXEL
   data[(2 * 8 + 2) * (depth == 10 ? 2 : 1)] ^= 64;
#endif
   frame.active = frame.source.active = true;
   frame.device                       = device;
   frame.source.pool                  = &identity_only;
   frame.source.ticket                = 1;
   frame.source.descriptor.fourcc =
         depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   frame.source.descriptor.width = frame.source.descriptor.height = 8;
   frame.ops.destroy_view  = vkDestroyImageView;
   frame.ops.destroy_image = vkDestroyImage;
   frame.ops.free_memory   = vkFreeMemory;
   view.x = view.y     = 2;
   view.width          = 3;
   view.height         = 2;
   view.yuv            = bt709 ? IMAGE_YUV_FLAG_BT709 : 0;
   view.color.matrix   = bt709 ? 1 : 6;
   view.color.range    = full ? 2 : 1;
   view.color.transfer = 13;
   image.format.fourcc = frame.source.descriptor.fourcc;
   image.offsets[1]    = 128;
   image.pitches[0] = image.pitches[1] = depth == 10 ? 16 : 8;
   native_import_convert(&frame, &view, &image, data);
   assert(vaapi_hevc_vulkan_frame_retire(&frame, &returned));
   memset(&returned, 0, sizeof(returned));
}

int main(void)
{
   unsigned depth, full, bt709;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   (void)import_debug;
   initialize();
   import_family = family;
   for (depth = 8; depth <= 10; depth += 2)
      for (full = 0; full < 2; full++)
         for (bt709 = 0; bt709 < 2; bt709++)
            check(depth, full, bt709);
   assert(import_pixels == 48 && import_max_error <= 1 && !errors);
   vkUnmapMemory(device, buffer_memory);
   vkDestroyBuffer(device, buffer, NULL);
   vkFreeMemory(device, buffer_memory, NULL);
   vkDestroyFence(device, fence, NULL);
   vkDestroyCommandPool(device, command_pool, NULL);
   vkDestroyDevice(device, NULL);
   destroy_messenger =
         (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
               instance, "vkDestroyDebugUtilsMessengerEXT");
   assert(destroy_messenger);
   destroy_messenger(instance, messenger, NULL);
   vkDestroyInstance(instance, NULL);
   assert(!errors);
   puts("PASS: native import conversion/readback oracle, 8 synthetic NV12/P010 cases, 48 cropped pixels, zero validation errors; no VA modifier import executed");
   return 0;
}
