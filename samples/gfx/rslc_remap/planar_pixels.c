#ifdef NDEBUG
#error Planar pixels require assertions
#endif
/* Execute rslc's planar sampler as SPIR-V on a real Vulkan device. */
#define main rgb_oracle_unused_main
#include "../mediacodec_session/vulkan_rgb_gpu_test.c"
#undef main
#include <stdlib.h>
#include <gfx/shaders/rslc.h>
#include <formats/image_yuv_blit.h>
#include "../../../gfx/common/hdr_viewer_colour.h"
static unsigned output_w = 9, output_h = 7, repeats = 1;
static double gpu_usec;
struct mapped_input
{
   VkImage image;
   VkImageView view;
   VkDeviceMemory allocation;
   void *map;
   VkSubresourceLayout pitch;
};
static void input_create(struct mapped_input *p, VkFormat format, unsigned w, unsigned h)
{
   VkImageCreateInfo info;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo view;
   VkImageSubresource sub;
   unsigned type;
   memset(p, 0, sizeof(*p));
   memset(&info, 0, sizeof(info));
   memset(&alloc, 0, sizeof(alloc));
   memset(&view, 0, sizeof(view));
   memset(&sub, 0, sizeof(sub));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format;
   info.extent.width = w;
   info.extent.height = h;
   info.extent.depth = 1;
   info.mipLevels = info.arrayLayers = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.tiling = VK_IMAGE_TILING_LINEAR;
   info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
   OK(vkCreateImage(device, &info, NULL, &p->image));
   vkGetImageMemoryRequirements(device, p->image, &req);
   type = memory_type(req.memoryTypeBits,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize = req.size;
   alloc.memoryTypeIndex = type;
   OK(vkAllocateMemory(device, &alloc, NULL, &p->allocation));
   OK(vkBindImageMemory(device, p->image, p->allocation, 0));
   OK(vkMapMemory(device, p->allocation, 0, VK_WHOLE_SIZE, 0, &p->map));
   sub.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   vkGetImageSubresourceLayout(device, p->image, &sub, &p->pitch);
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = p->image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   OK(vkCreateImageView(device, &view, NULL, &p->view));
}
static void input_free(struct mapped_input *p)
{
   vkUnmapMemory(device, p->allocation);
   vkDestroyImageView(device, p->view, NULL);
   vkDestroyImage(device, p->image, NULL);
   vkFreeMemory(device, p->allocation, NULL);
}
static void render(struct mapped_input *p, unsigned w, unsigned h, bool linear, bool hdr,
                   bool planar, float *result)
{
   struct rslc_opts options;
   struct rslc_remap remap;
   struct rslc_result compiled;
   static const char *const names[] = {"Source"};
   char text[2048], error[256];
   float colour[12];
   vulkan_android_pipeline_t pipeline;
   VkCommandBufferBeginInfo begin;
   VkBufferImageCopy copy;
   VkBufferMemoryBarrier host;
   VkFenceCreateInfo fi;
   VkFence fence;
   VkSubmitInfo submit;
   VkWriteDescriptorSet write;
   VkDescriptorImageInfo image;
   unsigned i;
   VkQueryPool queries = VK_NULL_HANDLE;
   VkQueryPoolCreateInfo query_info;
   uint64_t timestamps[2];
   VkMemoryBarrier again;
   snprintf(text, sizeof(text),
            "#version 450\nlayout(local_size_x=8,local_size_y=8) in;\n"
            "layout(set=0,binding=1) uniform sampler2D Source;\n"
            "layout(set=0,binding=3,%s) writeonly uniform image2D Destination;\n"
            "vec4 read_it(sampler2D s,vec2 p){return texture(s,p);}\n"
            "void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);if(p.x>=%u||p.y>=%u)return;vec2 "
            "uv=(vec2(p)+.5)/vec2(%u,%u)*1.4-.2;"
            "vec2 d=vec2(.6)/vec2(%u,%u);vec4 "
            "c=read_it(Source,uv)*.5+read_it(Source,uv+d)*.25+read_it(Source,uv-d)*.25;"
            "c=c*.75+texelFetch(Source,clamp(ivec2(uv*vec2(textureSize(Source,0))),"
            "ivec2(0),textureSize(Source,0)-1),0)*.25;"
            "c.rb=c.rb*1.5+vec2(-.25,1.0);imageStore(Destination,p,c);}",
            hdr ? "rgba16f" : "rgba8", output_w, output_h, output_w, output_h, w, h);
   memset(&options, 0, sizeof(options));
   memset(&remap, 0, sizeof(remap));
   remap.samplers = names;
   remap.sampler_count = 1;
   remap.rect_name = "Row0";
   remap.clamp_name = "Row1";
   remap.texels_name = "Row2";
   remap.planar = planar;
   remap.colour = !planar;
   remap.linear = linear;
   options.dst = RSLC_DST_SPIRV;
   options.remap = &remap;
   options.err = error;
   options.err_cap = sizeof(error);
   if (!rslc_compile(text, strlen(text), RSLC_STAGE_COMPUTE, &options, &compiled))
   {
      fprintf(stderr, "%s\n", error);
      abort();
   }
   assert(compiled.remap == RSLC_REMAP_REWRITTEN);
   make_image(hdr ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM, output_w, output_h,
              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &destination,
              &destination_memory, &destination_view);
   assert(vulkan_android_pipeline_create(device, &ops, sampler, 1, 1, compiled.spirv,
                                         compiled.spirv_len * 4, &pipeline));
   assert(vulkan_android_pipeline_update(device, &ops, &pipeline, 0, p->view, destination_view));
   memset(&image, 0, sizeof(image));
   memset(&write, 0, sizeof(write));
   image.imageView = p->view;
   image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
   write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   write.dstSet = pipeline.sets[0];
   write.dstBinding = 1;
   write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   write.descriptorCount = 1;
   write.pImageInfo = &image;
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   OK(vkResetCommandBuffer(command, 0));
   OK(vkBeginCommandBuffer(command, &begin));
   transition(p->image, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL,
              VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
   transition(destination, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
              VK_ACCESS_SHADER_WRITE_BIT);
   hdr_viewer_colour_rows(colour, false, 80.0f, 0);
   if (!hdr)
      memset(colour, 0, sizeof(colour));
   vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);
   vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 0, 1,
                           pipeline.sets, 0, NULL);
   vkCmdPushConstants(command, pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(colour),
                      colour);
   memset(&query_info, 0, sizeof(query_info));
   query_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
   query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
   query_info.queryCount = 2;
   OK(vkCreateQueryPool(device, &query_info, NULL, &queries));
   vkCmdResetQueryPool(command, queries, 0, 2);
   vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
   memset(&again, 0, sizeof(again));
   again.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   again.srcAccessMask = again.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
   for (i = 0; i < repeats; i++)
   {
      vkCmdDispatch(command, (output_w + 7) / 8, (output_h + 7) / 8, 1);
      vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &again, 0, NULL, 0, NULL);
   }
   vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
   transition(destination, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
   memset(&copy, 0, sizeof(copy));
   copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1;
   copy.imageExtent.width = output_w;
   copy.imageExtent.height = output_h;
   copy.imageExtent.depth = 1;
   vkCmdCopyImageToBuffer(command, destination, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1,
                          &copy);
   memset(&host, 0, sizeof(host));
   host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   host.buffer = buffer;
   host.size = VK_WHOLE_SIZE;
   host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                        NULL, 1, &host, 0, NULL);
   OK(vkEndCommandBuffer(command));
   memset(&fi, 0, sizeof(fi));
   fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   OK(vkCreateFence(device, &fi, NULL, &fence));
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &command;
   OK(vkQueueSubmit(queue, 1, &submit, fence));
   OK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_C(5000000000)));
   for (i = 0; i < output_w * output_h * 4; i++)
   {
      if (hdr)
      {
         uint16_t v;
         memcpy(&v, mapped + i * 2, 2);
         result[i] = (float)half_value(v);
      }
      else
         result[i] = mapped[i] / 255.0f;
   }
   OK(vkGetQueryPoolResults(device, queries, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT));
   gpu_usec =
       (timestamps[1] - timestamps[0]) * properties.limits.timestampPeriod / 1000.0 / repeats;
   vkDestroyQueryPool(device, queries, NULL);
   vkDestroyFence(device, fence, NULL);
   vulkan_android_pipeline_destroy(device, &ops, &pipeline);
   rslc_result_free(&compiled, &options);
   vkDestroyImageView(device, destination_view, NULL);
   vkDestroyImage(device, destination, NULL);
   vkFreeMemory(device, destination_memory, NULL);
}
static void pixels(unsigned w, unsigned h, unsigned tag, bool linear, bool hdr)
{
   struct mapped_input rgba, atlas;
   uint8_t *rgb, *bytes;
   unsigned x, y, c, ch = (h + 1) / 2;
   float coeff[6];
   float *expected = (float *)malloc(output_w * output_h * 4 * sizeof(float));
   float *actual = (float *)malloc(output_w * output_h * 4 * sizeof(float));
   double rgb_time, planar_time;
   VkSamplerCreateInfo si;
   input_create(&rgba, VK_FORMAT_R8G8B8A8_UNORM, w, h);
   input_create(&atlas, VK_FORMAT_R8_UNORM, w, h + 2 * ch + 1);
   rgb = (uint8_t *)rgba.map + rgba.pitch.offset;
   bytes = (uint8_t *)atlas.map + atlas.pitch.offset;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         bytes[y * atlas.pitch.rowPitch + x] = (uint8_t)(x * 47 + y * 83 + tag * 17);
   for (y = 0; y < ch; y++)
      for (x = 0; x < (w + 1) / 2; x++)
      {
         bytes[(h + y) * atlas.pitch.rowPitch + x] = (uint8_t)(x * 67 + y * 31 + tag * 19);
         bytes[(h + ch + y) * atlas.pitch.rowPitch + x] =
             (uint8_t)(255 - x * 29 - y * 41 - tag * 13);
      }
   bytes[(h + 2 * ch) * atlas.pitch.rowPitch] = (uint8_t)tag;
   image_yuv_coefficients(
       (tag & 1 ? IMAGE_YUV_FLAG_FULL_RANGE : 0) | (tag & 2 ? IMAGE_YUV_FLAG_BT709 : 0), coeff);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         float u = bytes[(h + y / 2) * atlas.pitch.rowPitch + x / 2] / 255.0f - 128.0f / 255.0f;
         float v =
             bytes[(h + ch + y / 2) * atlas.pitch.rowPitch + x / 2] / 255.0f - 128.0f / 255.0f;
         float yy = coeff[0] * (bytes[y * atlas.pitch.rowPitch + x] / 255.0f) + coeff[1];
         float values[3];
         values[0] = yy + coeff[2] * v;
         values[1] = yy + coeff[3] * u + coeff[4] * v;
         values[2] = yy + coeff[5] * u;
         for (c = 0; c < 3; c++)
         {
            float q = values[c] < 0 ? 0 : values[c] > 1 ? 1 : values[c];
            rgb[y * rgba.pitch.rowPitch + x * 4 + c] = (uint8_t)floor(q * 255 + .5);
         }
         rgb[y * rgba.pitch.rowPitch + x * 4 + 3] = 255;
      }
   memset(&si, 0, sizeof(si));
   si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   si.magFilter = si.minFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
   si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   OK(vkCreateSampler(device, &si, NULL, &sampler));
   render(&rgba, w, h, linear, hdr, false, expected);
   rgb_time = gpu_usec;
   render(&atlas, w, h, linear, hdr, true, actual);
   planar_time = gpu_usec;
   for (x = 0; x < output_w * output_h * 4; x++)
   {
      float delta = (float)fabs((double)actual[x] - expected[x]),
            limit = hdr ? .015f : 2.01f / 255.0f;
      if (delta > limit)
      {
         fprintf(stderr, "planar %ux%u tag %u linear %u hdr %u pixel %u: %.8f versus %.8f\n", w, h,
                 tag, linear, hdr, x, actual[x], expected[x]);
         abort();
      }
   }
   vkDestroySampler(device, sampler, NULL);
   input_free(&rgba);
   input_free(&atlas);
   printf("PASS Vulkan planar %ux%u tag %u linear %u HDR %u\n", w, h, tag, linear, hdr);
   if (repeats > 1)
      printf("SAMPLING_ONLY_GPU_US RGB %.3f planar %.3f (%u draws, %ux%u output)\n", rgb_time,
             planar_time, repeats, output_w, output_h);
   free(expected);
   free(actual);
}
int main(int argc, char **argv)
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
   bi.size = 8 * 1024 * 1024;
   bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   OK(vkCreateBuffer(device, &bi, NULL, &buffer));
   vkGetBufferMemoryRequirements(device, buffer, &req);
   memset(&ai, 0, sizeof(ai));
   ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   ai.allocationSize = req.size;
   ai.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   OK(vkAllocateMemory(device, &ai, NULL, &buffer_memory));
   OK(vkBindBufferMemory(device, buffer, buffer_memory, 0));
   OK(vkMapMemory(device, buffer_memory, 0, VK_WHOLE_SIZE, 0, (void **)&mapped));
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
   if (argc > 1 && !strcmp(argv[1], "--bench"))
   {
      output_w = 1280;
      output_h = 720;
      repeats = 32;
      for (i = 0; i < 4; i++)
         pixels(1280, 720, 3, (i & 1) != 0, (i & 2) != 0);
   }
   else
      for (i = 0; i < 32; i++)
      {
         pixels(i & 1 ? 6 : 5, i & 1 ? 4 : 3, (i >> 1) & 3, (i & 8) != 0, (i & 16) != 0);
         pixels(1, 1, (i >> 1) & 3, (i & 8) != 0, (i & 16) != 0);
      }
   vkUnmapMemory(device, buffer_memory);
   vkDestroyBuffer(device, buffer, NULL);
   vkFreeMemory(device, buffer_memory, NULL);
   vkDestroyCommandPool(device, command_pool, NULL);
   vkDestroyDevice(device, NULL);
   vkDestroyInstance(instance, NULL);
   return 0;
}
