/* Real Vulkan allocation, transfers and mip pixels through the shipping pool. */
#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <queues/mpsc_stack.h>
#include "gfx/video_defines.h"
#define VK_PLANAR_ATLAS_SLOTS 4
#define VK_STREAM_SLOTS 2
#define VK_WIN32_VIDEO_IMPORTS 2
#define GFX_MAX_FRAME_HISTORY 2
#define MAX(a,b) ((a)>(b)?(a):(b))
typedef unsigned vk_win32_video_import_t;
enum vk_texture_type { VULKAN_TEXTURE_STREAMED, VULKAN_TEXTURE_STATIC, VULKAN_TEXTURE_DYNAMIC };
#include "pool_types.inc"
struct context { VkDevice device; VkPhysicalDeviceMemoryProperties memory_properties; };
typedef struct { struct context *context; struct vk_stream_state *stream_states; uint64_t media_submit_serial; } vk_t;
static void RARCH_ERR(const char *text){fputs(text,stderr);}
#include "pool_memory.inc"
static void vulkan_planar_materialize(struct vk_texture *t){assert(!t->planar_atlas);}
static void vulkan_planar_atlas_free(VkDevice d,struct vk_texture *t){(void)d;assert(!t->planar_atlas);}
#include "pool_destroy.inc"
#include "pool_barrier.inc"
#include "gfx/common/vulkan_media_pool.h"
static unsigned validation_errors;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,const VkDebugUtilsMessengerCallbackDataEXT *data,void *user)
{
   (void)severity;(void)type;(void)user;validation_errors++;fputs(data->pMessage,stderr);return VK_FALSE;
}
static void begin(VkCommandBuffer cmd)
{
   VkCommandBufferBeginInfo info;
   memset(&info,0,sizeof(info));info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(vkResetCommandBuffer(cmd,0)==VK_SUCCESS);assert(vkBeginCommandBuffer(cmd,&info)==VK_SUCCESS);
}
static void submit(VkQueue queue,VkCommandBuffer cmd)
{
   VkSubmitInfo info;
   memset(&info,0,sizeof(info));info.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.commandBufferCount=1;info.pCommandBuffers=&cmd;
   assert(vkEndCommandBuffer(cmd)==VK_SUCCESS);
   assert(vkQueueSubmit(queue,1,&info,VK_NULL_HANDLE)==VK_SUCCESS);
   assert(vkQueueWaitIdle(queue)==VK_SUCCESS);
}
static void pixels(VkQueue queue,VkCommandBuffer cmd,VkBuffer buffer,void *mapped,
      const struct vk_texture *texture,unsigned level,const uint8_t *expected,unsigned bpp)
{
   VkBufferImageCopy copy;VkMemoryBarrier host;unsigned i,count=level?1:16;
   memset(&copy,0,sizeof(copy));copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.mipLevel=level;copy.imageSubresource.layerCount=1;
   copy.imageExtent.width=copy.imageExtent.height=level?1:4;copy.imageExtent.depth=1;
   begin(cmd);
   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,texture->image,texture->levels,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
   vkCmdCopyImageToBuffer(cmd,texture->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,texture->image,texture->levels,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
   memset(&host,0,sizeof(host));host.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
   vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,NULL,0,NULL);
   submit(queue,cmd);
   for(i=0;i<count;i++)assert(!memcmp((uint8_t*)mapped+i*bpp,expected,bpp));
}
static void run(vk_t *v,VkQueue queue,VkCommandBuffer cmd,VkBuffer buffer,void *mapped,VkFormat format,bool storage)
{
   struct vk_media_pool *pool=(struct vk_media_pool*)calloc(1,sizeof(*pool));
   struct vk_media_version seed,*first;
   struct vk_texture texture,*destination,*history[3]={NULL,NULL,NULL};
   uint8_t expected[3][8],current[8];VkBufferImageCopy copy;
   unsigned frame,i,bpp=format==VK_FORMAT_R16G16B16A16_SFLOAT?8:4;
   memset(&seed,0,sizeof(seed));memset(&copy,0,sizeof(copy));memset(expected,0,sizeof(expected));
   seed.texture.dims=VIDEO_SCALE_PACK(4,4);seed.texture.levels=storage?1:3;seed.texture.format=format;pool->storage=storage;
   pool->current=&seed;first=vulkan_media_version_new(v,pool);assert(first);pool->current=first;
   texture=first->texture;texture.media_pool=pool;
   copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;copy.imageSubresource.layerCount=1;
   copy.imageExtent.width=copy.imageExtent.height=4;copy.imageExtent.depth=1;
   for(frame=1;frame<=24;frame++)
   {
      uint8_t hdr[8]={0,0xb8,0,0x42,0,0x3c,0,0x3c};
      hdr[4]=(uint8_t)frame;
      memset(current,0,sizeof(current));
      if(bpp==8)memcpy(current,hdr,8);
      else{current[0]=(uint8_t)frame;current[1]=8;current[2]=120;current[3]=255;}
      for(i=0;i<16;i++)memcpy((uint8_t*)mapped+i*bpp,current,bpp);
      destination=vulkan_media_writable(v,&texture);assert(destination);
      begin(cmd);
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,destination->image,destination->levels,
         destination->layout,VK_IMAGE_LAYOUT_GENERAL,
         destination->layout==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,
         destination->layout==VK_IMAGE_LAYOUT_UNDEFINED?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
      vkCmdCopyBufferToImage(cmd,buffer,destination->image,VK_IMAGE_LAYOUT_GENERAL,1,&copy);
      vulkan_media_mipmaps(cmd,destination);
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,destination->image,destination->levels,
         VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED);
      submit(queue,cmd);vulkan_media_publish(&texture,destination);
      pixels(queue,cmd,buffer,mapped,destination,0,current,bpp);
      pixels(queue,cmd,buffer,mapped,destination,destination->levels-1,current,bpp);
      for(i=0;i<3;i++)if(history[i])pixels(queue,cmd,buffer,mapped,history[i],0,expected[i],bpp);
      for(i=0;i<3;i++)if(history[i])vulkan_media_read(history[i]);
      vulkan_media_read(destination);
      if(history[2])history[2]->history_refs--;
      for(i=2;i;i--){history[i]=history[i-1];memmove(expected[i],expected[i-1],8);}
      history[0]=destination;history[0]->history_refs++;memcpy(expected[0],current,8);
      v->media_submit_serial++;
      if(frame>6)assert(pool->count==4);
   }
   for(i=0;i<3;i++)history[i]->history_refs--;
   assert(!vulkan_media_pool_held(&texture));vulkan_media_pool_free(v,&texture);
}
int main(void)
{
   VkInstance instance;VkInstanceCreateInfo instance_info;VkApplicationInfo app;
   VkDeviceCreateInfo device_info;VkDeviceQueueCreateInfo queue_info;VkQueue queue;
   VkPhysicalDevice *gpus;VkQueueFamilyProperties *families;VkLayerProperties *layers;
   VkCommandPoolCreateInfo pool_info;VkCommandPool command_pool;VkCommandBufferAllocateInfo command_info;VkCommandBuffer cmd;
   VkBufferCreateInfo buffer_info;VkBuffer buffer;VkMemoryRequirements req;VkMemoryAllocateInfo memory_info;VkDeviceMemory memory;
   VkDebugUtilsMessengerCreateInfoEXT debug;VkDebugUtilsMessengerEXT messenger=VK_NULL_HANDLE;
   PFN_vkCreateDebugUtilsMessengerEXT create_debug;PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug;
   const char *layer="VK_LAYER_KHRONOS_validation",*extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
   float priority=1;uint32_t count,i,family=0;bool validation=false;void *mapped;
   struct context context;vk_t v;
   memset(&v,0,sizeof(v));memset(&context,0,sizeof(context));v.context=&context;
   vkEnumerateInstanceLayerProperties(&count,NULL);layers=(VkLayerProperties*)calloc(count,sizeof(*layers));
   vkEnumerateInstanceLayerProperties(&count,layers);
   for(i=0;i<count;i++)if(!strcmp(layers[i].layerName,layer))validation=true;
   free(layers);memset(&app,0,sizeof(app));app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;app.apiVersion=VK_API_VERSION_1_1;
   memset(&instance_info,0,sizeof(instance_info));instance_info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;instance_info.pApplicationInfo=&app;
   if(validation){instance_info.enabledLayerCount=1;instance_info.ppEnabledLayerNames=&layer;instance_info.enabledExtensionCount=1;instance_info.ppEnabledExtensionNames=&extension;}
   assert(vkCreateInstance(&instance_info,NULL,&instance)==VK_SUCCESS);
   memset(&debug,0,sizeof(debug));debug.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
   debug.pfnUserCallback=debug_message;
   create_debug=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
   destroy_debug=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
   if(validation)assert(create_debug(instance,&debug,NULL,&messenger)==VK_SUCCESS);
   assert(vkEnumeratePhysicalDevices(instance,&count,NULL)==VK_SUCCESS && count);
   gpus=(VkPhysicalDevice*)calloc(count,sizeof(*gpus));assert(vkEnumeratePhysicalDevices(instance,&count,gpus)==VK_SUCCESS);
   vkGetPhysicalDeviceMemoryProperties(gpus[0],&context.memory_properties);
   vkGetPhysicalDeviceQueueFamilyProperties(gpus[0],&count,NULL);families=(VkQueueFamilyProperties*)calloc(count,sizeof(*families));
   vkGetPhysicalDeviceQueueFamilyProperties(gpus[0],&count,families);
   for(i=0;i<count;i++)if(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){family=i;break;}
   free(families);memset(&queue_info,0,sizeof(queue_info));queue_info.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue_info.queueFamilyIndex=family;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
   memset(&device_info,0,sizeof(device_info));device_info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;device_info.queueCreateInfoCount=1;device_info.pQueueCreateInfos=&queue_info;
   assert(vkCreateDevice(gpus[0],&device_info,NULL,&context.device)==VK_SUCCESS);free(gpus);
   vkGetDeviceQueue(context.device,family,0,&queue);memset(&pool_info,0,sizeof(pool_info));pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool_info.queueFamilyIndex=family;
   assert(vkCreateCommandPool(context.device,&pool_info,NULL,&command_pool)==VK_SUCCESS);
   memset(&command_info,0,sizeof(command_info));command_info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command_info.commandPool=command_pool;command_info.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;command_info.commandBufferCount=1;
   assert(vkAllocateCommandBuffers(context.device,&command_info,&cmd)==VK_SUCCESS);
   memset(&buffer_info,0,sizeof(buffer_info));buffer_info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;buffer_info.size=256;
   buffer_info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   assert(vkCreateBuffer(context.device,&buffer_info,NULL,&buffer)==VK_SUCCESS);vkGetBufferMemoryRequirements(context.device,buffer,&req);
   memset(&memory_info,0,sizeof(memory_info));memory_info.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;memory_info.allocationSize=req.size;
   memory_info.memoryTypeIndex=vulkan_find_memory_type_fallback(&context.memory_properties,req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,0);
   assert(vkAllocateMemory(context.device,&memory_info,NULL,&memory)==VK_SUCCESS);
   assert(vkBindBufferMemory(context.device,buffer,memory,0)==VK_SUCCESS);assert(vkMapMemory(context.device,memory,0,256,0,&mapped)==VK_SUCCESS);
   run(&v,queue,cmd,buffer,mapped,VK_FORMAT_B8G8R8A8_UNORM,false);
   run(&v,queue,cmd,buffer,mapped,VK_FORMAT_R16G16B16A16_SFLOAT,false);
   run(&v,queue,cmd,buffer,mapped,VK_FORMAT_R8G8B8A8_UNORM,true);
   vkUnmapMemory(context.device,memory);vkDestroyBuffer(context.device,buffer,NULL);vkFreeMemory(context.device,memory,NULL);
   vkDestroyCommandPool(context.device,command_pool,NULL);vkDestroyDevice(context.device,NULL);
   if(messenger)destroy_debug(instance,messenger,NULL);
   vkDestroyInstance(instance,NULL);
   assert(!validation_errors);puts("PASS Vulkan pool GPU pixels: BGRA/FP16/storage images, negative/overwhite HDR, mips, retained history and bounded storage; validation clean");return 0;
}
