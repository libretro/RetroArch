/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VULKAN_ENTRY_TEST_MAIN renderer_dependency_suite
#include "vulkan_entry_test.c"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/common/vulkan_android_cache.h"
#include <stdlib.h>
#define VK_CTX_FLAG_ANDROID_IMAGES 1
static int locked,lock_calls,pool_live,destination_destroyed;
static const char *missing_proc;
typedef struct renderer_context {
 void *queue_lock;VkDevice device;VkQueue queue;unsigned flags,graphics_queue_index,android_sampler_descriptors;
 VkPhysicalDeviceMemoryProperties memory_properties;
} renderer_context_t;
typedef struct renderer {renderer_context_t *context;} vk_t;
struct vk_texture {
 vulkan_android_cache_t *android_cache;VkFormat format;unsigned dims;
 VkImage image;VkImageView view;VkImageLayout layout;
};
static void renderer_lock(void *lock){assert(lock==&native && !locked);locked=1;lock_calls++;}
static void renderer_unlock(void *lock){assert(lock==&native && locked);locked=0;}
static void vulkan_check_device_lost(vk_t *vk,VkResult result){assert(vk && result==VK_SUCCESS);}
static void vulkan_destroy_texture(VkDevice device,struct vk_texture *texture)
{assert(device && texture && !entry_live);destination_destroyed++;}
static VkResult renderer_queue(VkQueue q,uint32_t n,const VkSubmitInfo *p,VkFence f)
{assert(locked);return submit(&native,q,n,p,f);}
static VkResult renderer_pool(VkDevice device,const VkCommandPoolCreateInfo *info,
      const VkAllocationCallbacks *allocator,VkCommandPool *pool)
{assert(device && info->flags==VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT && !allocator);*pool=H(VkCommandPool,8);pool_live++;return VK_SUCCESS;}
static VKAPI_ATTR void VKAPI_CALL renderer_pool_destroy(VkDevice device,VkCommandPool pool,const VkAllocationCallbacks *allocator)
{assert(device && pool && !allocator && !entry_live && pool_live);pool_live--;}
static PFN_vkVoidFunction renderer_proc(VkDevice device,const char *name)
{
 assert(device);
 if(missing_proc && !strcmp(missing_proc,name))return NULL;
 if(!strcmp(name,"vkGetAndroidHardwareBufferPropertiesANDROID"))return (PFN_vkVoidFunction)entry_fixture_config.image.properties;
 if(!strcmp(name,"vkCreateImage"))return (PFN_vkVoidFunction)entry_fixture_config.image.create_image;
 if(!strcmp(name,"vkDestroyImage"))return (PFN_vkVoidFunction)entry_fixture_config.image.destroy_image;
 if(!strcmp(name,"vkAllocateMemory"))return (PFN_vkVoidFunction)entry_fixture_config.image.allocate;
 if(!strcmp(name,"vkFreeMemory"))return (PFN_vkVoidFunction)entry_fixture_config.image.free_memory;
 if(!strcmp(name,"vkBindImageMemory"))return (PFN_vkVoidFunction)entry_fixture_config.image.bind;
 if(!strcmp(name,"vkCreateSamplerYcbcrConversion"))return (PFN_vkVoidFunction)entry_fixture_config.image.create_conversion;
 if(!strcmp(name,"vkDestroySamplerYcbcrConversion"))return (PFN_vkVoidFunction)entry_fixture_config.image.destroy_conversion;
 if(!strcmp(name,"vkCreateSampler"))return (PFN_vkVoidFunction)entry_fixture_config.image.create_sampler;
 if(!strcmp(name,"vkDestroySampler"))return (PFN_vkVoidFunction)entry_fixture_config.image.destroy_sampler;
 if(!strcmp(name,"vkCreateImageView"))return (PFN_vkVoidFunction)entry_fixture_config.image.create_view;
 if(!strcmp(name,"vkDestroyImageView"))return (PFN_vkVoidFunction)entry_fixture_config.image.destroy_view;
 if(!strcmp(name,"vkCreateDescriptorSetLayout"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.create_set_layout;
 if(!strcmp(name,"vkDestroyDescriptorSetLayout"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.destroy_set_layout;
 if(!strcmp(name,"vkCreatePipelineLayout"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.create_layout;
 if(!strcmp(name,"vkDestroyPipelineLayout"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.destroy_layout;
 if(!strcmp(name,"vkCreateDescriptorPool"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.create_pool;
 if(!strcmp(name,"vkDestroyDescriptorPool"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.destroy_pool;
 if(!strcmp(name,"vkAllocateDescriptorSets"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.allocate_sets;
 if(!strcmp(name,"vkUpdateDescriptorSets"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.update_sets;
 if(!strcmp(name,"vkCreateShaderModule"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.create_shader;
 if(!strcmp(name,"vkDestroyShaderModule"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.destroy_shader;
 if(!strcmp(name,"vkCreateComputePipelines"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.create_pipeline;
 if(!strcmp(name,"vkDestroyPipeline"))return (PFN_vkVoidFunction)entry_fixture_config.pipeline.destroy_pipeline;
 if(!strcmp(name,"vkCreateSemaphore"))return (PFN_vkVoidFunction)entry_fixture_config.sync.create;
 if(!strcmp(name,"vkDestroySemaphore"))return (PFN_vkVoidFunction)entry_fixture_config.sync.destroy;
 if(!strcmp(name,"vkImportSemaphoreFdKHR"))return (PFN_vkVoidFunction)entry_fixture_config.sync.import_fd;
 if(!strcmp(name,"vkGetSemaphoreFdKHR"))return (PFN_vkVoidFunction)entry_fixture_config.sync.export_fd;
 if(!strcmp(name,"vkGetFenceStatus"))return (PFN_vkVoidFunction)entry_fixture_config.submit.fence_status;
 if(!strcmp(name,"vkResetFences"))return (PFN_vkVoidFunction)entry_fixture_config.submit.reset_fences;
 if(!strcmp(name,"vkResetCommandBuffer"))return (PFN_vkVoidFunction)entry_fixture_config.submit.reset_command;
 if(!strcmp(name,"vkBeginCommandBuffer"))return (PFN_vkVoidFunction)entry_fixture_config.submit.begin;
 if(!strcmp(name,"vkEndCommandBuffer"))return (PFN_vkVoidFunction)entry_fixture_config.submit.end;
 if(!strcmp(name,"vkCmdPipelineBarrier"))return (PFN_vkVoidFunction)entry_fixture_config.submit.barrier;
 if(!strcmp(name,"vkCmdBindPipeline"))return (PFN_vkVoidFunction)entry_fixture_config.submit.bind_pipeline;
 if(!strcmp(name,"vkCmdBindDescriptorSets"))return (PFN_vkVoidFunction)entry_fixture_config.submit.bind_sets;
 if(!strcmp(name,"vkCmdPushConstants"))return (PFN_vkVoidFunction)entry_fixture_config.submit.push;
 if(!strcmp(name,"vkCmdDispatch"))return (PFN_vkVoidFunction)entry_fixture_config.submit.dispatch;
 if(!strcmp(name,"vkAllocateCommandBuffers"))return (PFN_vkVoidFunction)entry_fixture_config.entry.allocate_commands;
 if(!strcmp(name,"vkFreeCommandBuffers"))return (PFN_vkVoidFunction)entry_fixture_config.entry.free_commands;
 if(!strcmp(name,"vkCreateFence"))return (PFN_vkVoidFunction)entry_fixture_config.entry.create_fence;
 if(!strcmp(name,"vkDestroyFence"))return (PFN_vkVoidFunction)entry_fixture_config.entry.destroy_fence;
 if(!strcmp(name,"vkDestroyCommandPool"))return (PFN_vkVoidFunction)renderer_pool_destroy;
 return NULL;
}
#define slock_lock renderer_lock
#define slock_unlock renderer_unlock
#define vkQueueSubmit renderer_queue
#define vkCreateCommandPool renderer_pool
#define vkDestroyCommandPool renderer_pool_destroy
#define vkGetDeviceProcAddr renderer_proc
#include "../../../gfx/drivers/vulkan_android_video.h"
int main(void)
{
 renderer_context_t context;vk_t vk;struct vk_texture *texture;
 android_image_ops_t image_ops;android_image_queue_t *queue;android_frame_pool_t *pool;
 android_frame_view_t source;video_hw_frame_t frame;video_frame_import_t input;
 assert(!renderer_dependency_suite());
 submit_fail=export_fail=end_fail=0;fence_status=VK_SUCCESS;
 memset(&context,0,sizeof(context));context.device=entry_fixture_config.device;context.queue=entry_fixture_config.queue;
 context.flags=VK_CTX_FLAG_ANDROID_IMAGES;context.android_sampler_descriptors=3;context.queue_lock=&native;
 context.memory_properties=*entry_fixture_config.memory;vk.context=&context;
 texture=(struct vk_texture*)calloc(1,sizeof(*texture));assert(texture);
 texture->dims=VIDEO_SCALE_PACK(64,32);texture->format=VK_FORMAT_R8G8B8A8_UNORM;
 texture->image=H(VkImage,9);texture->view=H(VkImageView,9);texture->layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 context.flags=0;assert(!vulkan_android_renderer_cache(&vk,texture,false) && !pool_live);context.flags=VK_CTX_FLAG_ANDROID_IMAGES;
 missing_proc="vkImportSemaphoreFdKHR";assert(!vulkan_android_renderer_cache(&vk,texture,false) && !pool_live);missing_proc=NULL;
 memset(&image_ops,0,sizeof(image_ops));image_ops.acquire=acquire;image_ops.describe=describe;
 image_ops.delete_image=del_image;image_ops.delete_reader=del_reader;
 queue=android_image_queue_new(&native,1,&image_ops);pool=android_frame_pool_new(1);assert(queue && pool);
 memset(&source,0,sizeof(source));source.format.serial=1;source.pixfmt=IMAGE_PIXFMT_NV12;
 source.color.primaries=source.color.matrix=source.color.range=1;source.color.transfer=13;
 assert(android_image_queue_acquire(queue,&source.image)==1);assert(!android_frame_export(pool,&source,&frame));
 memset(&input,0,sizeof(input));input.image=frame.image;input.owner=frame.owner;input.done=frame.done;input.done_value=frame.done_value;
 input.abandon=frame.abandon;input.pixfmt=frame.pixfmt;input.color=frame.color;
 input.pixfmt=IMAGE_PIXFMT_P010;
 assert(vulkan_android_renderer_import(&vk,texture,&input)==VIDEO_TEXTURE_UPDATE_REFUSED && !pool_live);
 input.pixfmt=frame.pixfmt;
 assert(vulkan_android_renderer_import(&vk,texture,&input)==VIDEO_TEXTURE_UPDATE_DONE);
 assert(lock_calls==1 && !locked && pool_live==1 && entry_live);
 assert(android_image_queue_poll(queue)==1);
 assert(android_image_queue_acquire(queue,&source.image)==1);assert(!android_frame_export(pool,&source,&frame));
 input.image=frame.image;input.done=frame.done;input.done_value=frame.done_value;
 assert(vulkan_android_renderer_import(&vk,texture,&input)==VIDEO_TEXTURE_UPDATE_DROPPED && lock_calls==1);
 video_hw_frame_abandon(&frame);assert(android_image_queue_poll(queue)==1);
 vulkan_android_cache_close(texture->android_cache);
 assert(!vulkan_android_cache_free(texture->android_cache));
 vulkan_android_cache_defer(texture->android_cache);
 assert(vulkan_android_cache_device_pending(context.device));
 vulkan_android_cache_device_destroying(context.device);
 assert(!pool_live && !entry_live && destination_destroyed==1);
 vulkan_android_cache_device_destroyed(context.device);
 assert(!vulkan_android_cache_device_pending(context.device));
 android_frame_pool_close(pool);assert(android_frame_pool_free(pool));
 android_image_queue_close(queue);assert(android_image_queue_free(queue));
 puts("PASS: shipping Android renderer import, opaque validation, cold cache, queue serialization and busy lease return");
 return 0;
}
