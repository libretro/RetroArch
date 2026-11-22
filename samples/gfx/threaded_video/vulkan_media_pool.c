#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#include <queues/mpsc_stack.h>
#include "gfx/video_defines.h"

#define VK_STREAM_SLOTS 2
#define VK_WIN32_VIDEO_IMPORTS 2
#define GFX_MAX_FRAME_HISTORY 2
#define VK_PLANAR_HEADER 20
#define VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT 1
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ID(value) ((unsigned)(uintptr_t)(value))
#define HANDLE(type,value) ((type)(uintptr_t)(value))
typedef unsigned vk_win32_video_import_t;
enum vk_texture_type { VULKAN_TEXTURE_STREAMED, VULKAN_TEXTURE_STATIC, VULKAN_TEXTURE_DYNAMIC };
#include "pool_types.inc"
struct context { VkDevice device; VkPhysicalDeviceMemoryProperties memory_properties; unsigned num_swapchain_images; };
typedef struct {
   struct context *context;
   mpsc_stack_t deferred_textures_in;
   mpsc_stack_node_t *deferred_textures;
   struct vk_stream_state *stream_states;
   uint64_t media_submit_serial;
   struct { VkPipeline yuv420_to_rgba8888; VkPipelineLayout layout; } pipelines;
} vk_t;
struct image { uint32_t pixels[4][32]; unsigned levels,format,destroyed; };
struct view { unsigned image,levels,deleted; };
struct operation { unsigned kind,image,source,level; };
static struct image images[32];
static struct view views[64];
static struct operation operations[2][16];
static unsigned operation_count[2],image_count,view_count,memory_count;
static unsigned allocations,blits,uploads,dispatches,changed,storage_view[2],bound_set[2];
static unsigned deleted_memory[32];
static bool fail_image,fail_submit,fail_end;
static bool ready[2]={true,true};
static uint8_t staging[2][256];
static unsigned bpp(VkFormat format){return format==VK_FORMAT_R16G16B16A16_SFLOAT?8:4;}
static VkResult vkCreateImage(VkDevice device,const VkImageCreateInfo *info,
      const VkAllocationCallbacks *allocator,VkImage *image)
{
   (void)device;(void)allocator;
   if(fail_image)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   assert(info->mipLevels && info->mipLevels<=4);
   assert(info->usage & VK_IMAGE_USAGE_SAMPLED_BIT);
   *image=HANDLE(VkImage,++image_count);assert(image_count<32);
   images[image_count].levels=info->mipLevels;images[image_count].format=info->format;
   allocations++;return VK_SUCCESS;
}
static void vkGetImageMemoryRequirements(VkDevice device,VkImage image,VkMemoryRequirements *r)
{(void)device;(void)image;memset(r,0,sizeof(*r));r->size=256;r->memoryTypeBits=1;}
static VkResult vkAllocateMemory(VkDevice device,const VkMemoryAllocateInfo *info,
      const VkAllocationCallbacks *allocator,VkDeviceMemory *memory)
{(void)device;(void)info;(void)allocator;*memory=HANDLE(VkDeviceMemory,++memory_count);return VK_SUCCESS;}
static VkResult vkBindImageMemory(VkDevice device,VkImage image,VkDeviceMemory memory,VkDeviceSize offset)
{(void)device;(void)image;(void)memory;(void)offset;return VK_SUCCESS;}
static VkResult vkCreateImageView(VkDevice device,const VkImageViewCreateInfo *info,
      const VkAllocationCallbacks *allocator,VkImageView *view)
{
   (void)device;(void)allocator;*view=HANDLE(VkImageView,++view_count);assert(view_count<64);
   views[view_count].image=ID(info->image);views[view_count].levels=info->subresourceRange.levelCount;
   return VK_SUCCESS;
}
static void vkDestroyImageView(VkDevice device,VkImageView view,const VkAllocationCallbacks *a)
{(void)device;(void)a;assert(!views[ID(view)].deleted++);}
static void vkDestroyImage(VkDevice device,VkImage image,const VkAllocationCallbacks *a)
{(void)device;(void)a;assert(!images[ID(image)].destroyed++);}
static void vkFreeMemory(VkDevice device,VkDeviceMemory memory,const VkAllocationCallbacks *a)
{(void)device;(void)a;assert(!deleted_memory[ID(memory)]++);}
static void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks *a){(void)d;(void)b;(void)a;}
static void vkUnmapMemory(VkDevice d,VkDeviceMemory m){(void)d;(void)m;}
static uint32_t vulkan_find_memory_type_fallback(const VkPhysicalDeviceMemoryProperties *p,
      uint32_t bits,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred)
{(void)p;(void)bits;(void)required;(void)preferred;return 0;}
static unsigned vulkan_format_to_bpp(VkFormat format){return bpp(format);}
static void vulkan_destroy_texture(VkDevice device,struct vk_texture *texture);
static void barrier(VkCommandBuffer cmd,VkImage image,unsigned levels,VkImageLayout old_layout,
      VkImageLayout new_layout,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags a,
      VkPipelineStageFlags b,unsigned family_a,unsigned family_b)
{(void)cmd;(void)image;(void)levels;(void)old_layout;(void)new_layout;(void)src;(void)dst;(void)a;(void)b;(void)family_a;(void)family_b;}
#define VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS barrier
#define VULKAN_IMAGE_LAYOUT_TRANSITION(cmd,image,old_layout,new_layout,src,dst,a,b) barrier(cmd,image,1,old_layout,new_layout,src,dst,a,b,0,0)
static void operation_add(VkCommandBuffer cmd,unsigned kind,unsigned image,unsigned source,unsigned level)
{
   unsigned slot=ID(cmd)-1;struct operation *o;
   assert(slot<2 && operation_count[slot]<16);o=&operations[slot][operation_count[slot]++];
   o->kind=kind;o->image=image;o->source=source;o->level=level;
}
static void vkCmdBlitImage(VkCommandBuffer cmd,VkImage source,VkImageLayout src_layout,
      VkImage destination,VkImageLayout dst_layout,uint32_t count,const VkImageBlit *blit,VkFilter filter)
{
   (void)src_layout;(void)dst_layout;(void)filter;assert(count==1 && source==destination);
   assert(blit->dstSubresource.mipLevel<images[ID(source)].levels);
   operation_add(cmd,2,ID(destination),0,blit->dstSubresource.mipLevel);blits++;
}
#include "gfx/common/vulkan_media_pool.h"
static struct vk_stream_state *vulkan_stream_state_get(vk_t *v,struct vk_texture *texture)
{assert(v->stream_states->texture==texture);return v->stream_states->pool?NULL:v->stream_states;}
static struct vk_stream_state *vulkan_planar_state_get(vk_t *v,struct vk_texture *texture)
{assert(v->stream_states->texture==texture);return v->stream_states->pool?v->stream_states:NULL;}
static VkResult vkGetFenceStatus(VkDevice d,VkFence f){(void)d;return ready[ID(f)-1]?VK_SUCCESS:VK_NOT_READY;}
static VkResult vkFlushMappedMemoryRanges(VkDevice d,uint32_t n,const VkMappedMemoryRange *r)
{(void)d;(void)n;(void)r;return VK_SUCCESS;}
static VkResult vkResetCommandBuffer(VkCommandBuffer cmd,VkCommandBufferResetFlags flags)
{(void)flags;operation_count[ID(cmd)-1]=0;return VK_SUCCESS;}
static VkResult vkBeginCommandBuffer(VkCommandBuffer cmd,const VkCommandBufferBeginInfo *info)
{(void)cmd;(void)info;return VK_SUCCESS;}
static VkResult vkEndCommandBuffer(VkCommandBuffer cmd){(void)cmd;return fail_end?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;}
static void vkCmdCopyBufferToImage(VkCommandBuffer cmd,VkBuffer buffer,VkImage image,
      VkImageLayout layout,uint32_t count,const VkBufferImageCopy *r)
{(void)layout;assert(count==1 && !r->imageSubresource.mipLevel);operation_add(cmd,1,ID(image),ID(buffer)-1,0);uploads++;}
static void vkUpdateDescriptorSets(VkDevice d,uint32_t count,const VkWriteDescriptorSet *writes,
      uint32_t copies,const VkCopyDescriptorSet *copy)
{(void)d;(void)copy;assert(count==1 && !copies && writes->dstBinding==3);storage_view[ID(writes->dstSet)-1]=ID(writes->pImageInfo->imageView);}
static void vkCmdBindPipeline(VkCommandBuffer cmd,VkPipelineBindPoint p,VkPipeline pipeline)
{(void)cmd;(void)p;(void)pipeline;}
static void vkCmdBindDescriptorSets(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipelineLayout layout,
      uint32_t first,uint32_t count,const VkDescriptorSet *set,uint32_t offsets,const uint32_t *values)
{(void)point;(void)layout;(void)first;(void)offsets;(void)values;assert(count==1);bound_set[ID(cmd)-1]=ID(*set)-1;}
static void vkCmdDispatch(VkCommandBuffer cmd,uint32_t x,uint32_t y,uint32_t z)
{(void)x;(void)y;(void)z;operation_add(cmd,3,views[storage_view[bound_set[ID(cmd)-1]]].image,ID(cmd)-1,0);dispatches++;}
void image_yuv_coefficients(unsigned flags,float coef[6])
{unsigned i;(void)flags;for(i=0;i<6;i++)coef[i]=1;}
static enum video_texture_update vulkan_stream_submit(vk_t *v,struct vk_stream_state *st,unsigned slot)
{
   unsigned i,j;(void)v;(void)st;
   if(fail_submit)return VIDEO_TEXTURE_UPDATE_REFUSED;
   for(i=0;i<operation_count[slot];i++)
   {
      struct operation *o=&operations[slot][i];struct image *image=&images[o->image];
      if(o->kind==1)memcpy(image->pixels[0],staging[o->source],16*bpp(image->format));
      else if(o->kind==2)memmove(image->pixels[o->level],image->pixels[0],bpp(image->format));
      else
      {
         uint32_t *header=(uint32_t*)staging[o->source];
         for(j=0;j<16;j++)image->pixels[0][j]=staging[o->source][header[2]+j];
      }
   }
   return VIDEO_TEXTURE_UPDATE_DONE;
}
static void vulkan_media_texture_changed(vk_t *v,uintptr_t id,bool written)
{(void)v;(void)id;assert(written);changed++;}
static unsigned cpu_frame_copies;
static void *upload_copy(void *destination,const void *source,size_t bytes)
{
   if(bytes>=16)cpu_frame_copies++;
   return memmove(destination,source,bytes);
}
#define memcpy upload_copy
#include "pool_upload.inc"
#undef memcpy
static unsigned retired;
static void vulkan_texture_retire(vk_t *v,struct vk_texture *texture)
{
   assert(!vulkan_media_pool_held(texture));
   vulkan_media_pool_free(v,texture);retired++;
}
#define VK_PARKED_TEXTURE(node) ((struct vk_texture*)((char*)(node)-offsetof(struct vk_texture,park)))
#include "pool_retire.inc"

static void setup(vk_t *v,struct vk_texture *texture,struct vk_stream_state *st,
      VkFormat format,bool planar)
{
   VkImageCreateInfo image;VkMemoryAllocateInfo memory;unsigned i;
   memset(texture,0,sizeof(*texture));memset(st,0,sizeof(*st));memset(&image,0,sizeof(image));
   image.mipLevels=planar?1:3;image.format=format;image.usage=VK_IMAGE_USAGE_SAMPLED_BIT;
   assert(vkCreateImage(v->context->device,&image,NULL,&texture->image)==VK_SUCCESS);
   memset(&memory,0,sizeof(memory));assert(vkAllocateMemory(v->context->device,&memory,NULL,&texture->memory)==VK_SUCCESS);
   texture->format=format;texture->dims=VIDEO_SCALE_PACK(4,4);texture->levels=image.mipLevels;
   texture->layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;texture->type=planar?VULKAN_TEXTURE_DYNAMIC:VULKAN_TEXTURE_STATIC;
   texture->view=vulkan_media_level_view(v,texture,texture->levels);
   st->texture=texture;st->pool=planar?HANDLE(VkDescriptorPool,1):VK_NULL_HANDLE;
   st->lent=planar?0:3;
   for(i=0;i<2;i++)
   {
      st->cmd[i]=HANDLE(VkCommandBuffer,i+1);st->fence[i]=HANDLE(VkFence,i+1);
      st->staging[i].mapped=staging[i];st->staging[i].buffer=HANDLE(VkBuffer,i+1);
      st->staging[i].stride=4*bpp(format);st->set[i]=HANDLE(VkDescriptorSet,i+1);
      st->media_target[i]=texture->view;storage_view[i]=ID(texture->view);
   }
   v->stream_states=st;v->media_submit_serial=0;
   st->import=true;vulkan_media_pool_get(v,texture);assert(!texture->media_pool);st->import=false;
   vulkan_media_pool_get(v,texture);assert(texture->media_pool && texture->media_version);
}

static void run(VkFormat format,bool planar)
{
   vk_t v;struct context context;struct vk_texture texture,overlay;
   struct vk_stream_state st;struct texture_image image;struct texture_planar planes;
   struct vk_texture *history[3]={NULL,NULL,NULL};
   struct vk_media_version *version;
   uint8_t y[16],cb[4],cr[4];uint32_t expected[3]={0,0,0};
   unsigned i,j,frame,names=0,before,shown,old,created,held,retire_before;
   memset(&v,0,sizeof(v));memset(&context,0,sizeof(context));memset(&image,0,sizeof(image));
   memset(&planes,0,sizeof(planes));v.context=&context;v.pipelines.yuv420_to_rgba8888=HANDLE(VkPipeline,1);
   context.num_swapchain_images=3;
   setup(&v,&texture,&st,format,planar);image.width=image.height=4;
   planes.planes[0]=y;planes.planes[1]=cb;planes.planes[2]=cr;
   planes.strides[0]=4;planes.strides[1]=planes.strides[2]=2;planes.chroma_step=1;
   memset(cb,128,sizeof(cb));memset(cr,128,sizeof(cr));image.planar=planar?&planes:NULL;
   overlay=texture;overlay.media_owner=&texture;
   image.pixels=(uint32_t*)staging[0];
   fail_image=true;
   assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
   assert(texture.media_pool->count==1);fail_image=false;
   ready[0]=ready[1]=false;
   assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
   ready[0]=ready[1]=true;
   for(frame=1;frame<=32;frame++)
   {
      unsigned value=planar?frame:(format==VK_FORMAT_R16G16B16A16_SFLOAT?0x4200b800u+frame:0x88004400u+frame);
      if(planar)memset(y,value,sizeof(y));
      else
      {
         uint32_t *pixels=(uint32_t*)staging[frame%2];
         for(j=0;j<16*bpp(format)/4;j++)pixels[j]=value;
         image.pixels=pixels;
      }
      before=allocations;
      assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      shown=ID(texture.image);assert(images[shown].pixels[0][0]==value);
      assert(vulkan_media_sample_texture(&overlay)->image==texture.image);
      for(i=0;i<3;i++)if(history[i])assert(images[ID(history[i]->image)].pixels[0][0]==expected[i]);
      /* Mark all reads before evicting an alias, just as the chain does. */
      for(i=0;i<3;i++)if(history[i])vulkan_media_read(history[i]);
      vulkan_media_read(&texture.media_version->texture);
      if(history[2])vulkan_media_history_release(history[2]);
      for(i=2;i;i--){history[i]=history[i-1];expected[i]=expected[i-1];}
      history[0]=&texture.media_version->texture;expected[0]=value;vulkan_media_history_retain(history[0]);
      assert(vulkan_media_pool_held(&texture));
      v.media_submit_serial++;
      if(frame==8)names=texture.media_pool->count;
      if(frame>8)assert(texture.media_pool->count==names && allocations==before);
      if(!planar)
      {
         assert(!cpu_frame_copies);
         for(i=1;i<texture.levels;i++)assert(images[shown].pixels[i][0]==value);
      }
   }
   old=ID(texture.image);created=changed;fail_submit=true;
   assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
   assert(ID(texture.image)==old && changed==created);fail_submit=false;
   fail_end=true;assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
   assert(ID(texture.image)==old);fail_end=false;
   /* Every version is either leased or read by unsubmitted commands. */
   for(version=texture.media_pool->versions;version;version=version->next)version->texture.history_refs++;
   while(texture.media_pool->count<2*(GFX_MAX_FRAME_HISTORY+1)+2)
   {
      struct vk_texture *target=vulkan_media_writable(&v,&texture);assert(target);
      target->history_refs++;
   }
   assert(vulkan_update_texture_internal(&v,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
   assert(ID(texture.image)==old);
   version=texture.media_pool->versions;assert(version->texture.history_refs==1);
   version->texture.history_refs=0;vulkan_media_read(&version->texture);
   assert(!vulkan_media_writable(&v,&texture));
   v.media_submit_serial++;assert(vulkan_media_writable(&v,&texture)==&version->texture);
   retire_before=retired;
   vulkan_deferred_textures_push(&v,&texture);
   for(i=0;i<20;i++)vulkan_deferred_textures_tick(&v);
   vulkan_deferred_textures_flush(&v);assert(retired==retire_before && texture.media_pool);
   held=0;
   for(i=0;i<3;i++){vulkan_media_history_release(history[i]);history[i]=NULL;}
   for(version=texture.media_pool->versions;version;version=version->next)
   {
      if(version->texture.history_refs)version->texture.history_refs--;
      held+=version->texture.history_refs;
   }
   assert(!held && !vulkan_media_pool_held(&texture));
   for(i=0;i<3;i++){vulkan_deferred_textures_tick(&v);assert(retired==retire_before);}
   vulkan_deferred_textures_tick(&v);assert(retired==retire_before+1 && !texture.media_pool);
}
int main(void)
{
   unsigned i;
   run(VK_FORMAT_B8G8R8A8_UNORM,false);
   run(VK_FORMAT_R16G16B16A16_SFLOAT,false);
   run(VK_FORMAT_R8G8B8A8_UNORM,true);
   assert(uploads && dispatches && blits);
   for(i=1;i<=image_count;i++)assert(images[i].destroyed==1);
   for(i=1;i<=view_count;i++)assert(views[i].deleted==1);
   for(i=1;i<=memory_count;i++)assert(deleted_memory[i]==1);
   puts("PASS Vulkan mutable pool: packed/FP16 lends, planar targets, immutable readers, mip levels, overlays, bounded reuse, failed publication and unsubmitted reads");
   return 0;
}
