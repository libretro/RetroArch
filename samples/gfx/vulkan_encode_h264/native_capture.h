/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NATIVE_ENCODE_CAPTURE_H
#define NATIVE_ENCODE_CAPTURE_H
static void capture_new(struct native_encoder *c,const vulkan_encode_pool_t *pool)
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
   image.format=VK_FORMAT_R8G8B8A8_UNORM;
   image.extent.width=pool->extent.width;
   image.extent.height=pool->extent.height;
   image.extent.depth=image.mipLevels=image.arrayLayers=1;
   image.samples=VK_SAMPLE_COUNT_1_BIT;
   image.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   assert(c->ops.pool.create_image(c->device,&image,NULL,&c->rgb)==VK_SUCCESS);
   c->ops.pool.image_memory(c->device,c->rgb,&requirements);
   c->ops.pool.memory_properties(c->physical,&c->memory);
   for (i=0;i<c->memory.memoryTypeCount;i++)
      if ((requirements.memoryTypeBits&(1u<<i))
            && (c->memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            && !(c->memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_PROTECTED_BIT)) break;
   assert(i<c->memory.memoryTypeCount);
   memset(&allocation,0,sizeof(allocation));
   allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.allocationSize=requirements.size;
   allocation.memoryTypeIndex=i;
   assert(c->ops.pool.allocate(c->device,&allocation,NULL,&c->rgb_memory)==VK_SUCCESS);
   assert(c->ops.pool.bind_image(c->device,c->rgb,c->rgb_memory,0)==VK_SUCCESS);
   memset(&view,0,sizeof(view));
   view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image=c->rgb;
   view.viewType=VK_IMAGE_VIEW_TYPE_2D;
   view.format=image.format;
   view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount=view.subresourceRange.layerCount=1;
   assert(c->ops.pool.create_view(c->device,&view,NULL,&c->rgb_view)==VK_SUCCESS);
   memset(&commands,0,sizeof(commands));
   commands.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex=c->producer_family;
   assert(c->ops.pool.create_commands(c->device,&commands,NULL,&c->producer_commands)==VK_SUCCESS);
   memset(&command,0,sizeof(command));
   command.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool=c->producer_commands;
   command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount=2;
   assert(c->ops.pool.allocate_command(c->device,&command,c->producer)==VK_SUCCESS);
   c->capture=vulkan_encode_capture_new_compiled(pool,&c->capture_ops,
         c->shader_error,sizeof(c->shader_error));
   if (!c->capture) fprintf(stderr,"%s\n",c->shader_error);
   assert(c->capture);
}
static void capture_frame(struct native_encoder *c,const vulkan_encode_pool_t *pool,
      unsigned frame)
{
   struct native_capture_command *r=&c->capture_command;
   PFN_vkCmdClearColorImage clear;
   VkCommandBuffer command=c->producer[frame%2];
   unsigned i,base=32+frame*12;
   r->ready=frame+1;
   r->done=frame-1;
   r->wait_stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   clear=(PFN_vkCmdClearColorImage)c->device_proc(c->device,"vkCmdClearColorImage");
   assert(clear && c->ops.submit.reset_command(command,0)==VK_SUCCESS);
   memset(&r->begin,0,sizeof(r->begin));
   r->begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(c->ops.submit.begin(command,&r->begin)==VK_SUCCESS);
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
   c->ops.submit.barrier(command,&r->dependency);
   for (i=0;i<3;i++) r->color.float32[i]=(float)(frame%3==i?base:16)/255.0f;
   r->color.float32[3]=1;
   clear(command,c->rgb,VK_IMAGE_LAYOUT_GENERAL,&r->color,1,&r->image.subresourceRange);
   memset(&r->source,0,sizeof(r->source));
   r->source.image=c->rgb;
   r->source.view=c->rgb_view;
   r->source.format=VK_FORMAT_R8G8B8A8_UNORM;
   r->source.layout=VK_IMAGE_LAYOUT_GENERAL;
   r->source.stage=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   r->source.access=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   r->source.width=r->source.valid_width=pool->extent.width;
   r->source.height=r->source.valid_height=pool->extent.height;
   r->source.limited_range=true;
   assert(vulkan_encode_capture_record(c->capture,frame%2,command,&r->source,frame>=2));
   assert(c->ops.submit.end(command)==VK_SUCCESS);
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
   assert(c->ops.submit.submit(c->producer_queue,1,&r->submit,VK_NULL_HANDLE)==VK_SUCCESS);
}
static void capture_free(struct native_encoder *c)
{
   vulkan_encode_capture_free(c->capture);
   c->ops.pool.destroy_view(c->device,c->rgb_view,NULL);
   c->ops.pool.destroy_image(c->device,c->rgb,NULL);
   c->ops.pool.free_memory(c->device,c->rgb_memory,NULL);
}
#endif
