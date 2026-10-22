/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_CACHE_TEST_MAIN cache_unit_main
#include "cache_test.c"
#undef VAAPI_CACHE_TEST_MAIN
#include "../../../gfx/video_driver.h"
#include "../../../gfx/common/vulkan_android_queue.h"

struct vk_texture
{
   vaapi_hevc_vulkan_cache_t *vaapi_cache;
   struct vk_texture *vaapi_next;
   VkImage image;
   VkImageView view;
   VkFormat format;
   VkImageLayout layout;
   unsigned dims;
};

static struct renderer_context
{
   VkPhysicalDeviceMemoryProperties memory_properties;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkInstance instance;
   VkQueue queue;
   unsigned graphics_queue_index;
   bool prime_import_device;
} renderer_context;

typedef struct vk
{
   struct renderer_context *context;
   struct vk_texture *vaapi_textures;
   vulkan_android_queue_t vaapi_submissions;
} vk_t;

static vk_t renderer;
static struct vk_texture *renderer_texture;
static vaapi_hevc_vulkan_cache_config_t renderer_ops;
static const char *renderer_missing;
static unsigned renderer_locked, renderer_submits, renderer_destinations;

static void vulkan_lock_queue(void *vk)
{
   assert(vk == &renderer && !renderer_locked);
   renderer_locked = 1;
}

static void vulkan_unlock_queue(void *vk)
{
   assert(vk == &renderer && renderer_locked);
   renderer_locked = 0;
}

static void vulkan_destroy_texture(VkDevice d, struct vk_texture *texture)
{
   assert(d == device && texture == renderer_texture);
   assert(!live && !vf_images && !vf_views && !vf_allocations);
   renderer_destinations++;
}

static VKAPI_ATTR void VKAPI_CALL renderer_update(VkDevice d, uint32_t n,
      const VkWriteDescriptorSet *writes, uint32_t copies,
      const VkCopyDescriptorSet *copy)
{
   sub = renderer_texture->vaapi_cache->entries[0].submission;
   vf  = *sub.frame;
   sub_update(d, n, writes, copies, copy);
}

static VkResult renderer_queue_submit(
      VkQueue queue, uint32_t n, const VkSubmitInfo *info, VkFence fence)
{
   assert(renderer_locked);
   renderer_submits++;
   vf = *sub.frame;
   return sub_submit(&sub, queue, n, info, fence);
}

static bool renderer_bind_io(vulkan_prime_image_ops_t *image)
{
   image->duplicate = vf_ops.duplicate;
   image->size      = vf_ops.size;
   image->close     = vf_ops.close;
   return true;
}

static PFN_vkVoidFunction renderer_get_device(VkDevice d, const char *name)
{
   assert(d == device);
   if (renderer_missing && !strcmp(renderer_missing, name))
      return NULL;
   if (!strcmp(name, "vkCreateImage"))
      return (PFN_vkVoidFunction)renderer_ops.image.create_image;
   if (!strcmp(name, "vkDestroyImage"))
      return (PFN_vkVoidFunction)renderer_ops.image.destroy_image;
   if (!strcmp(name, "vkGetImageMemoryRequirements2"))
      return (PFN_vkVoidFunction)renderer_ops.image.requirements;
   if (!strcmp(name, "vkGetMemoryFdPropertiesKHR"))
      return (PFN_vkVoidFunction)renderer_ops.image.fd_properties;
   if (!strcmp(name, "vkAllocateMemory"))
      return (PFN_vkVoidFunction)renderer_ops.image.allocate;
   if (!strcmp(name, "vkFreeMemory"))
      return (PFN_vkVoidFunction)renderer_ops.image.free_memory;
   if (!strcmp(name, "vkBindImageMemory2"))
      return (PFN_vkVoidFunction)renderer_ops.image.bind;
   if (!strcmp(name, "vkCreateImageView"))
      return (PFN_vkVoidFunction)renderer_ops.image.create_view;
   if (!strcmp(name, "vkDestroyImageView"))
      return (PFN_vkVoidFunction)renderer_ops.image.destroy_view;
   if (!strcmp(name, "vkCreateSampler"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_sampler;
   if (!strcmp(name, "vkDestroySampler"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_sampler;
   if (!strcmp(name, "vkCreateDescriptorSetLayout"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_set_layout;
   if (!strcmp(name, "vkDestroyDescriptorSetLayout"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_set_layout;
   if (!strcmp(name, "vkCreatePipelineLayout"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_layout;
   if (!strcmp(name, "vkDestroyPipelineLayout"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_layout;
   if (!strcmp(name, "vkCreateDescriptorPool"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_pool;
   if (!strcmp(name, "vkDestroyDescriptorPool"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_pool;
   if (!strcmp(name, "vkAllocateDescriptorSets"))
      return (PFN_vkVoidFunction)renderer_ops.resources.allocate_sets;
   if (!strcmp(name, "vkCreateShaderModule"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_shader;
   if (!strcmp(name, "vkDestroyShaderModule"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_shader;
   if (!strcmp(name, "vkCreateComputePipelines"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_pipeline;
   if (!strcmp(name, "vkDestroyPipeline"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_pipeline;
   if (!strcmp(name, "vkCreateCommandPool"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_commands;
   if (!strcmp(name, "vkDestroyCommandPool"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_commands;
   if (!strcmp(name, "vkAllocateCommandBuffers"))
      return (PFN_vkVoidFunction)renderer_ops.resources.allocate_commands;
   if (!strcmp(name, "vkCreateFence"))
      return (PFN_vkVoidFunction)renderer_ops.resources.create_fence;
   if (!strcmp(name, "vkDestroyFence"))
      return (PFN_vkVoidFunction)renderer_ops.resources.destroy_fence;
   if (!strcmp(name, "vkGetFenceStatus"))
      return (PFN_vkVoidFunction)renderer_ops.submit.fence_status;
   if (!strcmp(name, "vkResetFences"))
      return (PFN_vkVoidFunction)renderer_ops.submit.reset_fences;
   if (!strcmp(name, "vkResetCommandBuffer"))
      return (PFN_vkVoidFunction)renderer_ops.submit.reset_command;
   if (!strcmp(name, "vkBeginCommandBuffer"))
      return (PFN_vkVoidFunction)renderer_ops.submit.begin;
   if (!strcmp(name, "vkEndCommandBuffer"))
      return (PFN_vkVoidFunction)renderer_ops.submit.end;
   if (!strcmp(name, "vkUpdateDescriptorSets"))
      return (PFN_vkVoidFunction)renderer_ops.submit.update;
   if (!strcmp(name, "vkCmdPipelineBarrier"))
      return (PFN_vkVoidFunction)renderer_ops.submit.barrier;
   if (!strcmp(name, "vkCmdBindPipeline"))
      return (PFN_vkVoidFunction)renderer_ops.submit.bind_pipeline;
   if (!strcmp(name, "vkCmdBindDescriptorSets"))
      return (PFN_vkVoidFunction)renderer_ops.submit.bind_sets;
   if (!strcmp(name, "vkCmdPushConstants"))
      return (PFN_vkVoidFunction)renderer_ops.submit.push;
   if (!strcmp(name, "vkCmdDispatch"))
      return (PFN_vkVoidFunction)renderer_ops.submit.dispatch;
   assert(!"unexpected native function");
   return NULL;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL renderer_get_instance(
      VkInstance instance, const char *name)
{
   assert(instance == renderer_context.instance);
   if (renderer_missing && !strcmp(renderer_missing, name))
      return NULL;
   if (!strcmp(name, "vkGetPhysicalDeviceFormatProperties2"))
      return (PFN_vkVoidFunction)vf_format;
   assert(!strcmp(name, "vkGetPhysicalDeviceImageFormatProperties2"));
   return (PFN_vkVoidFunction)vf_format_image;
}

static PFN_vkGetInstanceProcAddr renderer_instance_proc(void)
{
   return renderer_missing && !strcmp(renderer_missing, "loader")
                ? NULL
                : renderer_get_instance;
}

#define vkQueueSubmit renderer_queue_submit
#define vkGetDeviceProcAddr renderer_get_device
#define vulkan_symbol_wrapper_instance_proc_addr renderer_instance_proc
#define vulkan_prime_image_bind_io renderer_bind_io
#include "../../../gfx/drivers/vulkan_vaapi_video.h"
#undef vulkan_prime_image_bind_io
#undef vulkan_symbol_wrapper_instance_proc_addr
#undef vkGetDeviceProcAddr
#undef vkQueueSubmit

static void renderer_setup(bool fp16, video_frame_import_t *input)
{
   memset(&vf, 0, sizeof(vf));
   vf_reset();
   device = vf_device;
   memset(&renderer_context, 0, sizeof(renderer_context));
   renderer_context.gpu                  = (VkPhysicalDevice)(uintptr_t)1;
   renderer_context.device               = device;
   renderer_context.instance             = (VkInstance)(uintptr_t)3;
   renderer_context.queue                = (VkQueue)(uintptr_t)7;
   renderer_context.graphics_queue_index = 2;
   renderer_context.memory_properties    = vf_memory;
   renderer_context.prime_import_device  = true;
   memset(&renderer.vaapi_submissions, 0, sizeof(renderer.vaapi_submissions));
   renderer.context                      = &renderer_context;
   renderer.vaapi_textures               = NULL;
   memset(&renderer_ops, 0, sizeof(renderer_ops));
   renderer_ops.image               = vf_ops;
   renderer_ops.resources           = ops;
   renderer_ops.submit              = sub_ops;
   renderer_ops.submit.fence_status = cache_status;
   renderer_ops.submit.barrier      = cache_barrier;
   renderer_ops.submit.update       = renderer_update;
   renderer_texture = (struct vk_texture *)calloc(1, sizeof(*renderer_texture));
   assert(renderer_texture);
   renderer_texture->image = (VkImage)(uintptr_t)500;
   renderer_texture->view  = (VkImageView)(uintptr_t)501;
   renderer_texture->format =
         fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
   renderer_texture->dims   = VIDEO_SCALE_PACK(60, 58);
   renderer_texture->layout = destination_layout = VK_IMAGE_LAYOUT_UNDEFINED;
   count                                         = 32;
   calls = next_fence = destroyed = fault = 0;
   phase = sub_fault = submissions = 0;
   renderer_submits = renderer_destinations = 0;
   renderer_missing                         = NULL;
   carrier                                  = vaapi_frame_pool_new(1);
   assert(carrier);
   cache_publish();
   video_hw_frame_abandon(&picture);
   assert(vaapi_frame_collect(carrier) == 1);
   ex_pool.ready = 1;
   ex_pool.generation[0]++;
   ex_closes = 0;
   assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
   {
      video_color_metadata_t unknown;
      vaapi_frame_view_t source;
      memset(&unknown, 0, sizeof(unknown));
      assert(!vaapi_frame_export(
            carrier, &ex_frame, 2, 2, 60, 58, 0, &unknown, &picture));
      assert(vaapi_frame_claim(picture.owner, picture.done, picture.done_value,
            picture.image, &source));
      vf_export_source = source.source;
      assert(vaapi_frame_unclaim(
            picture.owner, picture.done, picture.done_value, NULL));
   }
   memset(input, 0, sizeof(*input));
   input->color.primaries = input->color.matrix = 9;
   input->color.transfer                        = 16;
   input->color.range                           = 2;
   input->color.max_cll                         = 4000;
   input->color.max_fall                        = 350;
   input->color.mastering.present               = 0x100u;
   input->color.mastering.luminance_max         = 60000000;
   input->yuv           = IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709;
   input->image         = picture.image;
   input->done          = picture.done;
   input->done_value    = picture.done_value;
   input->owner         = picture.owner;
   input->abandon       = picture.abandon;
   input->pixfmt        = picture.pixfmt;
   input->crop_x        = 2;
   input->crop_y        = 2;
   input->dims          = renderer_texture->dims;
   input->output_pixfmt = fp16 ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888;
}

int main(void)
{
   video_frame_import_t input;
   unsigned depth, fp16, lost, prepared_calls;
   vulkan_video_layout_t expected;
   float linear = -1.0f;
   assert(!cache_unit_main());
   for (depth = 8; depth <= 10; depth += 2)
      for (fp16 = 0; fp16 < 2; fp16++)
         for (lost = 0; lost < 2; lost++)
         {
            ex_depth = depth;
            ex_mode  = 2;
            renderer_setup(fp16 != 0, &input);
            assert(vulkan_vaapi_renderer_import(&renderer, renderer_texture, &input)
                  == VIDEO_TEXTURE_UPDATE_DROPPED);
            assert(!calls && !renderer_submits && !renderer_locked);
            renderer_context.prime_import_device = false;
            assert(vulkan_vaapi_renderer_prepare(
                         &renderer, renderer_texture, &input)
                   == VIDEO_TEXTURE_UPDATE_REFUSED);
            renderer_context.prime_import_device = true;
            renderer_missing                     = "loader";
            assert(vulkan_vaapi_renderer_prepare(
                         &renderer, renderer_texture, &input)
                   == VIDEO_TEXTURE_UPDATE_REFUSED);
            renderer_missing = "vkGetMemoryFdPropertiesKHR";
            assert(vulkan_vaapi_renderer_prepare(
                         &renderer, renderer_texture, &input)
                   == VIDEO_TEXTURE_UPDATE_REFUSED);
            renderer_missing = "vkGetPhysicalDeviceFormatProperties2";
            assert(vulkan_vaapi_renderer_prepare(
                         &renderer, renderer_texture, &input)
                   == VIDEO_TEXTURE_UPDATE_REFUSED);
            assert(!live && !calls && !renderer.vaapi_textures && !ex_closes);
            renderer_missing = NULL;
            input.crop_x = 4;
            assert(vulkan_vaapi_renderer_prepare(&renderer, renderer_texture, &input)
                  == VIDEO_TEXTURE_UPDATE_DONE);
            assert(vulkan_vaapi_renderer_import(&renderer, renderer_texture, &input)
                  == VIDEO_TEXTURE_UPDATE_REFUSED);
            assert(!renderer_submits && !phase && !ex_closes);
            input.crop_x = 2;
            assert(vulkan_vaapi_renderer_prepare(&renderer, renderer_texture, &input)
                  == VIDEO_TEXTURE_UPDATE_DONE);
            prepared_calls = calls;
            renderer.vaapi_submissions.count = VULKAN_ANDROID_QUEUE_CAPACITY;
            assert(vulkan_vaapi_renderer_import(&renderer, renderer_texture, &input)
                  == VIDEO_TEXTURE_UPDATE_DROPPED);
            assert(calls == prepared_calls && !renderer_submits && !renderer_locked);
            renderer.vaapi_submissions.count = 0;
            phase = 0;
            sub_fault = lost ? 5 : 0;
            assert(vulkan_vaapi_renderer_import(
                         &renderer, renderer_texture, &input)
                   == VIDEO_TEXTURE_UPDATE_DONE);
            assert(calls == prepared_calls);
            assert(!renderer_submits && !renderer_locked && phase == 13
                  && renderer.vaapi_submissions.count == 1);
            vulkan_lock_queue(&renderer);
            assert(vulkan_android_queue_flush(&renderer.vaapi_submissions,
                  renderer_queue_submit) == (lost ? VK_ERROR_DEVICE_LOST : VK_SUCCESS));
            vulkan_unlock_queue(&renderer);
            cache = renderer_texture->vaapi_cache;
            assert(vulkan_video_layout(
                  &expected, 60, 58, input.pixfmt, input.yuv));
            assert(vulkan_video_color(&expected, input.yuv, &input.color));
            if (fp16)
               memcpy(&expected.header[19], &linear, sizeof(linear));
            assert(!memcmp(cache->entries[0].submission.header + 8,
                  expected.header + 8, 12 * sizeof(uint32_t)));
            assert(cache && renderer.vaapi_textures == renderer_texture
                   && phase == 14 && renderer_submits == 1 && !renderer_locked);
            assert(renderer_texture->layout
                   == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            memset(&vf, 0, sizeof(vf));
            vaapi_hevc_vulkan_cache_close(cache);
            if (lost)
            {
               assert(!vaapi_hevc_vulkan_cache_free(cache));
               vaapi_hevc_vulkan_cache_defer(cache);
               vaapi_hevc_vulkan_cache_device_destroying(device);
               assert(renderer_destinations == 1 && !ex_closes);
               vaapi_hevc_vulkan_cache_device_destroyed(device);
            }
            else
            {
               assert(vaapi_hevc_vulkan_cache_poll(cache) == 0);
               vf_fence_result = VK_SUCCESS;
               assert(vaapi_hevc_vulkan_cache_free(cache));
               vulkan_destroy_texture(device, renderer_texture);
               free(renderer_texture);
            }
            renderer.vaapi_textures = NULL;
            cache_collect();
         }
   puts("PASS: shipping VA Vulkan adapter admission, native function resolution, opaque frame fields, queue serialization, output state and uncertain destination retention");
   return 0;
}
