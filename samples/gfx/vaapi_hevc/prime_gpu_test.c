/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_vulkan_submit.h"
#include "../d3d_p010_import/fp16_reference.h"
#define OK(call) assert((call) == VK_SUCCESS)
static const uint32_t rgba_shader[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc"
;
static const uint32_t fp16_shader[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc"
;
static VkInstance instance;
static VkDebugUtilsMessengerEXT messenger;
static VkDevice device;
static VkPhysicalDevice physical;
static VkPhysicalDeviceMemoryProperties memory;
static VkQueue queue;
static unsigned family, errors, submitted;
static unsigned checked_channels, max_byte_error;
static double max_half_error, max_half_relative;
static VkCommandPool command_pool;
static VkCommandBuffer command;
static VkFence fence;
static VkBuffer buffer;
static VkDeviceMemory buffer_memory;
static uint8_t *mapped;
static VkSampler sampler;
static VkDescriptorSetLayout set_layout;
static VkPipelineLayout pipeline_layout;
static VkDescriptorPool descriptor_pool;
static VkDescriptorSet descriptor_set;
static VkPipeline pipeline;
static vaapi_hevc_vulkan_frame_t frame;
static vaapi_hevc_vulkan_submission_t entry;
static vaapi_hevc_pool_t identity_only;
static VkImage destination;
static VkDeviceMemory destination_memory;
static VkImageView destination_view;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *message, void *user)
{
   (void)type; (void)user;
   if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      fprintf(stderr, "%s\n", message->pMessage); errors++;
   }
   return VK_FALSE;
}
static unsigned find_memory(uint32_t bits, VkMemoryPropertyFlags flags)
{
   unsigned i;
   for (i = 0; i < memory.memoryTypeCount; i++)
      if ((bits & ((uint32_t)1 << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
   assert(0); return 0;
}
static void make_image(VkFormat format, unsigned width, unsigned height, bool external,
      VkImage *image, VkDeviceMemory *allocation, VkImageView *view)
{
   VkImageCreateInfo info;
   VkExternalMemoryImageCreateInfo handles;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   VkExportMemoryAllocateInfo export_info;
   VkMemoryDedicatedAllocateInfo dedicated;
   VkImageViewCreateInfo vi;
   VkPhysicalDeviceImageFormatInfo2 query;
   VkPhysicalDeviceExternalImageFormatInfo external_query;
   VkImageFormatProperties2 properties;
   VkExternalImageFormatProperties external_properties;
   memset(&info, 0, sizeof(info)); memset(&handles, 0, sizeof(handles));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format; info.extent.width = width; info.extent.height = height; info.extent.depth = 1;
   info.mipLevels = info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.usage = external ? VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      : VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   if (external)
   {
      memset(&query, 0, sizeof(query)); memset(&external_query, 0, sizeof(external_query));
      memset(&properties, 0, sizeof(properties)); memset(&external_properties, 0, sizeof(external_properties));
      query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
      query.format = format; query.type = VK_IMAGE_TYPE_2D; query.tiling = VK_IMAGE_TILING_OPTIMAL;
      query.usage = info.usage; query.pNext = &external_query;
      external_query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
      external_query.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2; properties.pNext = &external_properties;
      external_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
      OK(vkGetPhysicalDeviceImageFormatProperties2(physical, &query, &properties));
      assert(external_properties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT);
      handles.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
      handles.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT; info.pNext = &handles;
   }
   OK(vkCreateImage(device, &info, NULL, image)); vkGetImageMemoryRequirements(device, *image, &req);
   memset(&alloc, 0, sizeof(alloc)); memset(&export_info, 0, sizeof(export_info)); memset(&dedicated, 0, sizeof(dedicated));
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; alloc.allocationSize = req.size;
   alloc.memoryTypeIndex = find_memory(req.memoryTypeBits, 0);
   if (external)
   {
      export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
      export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
      dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO; dedicated.image = *image;
      dedicated.pNext = &export_info; alloc.pNext = &dedicated;
   }
   OK(vkAllocateMemory(device, &alloc, NULL, allocation)); OK(vkBindImageMemory(device, *image, *allocation, 0));
   memset(&vi, 0, sizeof(vi)); vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   vi.image = *image; vi.format = format; vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
   vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount = vi.subresourceRange.layerCount = 1;
   OK(vkCreateImageView(device, &vi, NULL, view));
}
static void transition(VkImage image, VkImageLayout old, VkImageLayout next,
      VkAccessFlags src, VkAccessFlags dst, unsigned source_family, unsigned destination_family)
{
   VkImageMemoryBarrier info;
   memset(&info, 0, sizeof(info)); info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   info.image = image; info.oldLayout = old; info.newLayout = next; info.srcAccessMask = src; info.dstAccessMask = dst;
   info.srcQueueFamilyIndex = source_family; info.dstQueueFamilyIndex = destination_family;
   info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; info.subresourceRange.levelCount = info.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         0, 0, NULL, 0, NULL, 1, &info);
}
static void begin_test(void)
{
   VkCommandBufferBeginInfo begin;
   memset(&begin, 0, sizeof(begin)); begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   OK(vkResetCommandBuffer(command, 0)); OK(vkBeginCommandBuffer(command, &begin));
}
static void submit_test(void)
{
   VkSubmitInfo info;
   memset(&info, 0, sizeof(info)); info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.commandBufferCount = 1; info.pCommandBuffers = &command;
   OK(vkEndCommandBuffer(command)); OK(vkResetFences(device, 1, &fence)); OK(vkQueueSubmit(queue, 1, &info, fence));
   /* Native oracle only; production submission has no wait or readback. */
   OK(vkWaitForFences(device, 1, &fence, VK_TRUE, (uint64_t)5000000000.0));
}
static unsigned sample(unsigned plane, unsigned x, unsigned y, unsigned depth, unsigned full)
{
   unsigned shift = depth == 10 ? 2 : 0;
   if (!plane)
   {
      if (x == 2 && y == 2) return full ? 0 : 16u << shift;
      if (x == 3 && y == 2) return full ? (1u << depth)-1 : 235u << shift;
      return (64 + (x*17 + y*13)%150) << shift;
   }
   if (x == 1 && y == 1) return 128u << shift;
   return (96 + (x*11 + y*13 + plane*7)%64) << shift;
}
static void upload(unsigned depth, unsigned full)
{
   unsigned x, y, p;
   VkBufferImageCopy copy;
   for (p = 0; p < 2; p++)
      for (y = 0; y < (p ? 4u : 8u); y++)
         for (x = 0; x < (p ? 4u : 8u); x++)
         {
            unsigned c;
            for (c = 0; c < (p ? 2u : 1u); c++)
            {
               unsigned offset = p ? 128 + (y*4+x)*2+c : y*8+x;
               uint16_t value = (uint16_t)sample(p ? c+1 : 0, x, y, depth, full);
               if (depth == 10)
               {
                  offset = p ? 128 + ((y*4+x)*2+c)*2 : (y*8+x)*2;
                  value = (uint16_t)(value << 6); memcpy(mapped+offset, &value, 2);
               }
               else mapped[offset] = (uint8_t)value;
            }
         }
   begin_test();
   for (p = 0; p < 2; p++)
   {
      transition(frame.layers[p].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
      memset(&copy, 0, sizeof(copy)); copy.bufferOffset = p ? 128 : 0;
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; copy.imageSubresource.layerCount = 1;
      copy.imageExtent.width = copy.imageExtent.height = p ? 4 : 8; copy.imageExtent.depth = 1;
      vkCmdCopyBufferToImage(command, buffer, frame.layers[p].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      transition(frame.layers[p].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, 0, family, VK_QUEUE_FAMILY_FOREIGN_EXT);
   }
   submit_test();
}
static void make_pipeline(bool fp16)
{
   VkDescriptorSetLayoutBinding bindings[3];
   VkDescriptorSetLayoutCreateInfo set;
   VkPipelineLayoutCreateInfo layout;
   VkPushConstantRange push;
   VkDescriptorPoolSize sizes[2];
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetAllocateInfo allocation;
   VkShaderModuleCreateInfo shader_info;
   VkComputePipelineCreateInfo compute;
   VkShaderModule shader;
   unsigned i;
   memset(bindings, 0, sizeof(bindings));
   for (i = 0; i < 3; i++)
   {
      bindings[i].binding = i+1; bindings[i].descriptorCount = 1; bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   }
   memset(&set, 0, sizeof(set)); set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set.bindingCount = 3; set.pBindings = bindings; OK(vkCreateDescriptorSetLayout(device, &set, NULL, &set_layout));
   memset(&push, 0, sizeof(push)); push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; push.size = 80;
   memset(&layout, 0, sizeof(layout)); layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout.setLayoutCount = 1; layout.pSetLayouts = &set_layout; layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
   OK(vkCreatePipelineLayout(device, &layout, NULL, &pipeline_layout));
   memset(sizes, 0, sizeof(sizes)); sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].descriptorCount = 2;
   sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[1].descriptorCount = 1;
   memset(&pool, 0, sizeof(pool)); pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.maxSets = 1; pool.poolSizeCount = 2; pool.pPoolSizes = sizes; OK(vkCreateDescriptorPool(device, &pool, NULL, &descriptor_pool));
   memset(&allocation, 0, sizeof(allocation)); allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocation.descriptorPool = descriptor_pool; allocation.descriptorSetCount = 1; allocation.pSetLayouts = &set_layout;
   OK(vkAllocateDescriptorSets(device, &allocation, &descriptor_set));
   memset(&shader_info, 0, sizeof(shader_info)); shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader_info.pCode = fp16 ? fp16_shader : rgba_shader; shader_info.codeSize = fp16 ? sizeof(fp16_shader) : sizeof(rgba_shader);
   OK(vkCreateShaderModule(device, &shader_info, NULL, &shader));
   memset(&compute, 0, sizeof(compute)); compute.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   compute.layout = pipeline_layout; compute.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   compute.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; compute.stage.module = shader; compute.stage.pName = "main";
   OK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute, NULL, &pipeline)); vkDestroyShaderModule(device, shader, NULL);
}
static VkResult serialized_submit(void *context, VkQueue target, uint32_t count, const VkSubmitInfo *info, VkFence done)
{
   assert(context == &entry && target == queue && count == 1 && frame.pending); submitted++;
   return vkQueueSubmit(target, count, info, done);
}
static void readback(VkImageLayout next)
{
   VkBufferImageCopy copy;
   VkBufferMemoryBarrier host;
   begin_test();
   transition(destination, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
   memset(&copy, 0, sizeof(copy)); copy.bufferOffset = 512; copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1; copy.imageExtent.width = 3; copy.imageExtent.height = 2; copy.imageExtent.depth = 1;
   vkCmdCopyImageToBuffer(command, destination, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
   transition(destination, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, next, VK_ACCESS_TRANSFER_READ_BIT,
         next == VK_IMAGE_LAYOUT_GENERAL ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
         VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
   memset(&host, 0, sizeof(host)); host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   host.buffer = buffer; host.offset = 512; host.size = 48; host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host, 0, NULL);
   submit_test();
}
static double hable(double x)
{ return ((x*(.15*x+.05)+.004)/(x*(.15*x+.5)+.06))-.02/.30; }
static double light(double x, unsigned transfer)
{
   if (transfer == 16)
   {
      double p = pow(x, 32.0/2523.0), n = p > 3424.0/4096.0 ? p-3424.0/4096.0 : 0;
      return pow(n/(2413.0/128.0-2392.0/128.0*p), 16384.0/2610.0)*(10000.0/203.0);
   }
   x = x <= .5 ? x*x/3 : (exp((x-.55991073)/.17883277)+.28466892)/12;
   return pow(x, 1.2)*(1000.0/203.0);
}
static void check_pixels(unsigned depth, unsigned full, unsigned transfer, unsigned flags, bool fp16)
{
   unsigned x, y, c;
   for (y = 0; y < 2; y++) for (x = 0; x < 3; x++)
   {
      unsigned yy = sample(0, x+2, y+2, depth, full), u = sample(1, (x+2)/2, (y+2)/2, depth, full);
      unsigned v = sample(2, (x+2)/2, (y+2)/2, depth, full);
      float expected[3];
      if (fp16)
      {
         uint16_t pixel[4]; memcpy(pixel, mapped+512+(y*3+x)*8, sizeof(pixel));
         fp16_reference(depth, full, transfer, flags, yy, u, v, expected);
         assert(fp16_reference_check(pixel, expected));
         for (c = 0; c < 4; c++)
         {
            double ref = c == 3 ? 1 : expected[c];
            double error = fabs(fp16_reference_half(pixel[c])-ref);
            double relative = error/(.001+fabs(ref));
            if (error > max_half_error) max_half_error = error;
            if (relative > max_half_relative) max_half_relative = relative;
            checked_channels++;
         }
      }
      else
      {
         double scale = depth == 10 ? 4 : 1;
         double ym = full ? (depth == 10 ? 1023 : 255) : 219*scale;
         double cm = full ? ym : 224*scale;
         double luma = (yy-(full ? 0 : 16*scale))/ym, cb = (u-128*scale)/cm, cr = (v-128*scale)/cm;
         double rgb[3];
         bool hdr = transfer == 16 || transfer == 18;
         rgb[0] = luma+(hdr ? 1.4746 : flags ? 1.5748 : 1.402)*cr;
         rgb[1] = luma-(hdr ? .16455313 : flags ? .187324 : .344136)*cb-(hdr ? .57135313 : flags ? .468124 : .714136)*cr;
         rgb[2] = luma+(hdr ? 1.8814 : flags ? 1.8556 : 1.772)*cb;
         if (hdr)
         {
            double lo = 1, hi = 8, gain = 2, mixed[3]; unsigned j;
            for (j = 0; j < 48; j++) { gain = (lo+hi)/2; if (hable(gain)/hable(gain*1000/203)<.8) lo=gain; else hi=gain; }
            for (c = 0; c < 3; c++)
            {
               double value = rgb[c] < 0 ? 0 : rgb[c] > 1 ? 1 : rgb[c];
               rgb[c] = hable(gain*light(value, transfer))/hable(gain*1000/203);
               if (rgb[c] > 1) rgb[c] = 1;
            }
            mixed[0]=(6801*rgb[0]-2407*rgb[1]-298*rgb[2])/4096;
            mixed[1]=(-510*rgb[0]+4640*rgb[1]-34*rgb[2])/4096;
            mixed[2]=(-75*rgb[0]-412*rgb[1]+4583*rgb[2])/4096;
            for (c = 0; c < 3; c++) rgb[c] = mixed[c] <= .0031308 ? 12.92*mixed[c] : 1.055*pow(mixed[c], 1/2.4)-.055;
         }
         for (c = 0; c < 4; c++)
         {
            double value = c == 3 ? 1 : rgb[c] < 0 ? 0 : rgb[c] > 1 ? 1 : rgb[c];
            int expected_byte = (int)floor(value*255+.5), actual = mapped[512+(y*3+x)*4+c];
            unsigned difference = (unsigned)abs(actual-expected_byte);
            if (difference > max_byte_error) max_byte_error = difference;
            checked_channels++;
            if (abs(actual-expected_byte) > (hdr ? 3 : 1)) fprintf(stderr, "depth=%u full=%u transfer=%u pixel=%u,%u/%u actual=%d expected=%d\n",depth,full,transfer,x,y,c,actual,expected_byte);
            assert(abs(actual-expected_byte) <= (hdr ? 3 : 1));
         }
      }
   }
}
static void execute(unsigned depth, unsigned full, unsigned mode)
{
   static video_color_metadata_t color;
   static vaapi_hevc_export_t returned;
   VkImageLayout tracked = VK_IMAGE_LAYOUT_UNDEFINED;
   unsigned transfer = mode == 2 || mode == 4 ? 16 : mode == 3 || mode == 5 ? 18 : 13;
   unsigned flags = mode >= 6 ? 0 : IMAGE_YUV_FLAG_BT709;
   bool fp16 = mode == 1 || mode == 2 || mode == 3 || mode == 7;
   unsigned i;
   memset(&frame, 0, sizeof(frame)); memset(&entry, 0, sizeof(entry)); memset(&returned, 0, sizeof(returned));
   make_image(depth == 10 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM, 8, 8, true, &frame.layers[0].image, &frame.layers[0].memory[0], &frame.layers[0].view);
   make_image(depth == 10 ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R8G8_UNORM, 4, 4, true, &frame.layers[1].image, &frame.layers[1].memory[0], &frame.layers[1].view);
   make_image(fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM, 3, 2, false, &destination, &destination_memory, &destination_view);
   make_pipeline(fp16); upload(depth, full);
   /* Synthetic ticket metadata, actual exportable Vulkan plane payloads. No VA
    * display/FD import is fabricated or counted as a native VA device pass. */
   frame.active = frame.source.active = true; frame.device = device; frame.source.pool = &identity_only; frame.source.ticket = 1;
   frame.source.descriptor.fourcc = depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   frame.source.descriptor.width = frame.source.descriptor.height = 8;
   frame.ops.destroy_view = vkDestroyImageView; frame.ops.destroy_image = vkDestroyImage; frame.ops.free_memory = vkFreeMemory;
   memset(&color, 0, sizeof(color)); color.primaries = transfer == 16 || transfer == 18 ? 9 : 1;
   color.matrix = transfer == 16 || transfer == 18 ? 9 : flags ? 1 : 6;
   color.transfer = transfer; color.range = full ? 2 : 1; color.max_cll = 1000;
   assert(vaapi_hevc_vulkan_submission_configure(&entry, &frame, 2, 2, 3, 2, flags, &color, fp16));
#ifdef PRIME_GPU_NEGATIVE_CROP
   entry.header[2] = 0;
#endif
   entry.queue=queue; entry.command=command; entry.fence=fence; entry.pipeline=pipeline; entry.layout=pipeline_layout;
   entry.set=descriptor_set; entry.sampler=sampler; entry.family=family;
   entry.ops.fence_status=vkGetFenceStatus; entry.ops.reset_fences=vkResetFences; entry.ops.reset_command=vkResetCommandBuffer;
   entry.ops.begin=vkBeginCommandBuffer; entry.ops.end=vkEndCommandBuffer; entry.ops.update=vkUpdateDescriptorSets;
   entry.ops.barrier=vkCmdPipelineBarrier; entry.ops.bind_pipeline=vkCmdBindPipeline; entry.ops.bind_sets=vkCmdBindDescriptorSets;
   entry.ops.push=vkCmdPushConstants; entry.ops.dispatch=vkCmdDispatch; entry.ops.submit=serialized_submit; entry.ops.context=&entry;
   for (i = 0; i < 3; i++)
   {
      assert(vaapi_hevc_vulkan_submission_present(&entry, destination, destination_view, entry.output_format, 3, 2, tracked) == 1);
      assert(!vaapi_hevc_vulkan_frame_retire(&frame, &returned));
      OK(vkWaitForFences(device, 1, &fence, VK_TRUE, (uint64_t)5000000000.0));
      assert(vaapi_hevc_vulkan_frame_poll(&frame, vkGetFenceStatus) == 1);
      tracked = i ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
      readback(tracked); check_pixels(depth, full, transfer, flags, fp16); assert(!errors);
   }
   vkDestroyDescriptorPool(device,descriptor_pool,NULL); vkDestroyPipeline(device,pipeline,NULL);
   vkDestroyPipelineLayout(device,pipeline_layout,NULL); vkDestroyDescriptorSetLayout(device,set_layout,NULL);
   assert(vaapi_hevc_vulkan_frame_retire(&frame,&returned)); memset(&returned,0,sizeof(returned));
   vkDestroyImageView(device,destination_view,NULL); vkDestroyImage(device,destination,NULL); vkFreeMemory(device,destination_memory,NULL);
   printf("PASS: native exportable-plane submission depth=%u full=%u mode=%u, 3 cropped/layout/foreign/fence rounds; not VA DMA-BUF import\n",depth,full,mode);
}
static void initialize(void)
{
   static VkPhysicalDevice devices[16];
   static VkQueueFamilyProperties families[32];
   static VkPhysicalDeviceProperties properties;
   const char *extensions[] = { VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME };
   const char *layer = "VK_LAYER_KHRONOS_validation", *debug = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
   VkApplicationInfo app;
   VkInstanceCreateInfo info;
   VkDebugUtilsMessengerCreateInfoEXT messages;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   VkDeviceQueueCreateInfo qi;
   VkDeviceCreateInfo di;
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo command_info;
   VkFenceCreateInfo fi;
   VkBufferCreateInfo bi;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo ai;
   VkSamplerCreateInfo si;
   uint32_t count=16, n=32, i;
   float priority=1;
   memset(&app,0,sizeof(app));app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;app.apiVersion=VK_API_VERSION_1_1;
   memset(&messages,0,sizeof(messages));messages.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   messages.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   messages.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   messages.pfnUserCallback=debug_message;
   memset(&info,0,sizeof(info));info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;info.pApplicationInfo=&app;
   info.pNext=&messages;info.enabledLayerCount=1;info.ppEnabledLayerNames=&layer;info.enabledExtensionCount=1;info.ppEnabledExtensionNames=&debug;
   OK(vkCreateInstance(&info,NULL,&instance));create_messenger=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
   assert(create_messenger);OK(create_messenger(instance,&messages,NULL,&messenger));
   OK(vkEnumeratePhysicalDevices(instance,&count,devices));assert(count && count<=16);physical=devices[0];
   for(i=0;i<count;i++){vkGetPhysicalDeviceProperties(devices[i],&properties);if(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU){physical=devices[i];break;}}
   vkGetPhysicalDeviceProperties(physical,&properties);printf("Native conversion oracle device: %s\n",properties.deviceName);
   vkGetPhysicalDeviceMemoryProperties(physical,&memory);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families);assert(n<=32);
   for(family=0;family<n;family++)if((families[family].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))break;
   assert(family<n);memset(&qi,0,sizeof(qi));qi.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
   memset(&di,0,sizeof(di));di.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
   di.enabledExtensionCount=2;di.ppEnabledExtensionNames=extensions;OK(vkCreateDevice(physical,&di,NULL,&device));vkGetDeviceQueue(device,family,0,&queue);
   memset(&pool,0,sizeof(pool));pool.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=family;
   OK(vkCreateCommandPool(device,&pool,NULL,&command_pool));memset(&command_info,0,sizeof(command_info));command_info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command_info.commandPool=command_pool;command_info.commandBufferCount=1;OK(vkAllocateCommandBuffers(device,&command_info,&command));
   memset(&fi,0,sizeof(fi));fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;OK(vkCreateFence(device,&fi,NULL,&fence));
   memset(&bi,0,sizeof(bi));bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;bi.size=1024;bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   OK(vkCreateBuffer(device,&bi,NULL,&buffer));vkGetBufferMemoryRequirements(device,buffer,&req);
   memset(&ai,0,sizeof(ai));ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=req.size;
   ai.memoryTypeIndex=find_memory(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   OK(vkAllocateMemory(device,&ai,NULL,&buffer_memory));OK(vkBindBufferMemory(device,buffer,buffer_memory,0));OK(vkMapMemory(device,buffer_memory,0,1024,0,(void**)&mapped));
   memset(&si,0,sizeof(si));si.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;OK(vkCreateSampler(device,&si,NULL,&sampler));
}
int main(void)
{
   unsigned depth, full, mode;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   initialize();
   for(depth=8;depth<=10;depth+=2)for(full=0;full<2;full++)for(mode=0;mode<8;mode++)execute(depth,full,mode);
   assert(submitted==96 && checked_channels==2304 && !errors);
   vkDestroySampler(device,sampler,NULL);vkUnmapMemory(device,buffer_memory);vkDestroyBuffer(device,buffer,NULL);vkFreeMemory(device,buffer_memory,NULL);
   vkDestroyFence(device,fence,NULL);vkDestroyCommandPool(device,command_pool,NULL);vkDestroyDevice(device,NULL);
   destroy_messenger=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
   assert(destroy_messenger);destroy_messenger(instance,messenger,NULL);vkDestroyInstance(instance,NULL);assert(!errors);
   printf("Pixel maxima over %u channels: RGBA8 %u codes; FP16 absolute %.8g relative %.8g\n",
         checked_channels,max_byte_error,max_half_error,max_half_relative);
   puts("PASS: 32 real Vulkan conversion cases, 96 foreign-ownership/fence rounds, zero validation errors; no native VA/DRM-modifier import executed");return 0;
}
