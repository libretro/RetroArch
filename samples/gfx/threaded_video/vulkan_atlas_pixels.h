/* Real GPU execution of the shipping atlas materialization callback. */
#include "pool_coeff.inc"
#define VK_PLANAR_HEADER 20
static VkQueue atlas_queue;
static struct vk_stream_state *vulkan_planar_state_get(vk_t *v,struct vk_texture *t)
{assert(v->stream_states->texture==t);return v->stream_states;}
static enum video_texture_update vulkan_stream_submit(vk_t *v,struct vk_stream_state *st,unsigned slot)
{
   VkSubmitInfo info;
   memset(&info,0,sizeof(info));info.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.commandBufferCount=1;info.pCommandBuffers=&st->cmd[slot];
   assert(vkResetFences(v->context->device,1,&st->fence[slot])==VK_SUCCESS);
   return vkQueueSubmit(atlas_queue,1,&info,st->fence[slot])==VK_SUCCESS
      ? VIDEO_TEXTURE_UPDATE_DONE:VIDEO_TEXTURE_UPDATE_REFUSED;
}
static void vulkan_media_texture_changed(vk_t *v,uintptr_t id,bool written)
{(void)v;(void)id;assert(written);}
#include "pool_convert.inc"

#ifndef VULKAN_ATLAS_TEST_SHADER
#define VULKAN_ATLAS_TEST_SHADER "../../../gfx/drivers/vulkan_shaders/yuv_atlas_to_rgba8888.comp.inc"
#endif
static const uint32_t atlas_shader[] =
#include VULKAN_ATLAS_TEST_SHADER
;

static void atlas_image(vk_t *v,struct vk_texture *t,unsigned w,unsigned h,bool linear)
{
   VkImageCreateInfo image;VkMemoryRequirements req;VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo view;VkImageSubresource sub;VkSubresourceLayout layout;
   memset(t,0,sizeof(*t));memset(&image,0,sizeof(image));
   t->dims=VIDEO_SCALE_PACK(w,h);t->levels=1;
   t->format=linear?VK_FORMAT_R8_UNORM:VK_FORMAT_R8G8B8A8_UNORM;
   t->layout=linear?VK_IMAGE_LAYOUT_PREINITIALIZED:VK_IMAGE_LAYOUT_UNDEFINED;
   image.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;image.imageType=VK_IMAGE_TYPE_2D;
   image.format=t->format;image.extent.width=w;image.extent.height=h;image.extent.depth=1;
   image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;
   image.tiling=linear?VK_IMAGE_TILING_LINEAR:VK_IMAGE_TILING_OPTIMAL;
   image.usage=linear?VK_IMAGE_USAGE_SAMPLED_BIT:
      VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
   image.initialLayout=t->layout;
   assert(vkCreateImage(v->context->device,&image,NULL,&t->image)==VK_SUCCESS);
   vkGetImageMemoryRequirements(v->context->device,t->image,&req);
   memset(&alloc,0,sizeof(alloc));alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize=req.size;
   alloc.memoryTypeIndex=vulkan_find_memory_type_fallback(&v->context->memory_properties,
         req.memoryTypeBits,linear?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0);
   assert(vkAllocateMemory(v->context->device,&alloc,NULL,&t->memory)==VK_SUCCESS);
   assert(vkBindImageMemory(v->context->device,t->image,t->memory,0)==VK_SUCCESS);
   memset(&view,0,sizeof(view));view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image=t->image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=t->format;
   view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount=view.subresourceRange.layerCount=1;
   assert(vkCreateImageView(v->context->device,&view,NULL,&t->view)==VK_SUCCESS);
   if(linear)
   {
      memset(&sub,0,sizeof(sub));sub.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
      vkGetImageSubresourceLayout(v->context->device,t->image,&sub,&layout);
      t->stride=layout.rowPitch;t->offset=layout.offset;
      assert(vkMapMemory(v->context->device,t->memory,0,VK_WHOLE_SIZE,0,&t->mapped)==VK_SUCCESS);
   }
}

static void atlas_read(vk_t *v,VkCommandBuffer cmd,VkQueue queue,VkBuffer buffer,void *mapped,
      struct vk_texture *texture,const struct vk_texture *source,unsigned flags)
{
   VkBufferImageCopy copy;VkMemoryBarrier host;
   const uint8_t *base=(const uint8_t*)source->mapped+source->offset;
   const image_yuv_coef_t *k=&image_yuv_coefs[flags];
   unsigned w=VIDEO_SCALE_W(texture->dims),h=VIDEO_SCALE_H(texture->dims),ch=(h+1)/2;
   unsigned x,y,c;int maximum=0;
   (void)v;
   memset(&copy,0,sizeof(copy));copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount=1;copy.imageExtent.width=w;copy.imageExtent.height=h;copy.imageExtent.depth=1;
   begin(cmd);
   VULKAN_IMAGE_LAYOUT_TRANSITION(cmd,texture->image,texture->layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
   vkCmdCopyImageToBuffer(cmd,texture->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
   VULKAN_IMAGE_LAYOUT_TRANSITION(cmd,texture->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,texture->layout,
         VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
   memset(&host,0,sizeof(host));host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,NULL,0,NULL);
   submit(queue,cmd);
   for(y=0;y<h;y++)for(x=0;x<w;x++)
   {
      int yy=k->ym*base[y*source->stride+x]+k->yb;
      int u=base[(h+y/2)*source->stride+x/2]-128;
      int cr=base[(h+ch+y/2)*source->stride+x/2]-128;
      int rgb[3];
      rgb[0]=(yy+k->rv*cr)>>IMAGE_YUV_SHIFT;
      rgb[1]=(yy+k->gu*u+k->gv*cr)>>IMAGE_YUV_SHIFT;
      rgb[2]=(yy+k->bu*u)>>IMAGE_YUV_SHIFT;
      for(c=0;c<3;c++)
      {
         int value=rgb[c]<0?0:rgb[c]>255?255:rgb[c];
         int error=abs((int)((const uint8_t*)mapped)[4*(y*w+x)+c]-value);
         if(error>maximum)maximum=error;
      }
      assert(((const uint8_t*)mapped)[4*(y*w+x)+3]==255);
   }
   assert(maximum<=1);
}

static void atlas_pixels(vk_t *v,VkQueue queue,VkCommandPool pool,VkCommandBuffer cmd,VkBuffer buffer,void *mapped)
{
   vulkan_android_pipeline_ops_t ops;vulkan_android_pipeline_t pipeline;
   VkSamplerCreateInfo sampler;VkCommandBufferAllocateInfo commands;VkFenceCreateInfo fence;
   VkFormatProperties props;struct vk_stream_state *st;
   struct vk_planar_atlas *atlas;struct vk_texture texture;
   unsigned geometry,mode,frame,x,y,w,h,ch,i;
   static const unsigned widths[3]={4,3,7},heights[3]={4,5,3};
   vkGetPhysicalDeviceFormatProperties(v->context->gpu,VK_FORMAT_R8_UNORM,&props);
   if(!(props.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
   {puts("SKIP sampled-atlas GPU materialization: no sampled linear R8 images");return;}
   atlas=(struct vk_planar_atlas*)calloc(1,sizeof(*atlas));
   st=(struct vk_stream_state*)calloc(1,sizeof(*st));assert(atlas && st);
   memset(&sampler,0,sizeof(sampler));sampler.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler.minFilter=sampler.magFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
   assert(vkCreateSampler(v->context->device,&sampler,NULL,&v->samplers.nearest)==VK_SUCCESS);
   memset(&ops,0,sizeof(ops));
   ops.create_set_layout=vkCreateDescriptorSetLayout;ops.destroy_set_layout=vkDestroyDescriptorSetLayout;
   ops.create_layout=vkCreatePipelineLayout;ops.destroy_layout=vkDestroyPipelineLayout;
   ops.create_pool=vkCreateDescriptorPool;ops.destroy_pool=vkDestroyDescriptorPool;
   ops.allocate_sets=vkAllocateDescriptorSets;ops.update_sets=vkUpdateDescriptorSets;
   ops.create_shader=vkCreateShaderModule;ops.destroy_shader=vkDestroyShaderModule;
   ops.create_pipeline=vkCreateComputePipelines;ops.destroy_pipeline=vkDestroyPipeline;
   assert(vulkan_android_pipeline_create(v->context->device,&ops,v->samplers.nearest,1,2,
         atlas_shader,sizeof(atlas_shader),&pipeline));
   v->pipelines.yuv_atlas_to_rgba8888=pipeline.pipeline;v->pipelines.layout=pipeline.layout;
   memset(&commands,0,sizeof(commands));commands.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   commands.commandPool=pool;commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=2;
   assert(vkAllocateCommandBuffers(v->context->device,&commands,st->cmd)==VK_SUCCESS);
   memset(&fence,0,sizeof(fence));fence.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;fence.flags=VK_FENCE_CREATE_SIGNALED_BIT;
   for(i=0;i<2;i++){st->set[i]=pipeline.sets[i];assert(vkCreateFence(v->context->device,&fence,NULL,&st->fence[i])==VK_SUCCESS);}
   v->stream_states=st;st->texture=&texture;atlas_queue=queue;
   for(geometry=0;geometry<3;geometry++)
   {
      struct vk_texture *source=&atlas->slots[0].texture;
      w=widths[geometry];h=heights[geometry];ch=(h+1)/2;
      atlas_image(v,source,w,h+2*ch+1,true);atlas_image(v,&texture,w,h,false);
      texture.planar_atlas=atlas;st->media_target[0]=st->media_target[1]=VK_NULL_HANDLE;
      for(mode=0;mode<4;mode++)for(frame=0;frame<4;frame++)
      {
         uint8_t *base=(uint8_t*)source->mapped+source->offset;
         unsigned slot=st->next_slot;
         for(y=0;y<h;y++)for(x=0;x<w;x++)base[y*source->stride+x]=(uint8_t)(x*31+y*23+frame*17);
         for(y=0;y<ch;y++)for(x=0;x<(w+1)/2;x++)
         {base[(h+y)*source->stride+x]=(uint8_t)(x*61+y*13+frame*47);
          base[(h+ch+y)*source->stride+x]=(uint8_t)(230-x*37-y*19-frame*17);}
         atlas->flags=(mode&1?IMAGE_YUV_FLAG_FULL_RANGE:0)|(mode&2?IMAGE_YUV_FLAG_BT709:0);atlas->pending=true;
         /* No CPU upload mapping exists in this oracle. */
         assert(!st->staging[slot].mapped);
         assert(vulkan_planar_materialize_gpu(v,&texture)==VIDEO_TEXTURE_UPDATE_DONE);
         assert(!atlas->pending && atlas->slots[0].conversion==st->fence[slot]);
         assert(vkWaitForFences(v->context->device,1,&st->fence[slot],VK_TRUE,UINT64_MAX)==VK_SUCCESS);
         atlas_read(v,cmd,queue,buffer,mapped,&texture,source,mode);
      }
      texture.planar_atlas=NULL;vkUnmapMemory(v->context->device,source->memory);source->mapped=NULL;
      vulkan_destroy_texture(v->context->device,source);vulkan_destroy_texture(v->context->device,&texture);
   }
   for(i=0;i<2;i++)vkDestroyFence(v->context->device,st->fence[i],NULL);
   vkFreeCommandBuffers(v->context->device,pool,2,st->cmd);
   vulkan_android_pipeline_destroy(v->context->device,&ops,&pipeline);
   vkDestroySampler(v->context->device,v->samplers.nearest,NULL);
   v->stream_states=NULL;free(st);free(atlas);
   puts("PASS sampled-atlas GPU materialization: 48 frames, odd/even sizes, 601/709/full/limited, no CPU upload mapping");
}
