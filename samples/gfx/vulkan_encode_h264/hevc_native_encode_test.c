/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "hevc_native_context.h"
#include "../../../gfx/common/vulkan_encode_pool.h"
#include "../../../gfx/common/vulkan_encode_hevc_submit.h"
struct native_hevc_encoder
{
   struct native_encoder base;
   vulkan_encode_pool_ops_t pool_ops;
   vulkan_encode_hevc_submit_ops_t submit_ops;
   VkPhysicalDeviceMemoryProperties memory;
   vulkan_encode_hevc_submit_t transaction;
   vulkan_encode_hevc_picture_t picture[2];
   vulkan_encode_hevc_resources_t resources;
   vulkan_encode_hevc_dpb_t dpb;
   VkQueue queue;
   VkCommandPool producer_commands;
   VkCommandBuffer producer[2];
   VkBuffer upload;
   VkDeviceMemory upload_memory;
   void *upload_data;
};
static void load_frame_ops(struct native_hevc_encoder *e)
{
   struct native_encoder *c = &e->base;
   e->pool_ops.memory_properties = (PFN_vkGetPhysicalDeviceMemoryProperties)c->instance_proc(c->instance, "vkGetPhysicalDeviceMemoryProperties");
   e->pool_ops.video_formats = (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)c->instance_proc(c->instance, "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
   e->pool_ops.image_properties = (PFN_vkGetPhysicalDeviceImageFormatProperties2)c->instance_proc(c->instance, "vkGetPhysicalDeviceImageFormatProperties2");
   e->pool_ops.format_properties = (PFN_vkGetPhysicalDeviceFormatProperties2)c->instance_proc(c->instance, "vkGetPhysicalDeviceFormatProperties2");
   e->pool_ops.create_image = (PFN_vkCreateImage)c->device_proc(c->device, "vkCreateImage");
   e->pool_ops.destroy_image = (PFN_vkDestroyImage)c->device_proc(c->device, "vkDestroyImage");
   e->pool_ops.image_memory = (PFN_vkGetImageMemoryRequirements)c->device_proc(c->device, "vkGetImageMemoryRequirements");
   e->pool_ops.bind_image = (PFN_vkBindImageMemory)c->device_proc(c->device, "vkBindImageMemory");
   e->pool_ops.create_view = (PFN_vkCreateImageView)c->device_proc(c->device, "vkCreateImageView");
   e->pool_ops.destroy_view = (PFN_vkDestroyImageView)c->device_proc(c->device, "vkDestroyImageView");
   e->pool_ops.create_buffer = (PFN_vkCreateBuffer)c->device_proc(c->device, "vkCreateBuffer");
   e->pool_ops.destroy_buffer = (PFN_vkDestroyBuffer)c->device_proc(c->device, "vkDestroyBuffer");
   e->pool_ops.buffer_memory = (PFN_vkGetBufferMemoryRequirements)c->device_proc(c->device, "vkGetBufferMemoryRequirements");
   e->pool_ops.bind_buffer = (PFN_vkBindBufferMemory)c->device_proc(c->device, "vkBindBufferMemory");
   e->pool_ops.map = (PFN_vkMapMemory)c->device_proc(c->device, "vkMapMemory");
   e->pool_ops.unmap = (PFN_vkUnmapMemory)c->device_proc(c->device, "vkUnmapMemory");
   e->pool_ops.allocate = (PFN_vkAllocateMemory)c->device_proc(c->device, "vkAllocateMemory");
   e->pool_ops.free_memory = (PFN_vkFreeMemory)c->device_proc(c->device, "vkFreeMemory");
   e->pool_ops.create_commands = (PFN_vkCreateCommandPool)c->device_proc(c->device, "vkCreateCommandPool");
   e->pool_ops.destroy_commands = (PFN_vkDestroyCommandPool)c->device_proc(c->device, "vkDestroyCommandPool");
   e->pool_ops.allocate_command = (PFN_vkAllocateCommandBuffers)c->device_proc(c->device, "vkAllocateCommandBuffers");
   e->pool_ops.create_fence = (PFN_vkCreateFence)c->device_proc(c->device, "vkCreateFence");
   e->pool_ops.destroy_fence = (PFN_vkDestroyFence)c->device_proc(c->device, "vkDestroyFence");
   e->pool_ops.create_queries = (PFN_vkCreateQueryPool)c->device_proc(c->device, "vkCreateQueryPool");
   e->pool_ops.destroy_queries = (PFN_vkDestroyQueryPool)c->device_proc(c->device, "vkDestroyQueryPool");
   e->pool_ops.create_semaphore = (PFN_vkCreateSemaphore)c->device_proc(c->device, "vkCreateSemaphore");
   e->pool_ops.destroy_semaphore = (PFN_vkDestroySemaphore)c->device_proc(c->device, "vkDestroySemaphore");
   e->submit_ops.reset_command = (PFN_vkResetCommandBuffer)c->device_proc(c->device, "vkResetCommandBuffer");
   e->submit_ops.begin = (PFN_vkBeginCommandBuffer)c->device_proc(c->device, "vkBeginCommandBuffer");
   e->submit_ops.end = (PFN_vkEndCommandBuffer)c->device_proc(c->device, "vkEndCommandBuffer");
   e->submit_ops.barrier = (PFN_vkCmdPipelineBarrier2)c->device_proc(c->device, "vkCmdPipelineBarrier2");
   e->submit_ops.reset_query = (PFN_vkCmdResetQueryPool)c->device_proc(c->device, "vkCmdResetQueryPool");
   e->submit_ops.begin_query = (PFN_vkCmdBeginQuery)c->device_proc(c->device, "vkCmdBeginQuery");
   e->submit_ops.end_query = (PFN_vkCmdEndQuery)c->device_proc(c->device, "vkCmdEndQuery");
   e->submit_ops.begin_coding = (PFN_vkCmdBeginVideoCodingKHR)c->device_proc(c->device, "vkCmdBeginVideoCodingKHR");
   e->submit_ops.control = (PFN_vkCmdControlVideoCodingKHR)c->device_proc(c->device, "vkCmdControlVideoCodingKHR");
   e->submit_ops.encode = (PFN_vkCmdEncodeVideoKHR)c->device_proc(c->device, "vkCmdEncodeVideoKHR");
   e->submit_ops.end_coding = (PFN_vkCmdEndVideoCodingKHR)c->device_proc(c->device, "vkCmdEndVideoCodingKHR");
   e->submit_ops.reset_fence = (PFN_vkResetFences)c->device_proc(c->device, "vkResetFences");
   e->submit_ops.submit = (PFN_vkQueueSubmit)c->device_proc(c->device, "vkQueueSubmit");
   e->submit_ops.status = (PFN_vkGetFenceStatus)c->device_proc(c->device, "vkGetFenceStatus");
   e->submit_ops.feedback = (PFN_vkGetQueryPoolResults)c->device_proc(c->device, "vkGetQueryPoolResults");
}
static void upload_new(struct native_hevc_encoder *c,const vulkan_encode_pool_t *pool)
{
   VkBufferCreateInfo buffer;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocation;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo command;
   unsigned i;
   assert(c->base.queue_flags&VK_QUEUE_TRANSFER_BIT);
   memset(&buffer,0,sizeof(buffer));
   buffer.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer.size=(VkDeviceSize)pool->extent.width*pool->extent.height*6/
      (pool->format==VK_FORMAT_G8_B8R8_2PLANE_420_UNORM?2:1);
   buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   assert(c->pool_ops.create_buffer(c->base.device,&buffer,NULL,&c->upload)==VK_SUCCESS);
   c->pool_ops.buffer_memory(c->base.device,c->upload,&requirements);
   c->pool_ops.memory_properties(c->base.physical,&c->memory);
   for (i=0;i<c->memory.memoryTypeCount;i++)
      if ((requirements.memoryTypeBits&(1u<<i))
            && (c->memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
               ==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) break;
   assert(i<c->memory.memoryTypeCount);
   memset(&allocation,0,sizeof(allocation));
   allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.allocationSize=requirements.size;
   allocation.memoryTypeIndex=i;
   assert(c->pool_ops.allocate(c->base.device,&allocation,NULL,&c->upload_memory)==VK_SUCCESS);
   assert(c->pool_ops.bind_buffer(c->base.device,c->upload,c->upload_memory,0)==VK_SUCCESS);
   assert(c->pool_ops.map(c->base.device,c->upload_memory,0,buffer.size,0,&c->upload_data)==VK_SUCCESS);
   memset(&commands,0,sizeof(commands));
   commands.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex=c->base.family;
   assert(c->pool_ops.create_commands(c->base.device,&commands,NULL,&c->producer_commands)==VK_SUCCESS);
   memset(&command,0,sizeof(command));
   command.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool=c->producer_commands;
   command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount=2;
   assert(c->pool_ops.allocate_command(c->base.device,&command,c->producer)==VK_SUCCESS);
}
static void upload_frame(struct native_hevc_encoder *c,const vulkan_encode_pool_t *pool,
      unsigned frame)
{
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier2 image;
   VkBufferMemoryBarrier2 buffer;
   VkDependencyInfo dependency;
   VkBufferImageCopy regions[2];
   VkTimelineSemaphoreSubmitInfo timeline;
   VkSubmitInfo submit;
   PFN_vkCmdCopyBufferToImage copy;
   VkCommandBuffer command=c->producer[frame%2];
   uint64_t ready=frame+1;
   unsigned i;
   size_t luma=(size_t)pool->extent.width*pool->extent.height;
   size_t bytes=luma*3/(pool->format==VK_FORMAT_G8_B8R8_2PLANE_420_UNORM?2:1);
   size_t offset=(frame%2)*bytes;
   uint8_t *data=(uint8_t*)c->upload_data+offset;
   if (pool->format==VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
   {
      memset(data,32+frame*6,luma);
      memset(data+luma,128,luma/2);
   }
   else
   {
      uint16_t *samples=(uint16_t*)data;
      for (i=0;i<luma;i++)
         samples[i]=(uint16_t)((128+frame*24)<<6);
      for (i=0;i<luma/2;i++)
         samples[luma+i]=32768;
      luma*=2;
   }
   copy=(PFN_vkCmdCopyBufferToImage)c->base.device_proc(c->base.device,"vkCmdCopyBufferToImage");
   assert(copy && c->submit_ops.reset_command(command,0)==VK_SUCCESS);
   memset(&begin,0,sizeof(begin));
   begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(c->submit_ops.begin(command,&begin)==VK_SUCCESS);
   memset(&image,0,sizeof(image));
   image.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   image.srcStageMask=frame<2?VK_PIPELINE_STAGE_2_NONE:VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   image.srcAccessMask=frame<2?0:VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
   image.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   image.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   image.oldLayout=frame<2?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_GENERAL;
   image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   image.image=pool->slot[frame%2].image;
   image.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   image.subresourceRange.levelCount=image.subresourceRange.layerCount=1;
   memset(&buffer,0,sizeof(buffer));
   buffer.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
   buffer.srcStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;
   buffer.srcAccessMask=VK_ACCESS_2_HOST_WRITE_BIT;
   buffer.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   buffer.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;
   buffer.srcQueueFamilyIndex=buffer.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   buffer.buffer=c->upload;
   buffer.offset=offset;
   buffer.size=bytes;
   memset(&dependency,0,sizeof(dependency));
   dependency.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependency.imageMemoryBarrierCount=dependency.bufferMemoryBarrierCount=1;
   dependency.pImageMemoryBarriers=&image;
   dependency.pBufferMemoryBarriers=&buffer;
   c->submit_ops.barrier(command,&dependency);
   memset(regions,0,sizeof(regions));
   for (i=0;i<2;i++)
   {
      regions[i].bufferOffset=offset+(i?luma:0);
      regions[i].imageSubresource.aspectMask=i?VK_IMAGE_ASPECT_PLANE_1_BIT:VK_IMAGE_ASPECT_PLANE_0_BIT;
      regions[i].imageSubresource.layerCount=1;
      regions[i].imageExtent.width=pool->extent.width>>i;
      regions[i].imageExtent.height=pool->extent.height>>i;
      regions[i].imageExtent.depth=1;
   }
   copy(command,c->upload,pool->slot[frame%2].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,regions);
   image.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   image.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   image.dstStageMask=VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   image.dstAccessMask=VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
   image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   image.newLayout=VK_IMAGE_LAYOUT_GENERAL;
   dependency.bufferMemoryBarrierCount=0;
   dependency.pBufferMemoryBarriers=NULL;
   c->submit_ops.barrier(command,&dependency);
   assert(c->submit_ops.end(command)==VK_SUCCESS);
   memset(&timeline,0,sizeof(timeline));
   timeline.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   timeline.signalSemaphoreValueCount=1;
   timeline.pSignalSemaphoreValues=&ready;
   memset(&submit,0,sizeof(submit));
   submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext=&timeline;
   submit.commandBufferCount=submit.signalSemaphoreCount=1;
   submit.pCommandBuffers=&command;
   submit.pSignalSemaphores=&pool->ready;
   assert(c->submit_ops.submit(c->queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS);
}
#ifndef HEVC_NATIVE_HELPERS_ONLY
int main(int argc, char **argv)
{
   struct native_hevc_encoder *e = (struct native_hevc_encoder*)calloc(1, sizeof(*e));
   struct native_encoder *c;
   vulkan_encode_hevc_session_config_t config;
   vulkan_encode_pool_config_t pool_config;
   vulkan_encode_hevc_session_t *session = NULL;
   vulkan_encode_pool_t *pool = NULL;
   const vulkan_encode_hevc_picture_t *previous = NULL;
   PFN_vkGetDeviceQueue get_queue;
   PFN_vkWaitForFences wait;
   unsigned char *headers = (unsigned char*)malloc(65536);
   unsigned ctb, i, frame, initialized = 0;
   const void *packet, *again;
   size_t size, again_size;
   FILE *file = NULL;
   int result = 77;
   assert(e && headers && argc == 3);
   c = &e->base;
   if (!startup(c) || !(c->queue_flags & VK_QUEUE_TRANSFER_BIT))
   {
      puts("UNAVAILABLE native HEVC encode device or validation layer");
      goto done;
   }
   load_frame_ops(e);
   get_queue = (PFN_vkGetDeviceQueue)c->device_proc(c->device, "vkGetDeviceQueue");
   wait = (PFN_vkWaitForFences)c->device_proc(c->device, "vkWaitForFences");
   assert(get_queue && wait);
   get_queue(c->device, c->family, 0, &e->queue);
   memset(&config, 0, sizeof(config));
   config.physical = c->physical;
   config.device = c->device;
   config.family = c->family;
   config.video.width = 192;
   config.video.height = 128;
   config.video.fps_num = 30;
   config.video.fps_den = 1;
   config.video.bitrate = 2000000;
   config.video.bit_depth = (unsigned)atoi(argv[1]);
   config.video.color = config.video.bit_depth == 10 ? VULKAN_ENCODE_HEVC_PQ : VULKAN_ENCODE_HEVC_BT709;
   for (ctb = 64; ctb >= 16 && !session; ctb /= 2)
   {
      config.video.ctb_size = ctb;
      session = vulkan_encode_hevc_session_new(&config, &c->ops);
   }
   assert(session);
   pool_config.profile = &session->profile;
   pool_config.capabilities = &session->capabilities;
   pool_config.physical = c->physical;
   pool_config.device = c->device;
   pool_config.extent.width = session->params.coded_width;
   pool_config.extent.height = session->params.coded_height;
   pool_config.format = session->format;
   pool_config.packet_size = 1048576;
   pool_config.slots = 2;
   pool_config.encode_family = pool_config.producer_family = c->family;
   pool_config.storage_image_extended_formats = false;
   pool = vulkan_encode_pool_new(&pool_config, &e->pool_ops);
   assert(pool);
   upload_new(e, pool);
   for (i = 0; i < 2; i++)
   {
      e->dpb.image[i] = pool->dpb.image[i];
      e->dpb.view[i] = pool->dpb.view[i];
   }
   file = fopen(argv[2], "wb");
   assert(file && vulkan_encode_hevc_session_headers(session, headers, 65536, &size));
   assert(fwrite(headers, 1, size, file) == size);
   for (frame = 0; frame < 24; frame++)
   {
      unsigned index = frame % 2;
      vulkan_encode_hevc_resources_t *r = &e->resources;
      vulkan_encode_pool_slot_t *slot = &pool->slot[index];
      vulkan_encode_hevc_picture_t *picture = &e->picture[index];
      upload_frame(e, pool, frame);
      r->dpb = &e->dpb;
      r->mapped = slot->output.mapped;
      r->capacity = (size_t)slot->output.size;
      r->queue = e->queue;
      r->command = slot->command;
      r->fence = slot->fence;
      r->queries = pool->queries;
      r->query = index;
      r->bitstream = slot->output.buffer;
      r->input = slot->image;
      r->input_view = slot->view;
      r->ready = pool->ready;
      r->done = pool->done;
      r->ready_value = r->done_value = frame + 1;
      assert(vulkan_encode_hevc_picture(picture, previous, index, 8, 22, frame == 7));
      assert(vulkan_encode_hevc_submit_start(&e->transaction, session, &e->submit_ops,
            r, picture, previous, initialized, !previous) == VULKAN_ENCODE_HEVC_OK);
      previous = picture;
      initialized |= 1u << index;
      /* Fixture only: production submission and polling never wait. */
      assert(wait(c->device, 1, &r->fence, VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(vulkan_encode_hevc_submit_poll(&e->transaction, &packet, &size) == VULKAN_ENCODE_HEVC_OK);
      assert(vulkan_encode_hevc_submit_poll(&e->transaction, &again, &again_size) == VULKAN_ENCODE_HEVC_OK);
      assert(packet == again && size == again_size && size);
      assert(fwrite(packet, 1, size, file) == size);
      assert(vulkan_encode_hevc_submit_retire(&e->transaction));
   }
   assert(fclose(file) == 0);
   file = NULL;
   e->pool_ops.destroy_commands(c->device, e->producer_commands, NULL);
   e->pool_ops.unmap(c->device, e->upload_memory);
   e->pool_ops.destroy_buffer(c->device, e->upload, NULL);
   e->pool_ops.free_memory(c->device, e->upload_memory, NULL);
   vulkan_encode_pool_free(pool);
   pool = NULL;
   vulkan_encode_hevc_session_free(session);
   session = NULL;
   printf("device=%s depth=%u pictures=24 validation_errors=%u\n",
         c->properties.deviceName, config.video.bit_depth, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   if (c->device)
      c->destroy_device(c->device, NULL);
   if (c->messenger)
      c->destroy_messenger(c->instance, c->messenger, NULL);
   if (c->instance)
      c->destroy_instance(c->instance, NULL);
   if (c->library)
      dylib_close(c->library);
   free(headers);
   free(e);
   return result;
}

#endif
