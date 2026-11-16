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
   VkSemaphoreCreateInfo semaphore;
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
   if (c->renderer)
   {
      semaphore.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      semaphore.pNext=NULL;
      semaphore.flags=0;
      for (i=0;i<2;i++)
         assert(c->ops.pool.create_semaphore(c->device,&semaphore,NULL,&c->capture_signals[i])==VK_SUCCESS);
   }
   if (!c->capture_borrowed)
      c->capture=vulkan_encode_capture_new_compiled(pool,&c->capture_ops,
            c->shader_error,sizeof(c->shader_error));
   if (!c->capture) fprintf(stderr,"%s\n",c->shader_error);
   assert(c->capture);
}
static void capture_discard_native(struct native_encoder *c,const vulkan_encode_pool_t *pool)
{
   VkCommandBufferBeginInfo begin;
   VkSubmitInfo submit;
   vulkan_encode_capture_source_t source;
   memset(&begin,0,sizeof(begin));
   begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(c->ops.submit.begin(c->producer[0],&begin)==VK_SUCCESS);
   memset(&source,0,sizeof(source));
   source.image=c->rgb;
   source.view=c->rgb_view;
   source.format=VK_FORMAT_R8G8B8A8_UNORM;
   source.layout=VK_IMAGE_LAYOUT_GENERAL;
   source.stage=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   source.access=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   source.width=source.valid_width=pool->extent.width;
   source.height=source.valid_height=pool->extent.height;
   assert(record_gpu_renderer_vulkan_record(c->renderer,c->producer[0],&source,0)==RECORD_GPU_QUEUE_OK);
   assert(c->ops.submit.end(c->producer[0])==VK_SUCCESS);
   memset(&submit,0,sizeof(submit));
   submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount=1;
   submit.pCommandBuffers=&c->producer[0];
   assert(record_gpu_renderer_vulkan_submission(c->renderer,&submit));
   assert(c->ops.submit.reset_command(c->producer[0],0)==VK_SUCCESS);
   assert(record_gpu_renderer_vulkan_discard(c->renderer));
}
static void capture_frame(struct native_encoder *c,const vulkan_encode_pool_t *pool,
      unsigned frame)
{
   struct native_capture_command *r=&c->capture_command;
   PFN_vkCmdClearColorImage clear;
   VkCommandBuffer command=c->producer[frame%2];
   const VkSubmitInfo *submission;
   VkSemaphoreWaitInfo reuse;
   PFN_vkWaitSemaphores wait_reuse;
   uint64_t previous=c->producer_completion[frame%2];
   unsigned i,retry,base=32+frame*12;
   int result;
   if (c->renderer && frame>=2)
   {
      /* Fixture command reuse, independent of encoder slot ownership. */
      wait_reuse=(PFN_vkWaitSemaphores)c->device_proc(c->device,
            c->api_version<VK_API_VERSION_1_2?"vkWaitSemaphoresKHR":"vkWaitSemaphores");
      assert(wait_reuse);
      memset(&reuse,0,sizeof(reuse));
      reuse.sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
      reuse.semaphoreCount=1;
      reuse.pSemaphores=&pool->ready;
      reuse.pValues=&previous;
      assert(wait_reuse(c->device,&reuse,2000000000)==VK_SUCCESS);
   }
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
   if (c->renderer)
   {
      for (retry=0;retry<10000;retry++)
      {
         result=record_gpu_renderer_vulkan_record(c->renderer,command,&r->source,frame);
         if (result==RECORD_GPU_QUEUE_OK) break;
         assert(result==RECORD_GPU_QUEUE_BUSY);
         native_record_pause();
      }
      assert(retry<10000);
   }
   else
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
   if (c->renderer)
   {
      /* Preserve the renderer's binary dependencies and timeline node. */
      r->binary_value=0;
      r->timeline.signalSemaphoreValueCount=1;
      r->timeline.pSignalSemaphoreValues=&r->binary_value;
      r->timeline.waitSemaphoreValueCount=frame?1:0;
      r->timeline.pWaitSemaphoreValues=frame?&r->binary_value:NULL;
      r->submit.waitSemaphoreCount=frame?1:0;
      r->submit.signalSemaphoreCount=1;
      r->submit.pWaitSemaphores=frame?&c->capture_signals[(frame-1)%2]:NULL;
      r->submit.pSignalSemaphores=&c->capture_signals[frame%2];
      r->submit.pWaitDstStageMask=frame?&r->wait_stage:NULL;
      submission=record_gpu_renderer_vulkan_submission(c->renderer,&r->submit);
      assert(submission);
      for (i=0;i<submission->signalSemaphoreCount;i++)
         if (submission->pSignalSemaphores[i]==pool->ready)
            c->producer_completion[frame%2]=((const VkTimelineSemaphoreSubmitInfo*)submission->pNext)->pSignalSemaphoreValues[i];
      assert(c->producer_completion[frame%2]>previous);
      assert(c->ops.submit.submit(c->producer_queue,1,submission,VK_NULL_HANDLE)==VK_SUCCESS);
      assert(record_gpu_renderer_vulkan_submitted(c->renderer,VK_SUCCESS));
   }
   else
      assert(c->ops.submit.submit(c->producer_queue,1,&r->submit,VK_NULL_HANDLE)==VK_SUCCESS);
}
static void capture_free(struct native_encoder *c)
{
   unsigned i;
   if (!c->capture_borrowed)
      vulkan_encode_capture_free(c->capture);
   c->capture=NULL;
   for (i=0;i<2;i++)
      if (c->capture_signals[i])
      {
         c->ops.pool.destroy_semaphore(c->device,c->capture_signals[i],NULL);
         c->capture_signals[i]=VK_NULL_HANDLE;
      }
   c->ops.pool.destroy_view(c->device,c->rgb_view,NULL);
   c->ops.pool.destroy_image(c->device,c->rgb,NULL);
   c->ops.pool.free_memory(c->device,c->rgb_memory,NULL);
}
#endif
