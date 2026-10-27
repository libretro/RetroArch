/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_android_pipeline.h"
#ifndef VULKAN_ANDROID_RGBA8_SHADER
#define VULKAN_ANDROID_RGBA8_SHADER "../../../gfx/drivers/vulkan_shaders/external_rgb_to_rgba8888.comp.inc"
#endif
#ifndef VULKAN_ANDROID_FP16_SHADER
#define VULKAN_ANDROID_FP16_SHADER "../../../gfx/drivers/vulkan_shaders/external_rgb_to_rgba16f.comp.inc"
#endif
static const uint32_t rgba8_shader[] =
#include VULKAN_ANDROID_RGBA8_SHADER
;
static const uint32_t fp16_shader[] =
#include VULKAN_ANDROID_FP16_SHADER
;
static VkDevice device;
static VkPhysicalDevice physical;
static VkPhysicalDeviceMemoryProperties memory;
static VkPhysicalDeviceProperties properties;
static VkQueue queue;
static VkCommandPool command_pool;
static VkCommandBuffer command;
static unsigned family;
static VkBuffer buffer;
static VkDeviceMemory buffer_memory;
static uint8_t *mapped;
static VkImage source, destination;
static VkDeviceMemory source_memory, destination_memory;
static VkImageView source_view, destination_view;
static VkSampler sampler;
static vulkan_android_pipeline_ops_t ops;
static uint32_t words[20];
#define OK(call) assert((call) == VK_SUCCESS)
static unsigned memory_type(uint32_t bits, VkMemoryPropertyFlags flags)
{
   unsigned i;
   for (i = 0; i < memory.memoryTypeCount; i++)
      if ((bits & ((uint32_t)1 << i))
            && (memory.memoryTypes[i].propertyFlags & flags) == flags)
         return i;
   assert(0);
   return 0;
}
static void make_image(VkFormat format, unsigned w, unsigned h,
      VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *allocation,
      VkImageView *view)
{
   VkImageCreateInfo info;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo vi;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format;
   info.extent.width = w;
   info.extent.height = h;
   info.extent.depth = 1;
   info.mipLevels = info.arrayLayers = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.usage = usage;
   OK(vkCreateImage(device, &info, NULL, image));
   vkGetImageMemoryRequirements(device, *image, &req);
   memset(&alloc, 0, sizeof(alloc));
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize = req.size;
   alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, 0);
   OK(vkAllocateMemory(device, &alloc, NULL, allocation));
   OK(vkBindImageMemory(device, *image, *allocation, 0));
   memset(&vi, 0, sizeof(vi));
   vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   vi.image = *image;
   vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
   vi.format = format;
   vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   vi.subresourceRange.levelCount = vi.subresourceRange.layerCount = 1;
   OK(vkCreateImageView(device, &vi, NULL, view));
}
static void transition(VkImage image, VkImageLayout old, VkImageLayout next,
      VkAccessFlags src, VkAccessFlags dst)
{
   VkImageMemoryBarrier b;
   memset(&b, 0, sizeof(b));
   b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.image = image;
   b.oldLayout = old;
   b.newLayout = next;
   b.srcAccessMask = src;
   b.dstAccessMask = dst;
   b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount = b.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}
static double half_value(uint16_t h)
{
   unsigned e = (h >> 10) & 31, m = h & 1023;
   double v = e ? ldexp(1.0 + (double)m / 1024, (int)e - 15)
                : ldexp((double)m, -24);
   assert(e != 31);
   return h & 32768 ? -v : v;
}
/* Independent double-precision ST2084/HLG and sRGB references, in scRGB units.
 */
static double reference(double x, unsigned mode)
{
   if (mode == 0)
      return x;
   if (mode == 1)
      return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
   if (mode == 2)
   {
      double p = pow(x, 32.0 / 2523.0);
      double base
            = (p - 3424.0 / 4096.0) / (2413.0 / 128.0 - 2392.0 / 128.0 * p);
      return base <= 0 ? 0 : 125.0 * pow(base, 16384.0 / 2610.0);
   }
   x = x <= 0.5 ? x * x / 3.0
                : (exp((x - 0.55991073) / 0.17883277) + 0.28466892) / 12.0;
   return 12.5 * pow(x, 1.2);
}
static double reference_601(unsigned x, unsigned y, unsigned c,
      bool fp16, bool srgb, bool pal)
{
   static const double to_xyz[2][3][3] = {
      {
         {0.393520903659, 0.365258076718, 0.191676946675},
         {0.212376360705, 0.701059856926, 0.086563782369},
         {0.018739090650, 0.111933926736, 0.958384733373}
      },
      {
         {0.430553813330, 0.341549803531, 0.178352310191},
         {0.222004309998, 0.706654765925, 0.071340924076},
         {0.020182210000, 0.129553373753, 0.939322167007}
      }
   };
   static const double from_xyz[3][3] = {
      {3.240969941905, -1.537383177570, -0.498610760293},
      {-0.969243636281, 1.875967501508, 0.041555057407},
      {0.055630079697, -0.203976958889, 1.056971514243}
   };
   double linear[3], xyz[3], value = 0.0;
   unsigned i, j;
   if (c == 3)
      return 1.0;
   for (i = 0; i < 3; i++)
   {
      double encoded = mapped[((y + 3) * 8 + x + 2) * 4 + i] / 255.0;
      linear[i] = srgb ? reference(encoded, 1)
            : encoded < 0.081 ? encoded / 4.5
            : pow((encoded + 0.099) / 1.099, 1.0 / 0.45);
   }
   for (i = 0; i < 3; i++)
   {
      xyz[i] = 0.0;
      for (j = 0; j < 3; j++)
         xyz[i] += to_xyz[pal ? 1 : 0][i][j] * linear[j];
   }
   for (i = 0; i < 3; i++)
      value += from_xyz[c][i] * xyz[i];
   if (fp16)
      return value;
   value = value < 0 ? 0 : value > 1 ? 1 : value;
   return value <= 0.0031308 ? value * 12.92
         : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}
static void execute(unsigned mode)
{
   vulkan_android_pipeline_t p;
   VkCommandBufferBeginInfo begin;
   VkBufferImageCopy copy;
   VkBufferMemoryBarrier host;
   VkSubmitInfo submit;
   VkFenceCreateInfo fi;
   VkFence fence;
   bool fp16 = mode < 4 ? mode != 0 : (mode & 1) != 0;
   unsigned x, y, c, bps = fp16 ? 8 : 4;
   float transfer = mode == 2 ? 16.0f
         : mode == 3          ? 18.0f
                              : 0.0f,
         linear = fp16 ? -1.0f : 0.0f;
   make_image(fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
         3, 2, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
         &destination, &destination_memory, &destination_view);
   assert(vulkan_android_pipeline_create(device, &ops, sampler, 1, 1,
         fp16 ? fp16_shader : rgba8_shader,
         fp16 ? sizeof(fp16_shader) : sizeof(rgba8_shader), &p));
   assert(vulkan_android_pipeline_update(
         device, &ops, &p, 0, source_view, destination_view));
   memset(words, 0, sizeof(words));
   words[0] = 3;
   words[1] = 2;
   words[2] = 2;
   words[3] = 3;
   words[4] = words[5] = 8;
   words[6] = 1;
   words[7] = mode == 1 ? 13 : (unsigned)transfer;
   if (mode >= 4)
   {
      words[6] = mode >= 8 ? 5 : 6;
      words[7] = mode % 4 >= 2 ? 13 : 6;
   }
   memcpy(&words[16], &transfer, 4);
   memcpy(&words[19], &linear, 4);
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   OK(vkResetCommandBuffer(command, 0));
   OK(vkBeginCommandBuffer(command, &begin));
   transition(source,
         mode ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
              : VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         mode ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT);
   memset(&copy, 0, sizeof(copy));
   copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1;
   copy.imageExtent.width = copy.imageExtent.height = 8;
   copy.imageExtent.depth = 1;
   vkCmdCopyBufferToImage(command, buffer, source,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
   transition(source, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT);
   transition(destination, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
         0, VK_ACCESS_SHADER_WRITE_BIT);
   vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
   vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout, 0,
         1, p.sets, 0, NULL);
   vkCmdPushConstants(command, p.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
         sizeof(words), words);
   vkCmdDispatch(command, 1, 1, 1);
   transition(destination, VK_IMAGE_LAYOUT_GENERAL,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT);
   copy.bufferOffset = 256;
   copy.imageExtent.width = 3;
   copy.imageExtent.height = 2;
   vkCmdCopyImageToBuffer(command, destination,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
   memset(&host, 0, sizeof(host));
   host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   host.buffer = buffer;
   host.offset = 256;
   host.size = 48;
   host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   host.srcQueueFamilyIndex = host.dstQueueFamilyIndex
         = VK_QUEUE_FAMILY_IGNORED;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host, 0, NULL);
   OK(vkEndCommandBuffer(command));
   memset(&fi, 0, sizeof(fi));
   fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   OK(vkCreateFence(device, &fi, NULL, &fence));
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &command;
   OK(vkQueueSubmit(queue, 1, &submit, fence));
   /* Test oracle only: readback/wait never belongs to the production helper. */
   OK(vkWaitForFences(device, 1, &fence, VK_TRUE, (uint64_t)5000000000.0));
   for (y = 0; y < 2; y++)
      for (x = 0; x < 3; x++)
         for (c = 0; c < 4; c++)
         {
            double expected = c == 3
                  ? 1
                  : reference(
                          mapped[((y + 3) * 8 + x + 2) * 4 + c] / 255.0, mode);
            double actual;
            if (mode >= 4)
               expected = reference_601(x, y, c, fp16, mode % 4 >= 2, mode >= 8);
            if (fp16)
            {
               uint16_t h;
               memcpy(&h, mapped + 256 + (y * 3 + x) * bps + c * 2, 2);
               actual = half_value(h);
            }
            else
               actual = mapped[256 + (y * 3 + x) * bps + c] / 255.0;
            assert(fabs(actual - expected) < 0.002 + fabs(expected) * 0.006);
         }
   vkDestroyFence(device, fence, NULL);
   vulkan_android_pipeline_destroy(device, &ops, &p);
   vkDestroyImageView(device, destination_view, NULL);
   vkDestroyImage(device, destination, NULL);
   vkFreeMemory(device, destination_memory, NULL);
   printf("PASS: executing Vulkan external RGB shader mode %u, cropped 3x2 "
          "pixels\n",
         mode);
}
int main(void)
{
   static VkQueueFamilyProperties families[32];
   VkInstanceCreateInfo ii;
   VkInstance instance;
   VkPhysicalDevice devices[16];
   uint32_t count = 16, n = 32;
   VkDeviceQueueCreateInfo qi;
   VkDeviceCreateInfo di;
   float priority = 1;
   VkCommandPoolCreateInfo pi;
   VkCommandBufferAllocateInfo ci;
   VkBufferCreateInfo bi;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo ai;
   VkSamplerCreateInfo si;
   unsigned i;
   memset(&ii, 0, sizeof(ii));
   ii.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   OK(vkCreateInstance(&ii, NULL, &instance));
   OK(vkEnumeratePhysicalDevices(instance, &count, devices));
   assert(count);
   physical = devices[0];
   vkGetPhysicalDeviceProperties(physical, &properties);
   printf("Vulkan RGB oracle device: %s\n", properties.deviceName);
   vkGetPhysicalDeviceQueueFamilyProperties(physical, &n, families);
   assert(n <= 32);
   for (family = 0; family < n; family++)
      if (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT)
         break;
   assert(family < n);
   vkGetPhysicalDeviceMemoryProperties(physical, &memory);
   memset(&qi, 0, sizeof(qi));
   qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   qi.queueFamilyIndex = family;
   qi.queueCount = 1;
   qi.pQueuePriorities = &priority;
   memset(&di, 0, sizeof(di));
   di.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   di.queueCreateInfoCount = 1;
   di.pQueueCreateInfos = &qi;
   OK(vkCreateDevice(physical, &di, NULL, &device));
   vkGetDeviceQueue(device, family, 0, &queue);
   memset(&pi, 0, sizeof(pi));
   pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pi.queueFamilyIndex = family;
   pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   OK(vkCreateCommandPool(device, &pi, NULL, &command_pool));
   memset(&ci, 0, sizeof(ci));
   ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   ci.commandPool = command_pool;
   ci.commandBufferCount = 1;
   OK(vkAllocateCommandBuffers(device, &ci, &command));
   memset(&bi, 0, sizeof(bi));
   bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bi.size = 512;
   bi.usage
         = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   OK(vkCreateBuffer(device, &bi, NULL, &buffer));
   vkGetBufferMemoryRequirements(device, buffer, &req);
   memset(&ai, 0, sizeof(ai));
   ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   ai.allocationSize = req.size;
   ai.memoryTypeIndex = memory_type(req.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   OK(vkAllocateMemory(device, &ai, NULL, &buffer_memory));
   OK(vkBindBufferMemory(device, buffer, buffer_memory, 0));
   OK(vkMapMemory(device, buffer_memory, 0, 512, 0, (void **)&mapped));
   for (i = 0; i < 64; i++)
   {
      mapped[i * 4] = (uint8_t)(i * 3);
      mapped[i * 4 + 1] = (uint8_t)(255 - i * 2);
      mapped[i * 4 + 2] = (uint8_t)(i * 4);
      mapped[i * 4 + 3] = 255;
   }
   {
      static const uint8_t pixels[6][3] = {
         {0, 0, 0}, {255, 255, 255}, {10, 20, 21},
         {22, 64, 192}, {255, 0, 0}, {0, 255, 255}
      };
      for (i = 0; i < 6; i++)
      {
         unsigned x = i % 3 + 2, y = i / 3 + 3;
         memcpy(mapped + (y * 8 + x) * 4, pixels[i], 3);
      }
   }
   make_image(VK_FORMAT_R8G8B8A8_UNORM, 8, 8,
         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &source,
         &source_memory, &source_view);
   memset(&si, 0, sizeof(si));
   si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   si.addressModeU = si.addressModeV = si.addressModeW
         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   OK(vkCreateSampler(device, &si, NULL, &sampler));
   ops.create_set_layout = vkCreateDescriptorSetLayout;
   ops.destroy_set_layout = vkDestroyDescriptorSetLayout;
   ops.create_layout = vkCreatePipelineLayout;
   ops.destroy_layout = vkDestroyPipelineLayout;
   ops.create_pool = vkCreateDescriptorPool;
   ops.destroy_pool = vkDestroyDescriptorPool;
   ops.allocate_sets = vkAllocateDescriptorSets;
   ops.update_sets = vkUpdateDescriptorSets;
   ops.create_shader = vkCreateShaderModule;
   ops.destroy_shader = vkDestroyShaderModule;
   ops.create_pipeline = vkCreateComputePipelines;
   ops.destroy_pipeline = vkDestroyPipeline;
   for (i = 0; i < 12; i++)
      execute(i);
   vkDestroySampler(device, sampler, NULL);
   vkDestroyImageView(device, source_view, NULL);
   vkDestroyImage(device, source, NULL);
   vkFreeMemory(device, source_memory, NULL);
   vkUnmapMemory(device, buffer_memory);
   vkDestroyBuffer(device, buffer, NULL);
   vkFreeMemory(device, buffer_memory, NULL);
   vkDestroyCommandPool(device, command_pool, NULL);
   vkDestroyDevice(device, NULL);
   vkDestroyInstance(instance, NULL);
   return 0;
}
