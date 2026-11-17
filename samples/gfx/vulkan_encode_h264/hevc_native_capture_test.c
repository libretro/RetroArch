/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_ADAPTER_HELPERS_ONLY
#include "hevc_native_adapter_test.c"
#include "../../../gfx/common/vulkan_encode_capture.h"
#include <math.h>
struct native_capture_command
{
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier2 image;
   VkDependencyInfo dependency;
   VkClearColorValue color;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkSubmitInfo submit;
   VkFence fence;
   VkPipelineStageFlags wait_stage;
   vulkan_encode_capture_source_t source;
   uint64_t ready, done;
};
struct native_hevc_capture
{
   struct native_hevc_adapter adapter;
   struct native_capture_command capture_command;
   vulkan_encode_capture_ops_t capture_ops;
   vulkan_encode_capture_t *capture;
   VkImage rgb;
   VkImageView rgb_view;
   VkDeviceMemory rgb_memory;
   VkFormat rgb_format;
   unsigned output_mode;
   bool pq_signal;
   char shader_error[512];
};
static void load_capture(struct native_hevc_capture *n)
{
   struct native_encoder *c = &n->adapter.gpu.base;
   (void)upload_frame;
   n->capture_ops.create_set_layout = (PFN_vkCreateDescriptorSetLayout)c->device_proc(c->device, "vkCreateDescriptorSetLayout");
   n->capture_ops.destroy_set_layout = (PFN_vkDestroyDescriptorSetLayout)c->device_proc(c->device, "vkDestroyDescriptorSetLayout");
   n->capture_ops.create_descriptor_pool = (PFN_vkCreateDescriptorPool)c->device_proc(c->device, "vkCreateDescriptorPool");
   n->capture_ops.destroy_descriptor_pool = (PFN_vkDestroyDescriptorPool)c->device_proc(c->device, "vkDestroyDescriptorPool");
   n->capture_ops.allocate_sets = (PFN_vkAllocateDescriptorSets)c->device_proc(c->device, "vkAllocateDescriptorSets");
   n->capture_ops.update_sets = (PFN_vkUpdateDescriptorSets)c->device_proc(c->device, "vkUpdateDescriptorSets");
   n->capture_ops.create_sampler = (PFN_vkCreateSampler)c->device_proc(c->device, "vkCreateSampler");
   n->capture_ops.destroy_sampler = (PFN_vkDestroySampler)c->device_proc(c->device, "vkDestroySampler");
   n->capture_ops.create_pipeline_layout = (PFN_vkCreatePipelineLayout)c->device_proc(c->device, "vkCreatePipelineLayout");
   n->capture_ops.destroy_pipeline_layout = (PFN_vkDestroyPipelineLayout)c->device_proc(c->device, "vkDestroyPipelineLayout");
   n->capture_ops.create_shader = (PFN_vkCreateShaderModule)c->device_proc(c->device, "vkCreateShaderModule");
   n->capture_ops.destroy_shader = (PFN_vkDestroyShaderModule)c->device_proc(c->device, "vkDestroyShaderModule");
   n->capture_ops.create_pipelines = (PFN_vkCreateComputePipelines)c->device_proc(c->device, "vkCreateComputePipelines");
   n->capture_ops.destroy_pipeline = (PFN_vkDestroyPipeline)c->device_proc(c->device, "vkDestroyPipeline");
   n->capture_ops.bind_pipeline = (PFN_vkCmdBindPipeline)c->device_proc(c->device, "vkCmdBindPipeline");
   n->capture_ops.bind_sets = (PFN_vkCmdBindDescriptorSets)c->device_proc(c->device, "vkCmdBindDescriptorSets");
   n->capture_ops.push = (PFN_vkCmdPushConstants)c->device_proc(c->device, "vkCmdPushConstants");
   n->capture_ops.dispatch = (PFN_vkCmdDispatch)c->device_proc(c->device, "vkCmdDispatch");
   n->capture_ops.barrier = (PFN_vkCmdPipelineBarrier2)c->device_proc(c->device, "vkCmdPipelineBarrier2");
   n->capture_ops.copy = (PFN_vkCmdCopyBufferToImage)c->device_proc(c->device, "vkCmdCopyBufferToImage");
}
static void capture_new_source(struct native_hevc_capture *c,const vulkan_encode_pool_t *pool)
{
   VkImageCreateInfo image;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocation;
   VkImageViewCreateInfo view;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo command;
   unsigned i;
   memset(&image,0,sizeof(image));
   image.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.imageType=VK_IMAGE_TYPE_2D;
   image.format=c->rgb_format ? c->rgb_format : VK_FORMAT_R8G8B8A8_UNORM;
   image.extent.width=pool->extent.width;
   image.extent.height=pool->extent.height;
   image.extent.depth=image.mipLevels=image.arrayLayers=1;
   image.samples=VK_SAMPLE_COUNT_1_BIT;
   image.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   assert(c->adapter.ops.pool.create_image(c->adapter.gpu.base.device,&image,NULL,&c->rgb)==VK_SUCCESS);
   c->adapter.ops.pool.image_memory(c->adapter.gpu.base.device,c->rgb,&requirements);
   c->adapter.ops.pool.memory_properties(c->adapter.gpu.base.physical,&c->adapter.gpu.memory);
   for (i=0;i<c->adapter.gpu.memory.memoryTypeCount;i++)
      if ((requirements.memoryTypeBits&(1u<<i))
            && (c->adapter.gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            && !(c->adapter.gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_PROTECTED_BIT)) break;
   assert(i<c->adapter.gpu.memory.memoryTypeCount);
   memset(&allocation,0,sizeof(allocation));
   allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.allocationSize=requirements.size;
   allocation.memoryTypeIndex=i;
   assert(c->adapter.ops.pool.allocate(c->adapter.gpu.base.device,&allocation,NULL,&c->rgb_memory)==VK_SUCCESS);
   assert(c->adapter.ops.pool.bind_image(c->adapter.gpu.base.device,c->rgb,c->rgb_memory,0)==VK_SUCCESS);
   memset(&view,0,sizeof(view));
   view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image=c->rgb;
   view.viewType=VK_IMAGE_VIEW_TYPE_2D;
   view.format=image.format;
   view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount=view.subresourceRange.layerCount=1;
   assert(c->adapter.ops.pool.create_view(c->adapter.gpu.base.device,&view,NULL,&c->rgb_view)==VK_SUCCESS);
   memset(&commands,0,sizeof(commands));
   commands.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex=c->adapter.gpu.base.producer_family;
   assert(c->adapter.ops.pool.create_commands(c->adapter.gpu.base.device,&commands,NULL,&c->adapter.gpu.producer_commands)==VK_SUCCESS);
   memset(&command,0,sizeof(command));
   command.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool=c->adapter.gpu.producer_commands;
   command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount=2;
   assert(c->adapter.ops.pool.allocate_command(c->adapter.gpu.base.device,&command,c->adapter.gpu.producer)==VK_SUCCESS);
}
static void capture_new(struct native_hevc_capture *c,const vulkan_encode_pool_t *pool)
{
   capture_new_source(c, pool);
   c->capture=vulkan_encode_capture_new_compiled(pool,&c->capture_ops,
         c->shader_error,sizeof(c->shader_error));
   if (!c->capture) fprintf(stderr,"%s\n",c->shader_error);
   assert(c->capture);
}
static void capture_hdr_color(struct native_hevc_capture *c, unsigned frame,
      float *color)
{
   static const double to709[3][3] = {
      {1.6604910, -0.5876411, -0.0728499},
      {-0.1245505, 1.1328999, -0.0083494},
      {-0.0181508, -0.1005789, 1.1187297}
   };
   static const double peaks[8] = {0, 0.01, 1, 16, 200, 1000, 4000, 12000};
   double nits[3], value, power, peak = peaks[(frame / 3) % 8];
   unsigned i, j;
   for (i = 0; i < 3; i++)
      nits[i] = frame % 3 == i ? peak : peak / 16.0;
   for (i = 0; i < 3; i++)
   {
      if (c->output_mode == 1 || c->pq_signal)
      {
         power = pow(nits[i] / 10000.0, 2610.0 / 16384.0);
         value = pow((3424.0 / 4096.0 + (2413.0 / 128.0) * power)
               / (1.0 + (2392.0 / 128.0) * power), 2523.0 / 32.0);
      }
      else
      {
         value = 0;
         for (j = 0; j < 3; j++)
            value += to709[i][j] * nits[j] / 80.0;
      }
      color[i] = (float)value;
   }
}
static void capture_frame(struct native_hevc_capture *c,const vulkan_encode_pool_t *pool,
      unsigned frame)
{
   struct native_capture_command *r=&c->capture_command;
   PFN_vkCmdClearColorImage clear;
   VkCommandBuffer command=c->adapter.gpu.producer[frame%2];
   unsigned i,base=32+frame*6;
   r->ready=frame+1;
   r->done=frame>=2?frame-1:0;
   r->wait_stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   clear=(PFN_vkCmdClearColorImage)c->adapter.gpu.base.device_proc(c->adapter.gpu.base.device,"vkCmdClearColorImage");
   assert(clear && c->adapter.ops.submit.reset_command(command,0)==VK_SUCCESS);
   memset(&r->begin,0,sizeof(r->begin));
   r->begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(c->adapter.ops.submit.begin(command,&r->begin)==VK_SUCCESS);
   memset(&r->image,0,sizeof(r->image));
   r->image.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   r->image.srcStageMask=frame?VK_PIPELINE_STAGE_2_TRANSFER_BIT:VK_PIPELINE_STAGE_2_NONE;
   r->image.srcAccessMask=frame?VK_ACCESS_2_TRANSFER_WRITE_BIT:0;
   r->image.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   r->image.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   r->image.oldLayout=frame?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;
   r->image.newLayout=VK_IMAGE_LAYOUT_GENERAL;
   r->image.srcQueueFamilyIndex=r->image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   r->image.image=c->rgb;
   r->image.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   r->image.subresourceRange.levelCount=r->image.subresourceRange.layerCount=1;
   memset(&r->dependency,0,sizeof(r->dependency));
   r->dependency.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   r->dependency.imageMemoryBarrierCount=1;
   r->dependency.pImageMemoryBarriers=&r->image;
   c->adapter.ops.submit.barrier(command,&r->dependency);
   for (i=0;i<3;i++) r->color.float32[i]=(float)(frame%3==i?base:16)/255.0f;
   if (c->output_mode)
      capture_hdr_color(c, frame, r->color.float32);
   r->color.float32[3]=1;
   clear(command,c->rgb,VK_IMAGE_LAYOUT_GENERAL,&r->color,1,&r->image.subresourceRange);
   memset(&r->source,0,sizeof(r->source));
   r->source.image=c->rgb;
   r->source.view=c->rgb_view;
   r->source.format=c->rgb_format ? c->rgb_format : VK_FORMAT_R8G8B8A8_UNORM;
   r->source.output_mode=c->output_mode;
   r->source.layout=VK_IMAGE_LAYOUT_GENERAL;
   r->source.stage=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   r->source.access=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   r->source.width=r->source.valid_width=pool->extent.width;
   r->source.height=r->source.valid_height=pool->extent.height;
   r->source.limited_range=true;
   r->source.crop_x=3;
   r->source.crop_y=2;
   r->source.valid_width=pool->extent.width-7;
   r->source.valid_height=pool->extent.height-5;
   r->source.limited_range=!c->adapter.config.session.video.full_range;
   r->source.flip_y=(frame&1)!=0;
   assert(vulkan_encode_capture_record(c->capture,frame%2,command,&r->source,frame>=2));
   assert(c->adapter.ops.submit.end(command)==VK_SUCCESS);
   memset(&r->timeline,0,sizeof(r->timeline));
   r->timeline.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   r->timeline.signalSemaphoreValueCount=1;
   r->timeline.pSignalSemaphoreValues=&r->ready;
   r->timeline.waitSemaphoreValueCount=frame>=2?1:0;
   r->timeline.pWaitSemaphoreValues=frame>=2?&r->done:NULL;
   memset(&r->submit,0,sizeof(r->submit));
   r->submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   r->submit.pNext=&r->timeline;
   r->submit.commandBufferCount=r->submit.signalSemaphoreCount=1;
   r->submit.pCommandBuffers=&command;
   r->submit.pSignalSemaphores=&pool->ready;
   r->submit.waitSemaphoreCount=frame>=2?1:0;
   r->submit.pWaitSemaphores=frame>=2?&pool->done:NULL;
   r->submit.pWaitDstStageMask=frame>=2?&r->wait_stage:NULL;
   assert(c->adapter.ops.submit.submit(c->adapter.gpu.base.producer_queue,1,&r->submit,r->fence)==VK_SUCCESS);
}
static void capture_free_source(struct native_hevc_capture *c)
{
   c->adapter.ops.pool.destroy_view(c->adapter.gpu.base.device,c->rgb_view,NULL);
   c->adapter.ops.pool.destroy_image(c->adapter.gpu.base.device,c->rgb,NULL);
   c->adapter.ops.pool.free_memory(c->adapter.gpu.base.device,c->rgb_memory,NULL);
}

static void capture_free(struct native_hevc_capture *c)
{
   vulkan_encode_capture_free(c->capture);
   capture_free_source(c);
}

#ifndef HEVC_NATIVE_CAPTURE_HELPERS_ONLY
int main(int argc, char **argv)
{
   struct native_hevc_capture *n = (struct native_hevc_capture*)calloc(1, sizeof(*n));
   struct native_encoder *c;
   vulkan_encode_hevc_t *encoder;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_packet_t packet;
   PFN_vkWaitForFences wait;
   unsigned char *headers = (unsigned char*)malloc(65536);
   unsigned frame, depth;
   size_t size;
   FILE *file;
   bool direct, full;
   int result = 77;
   assert(n && headers && argc == 5);
   c = &n->adapter.gpu.base;
   (void)upload_frame;
   depth = (unsigned)atoi(argv[1]);
   direct = atoi(argv[3]) != 0;
   full = atoi(argv[4]) != 0;
   encoder = native_adapter_new_mode(&n->adapter, depth, true, direct, full);
   if (!encoder)
   {
      puts("UNAVAILABLE HEVC encode and compute capture device");
      goto done;
   }
   pool = vulkan_encode_hevc_pool(encoder);
   if (direct && !pool->storage_images)
   {
      assert(vulkan_encode_hevc_free(encoder));
      puts("UNAVAILABLE direct capture storage planes");
      goto done;
   }
   assert(pool->storage_images == direct);
   load_capture(n);
   capture_new(n, pool);
   wait = (PFN_vkWaitForFences)c->device_proc(c->device, "vkWaitForFences");
   assert(wait);
   file = fopen(argv[2], "wb");
   assert(file && vulkan_encode_hevc_headers(encoder, headers, 65536, &size));
   assert(fwrite(headers, 1, size, file) == size);
   for (frame = 0; frame < 24; frame++)
   {
      capture_frame(n, pool, frame);
      assert(vulkan_encode_hevc_submit(encoder, frame % 2, frame, 1,
               frame + 1, frame == 7) == VULKAN_ENCODE_OK);
      /* Fixture completion only; capture and encoding never wait. */
      assert(wait(c->device, 1, &pool->slot[frame % 2].fence,
               VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_OK);
      assert(fwrite(packet.data, 1, packet.size, file) == packet.size);
      assert(vulkan_encode_hevc_release(encoder));
   }
   assert(vulkan_encode_hevc_finish(encoder) == VULKAN_ENCODE_OK);
   assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_END);
   capture_free(n);
   assert(vulkan_encode_hevc_free(encoder) && fclose(file) == 0);
   printf("device=%s depth=%u direct=%u full=%u pictures=24 sync_validation=1 validation_errors=%u\n",
         c->properties.deviceName, depth, direct ? 1u : 0u, full ? 1u : 0u, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   free(headers);
   native_adapter_context_free(&n->adapter);
   return result;
}

#endif
