/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_PRIME_FRAME_TEST_MAIN frame_unit_main
#include "prime_frame_test.c"
#undef VAAPI_PRIME_FRAME_TEST_MAIN
#include <math.h>
#include "../../../gfx/common/vaapi_hevc_vulkan_submit.h"
static vaapi_hevc_vulkan_submission_t sub;
static unsigned phase, sub_fault, submissions;
static VkImageLayout destination_layout;
static VKAPI_ATTR void VKAPI_CALL sub_update(VkDevice device, uint32_t count,
      const VkWriteDescriptorSet *writes, uint32_t copies, const VkCopyDescriptorSet *copy)
{
   unsigned i;
   assert(device == vf_device && phase++ == 0 && count == 3 && !copies && !copy);
   for (i = 0; i < count; i++)
   {
      const VkDescriptorImageInfo *image = writes[i].pImageInfo;
      assert(writes[i].sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET && !writes[i].pNext
            && writes[i].dstSet == sub.set && writes[i].dstBinding == i+1 && writes[i].descriptorCount == 1);
      assert(writes[i].descriptorType == (i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
      assert(image->imageView == (i < 2 ? vf.layers[i].view : (VkImageView)(uintptr_t)501));
      assert(image->imageLayout == (i < 2 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL)
            && image->sampler == (i < 2 ? sub.sampler : VK_NULL_HANDLE));
   }
}
static VKAPI_ATTR VkResult VKAPI_CALL sub_reset(VkCommandBuffer command, VkCommandBufferResetFlags flags)
{
   assert(command == sub.command && !flags && phase++ == 1);
   return sub_fault == 1 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL sub_begin(VkCommandBuffer command, const VkCommandBufferBeginInfo *info)
{
   assert(command == sub.command && phase++ == 2 && info->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
         && info->flags == VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT && !info->pNext && !info->pInheritanceInfo);
   return sub_fault == 2 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL sub_barrier(VkCommandBuffer command, VkPipelineStageFlags src,
      VkPipelineStageFlags dst, VkDependencyFlags dependency, uint32_t memories,
      const VkMemoryBarrier *memory, uint32_t buffers, const VkBufferMemoryBarrier *buffer,
      uint32_t count, const VkImageMemoryBarrier *images)
{
   assert(phase == 3 || phase == 4 || phase == 9 || phase == 10);
   if (phase == 3 || phase == 10)
   {
      vf_acquire = phase == 3; vf_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      vf_barrier(command, src, dst, dependency, memories, memory, buffers, buffer, count, images);
   }
   else
   {
      bool before = phase == 4;
      VkAccessFlags access = destination_layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 :
         destination_layout == VK_IMAGE_LAYOUT_GENERAL ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
      assert(count == 1 && command == sub.command && !dependency && !memories && !memory && !buffers && !buffer);
      assert(images[0].image == (VkImage)(uintptr_t)500
            && images[0].srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED && images[0].dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
      assert(images[0].oldLayout == (before ? destination_layout : VK_IMAGE_LAYOUT_GENERAL)
            && images[0].newLayout == (before ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
      assert(images[0].srcAccessMask == (before ? access : VK_ACCESS_SHADER_WRITE_BIT)
            && images[0].dstAccessMask == (before ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT));
      assert(src == (before ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
            && dst == (before ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
   }
   phase++;
}
static VKAPI_ATTR void VKAPI_CALL sub_pipeline(VkCommandBuffer command, VkPipelineBindPoint point, VkPipeline pipeline)
{ assert(command == sub.command && point == VK_PIPELINE_BIND_POINT_COMPUTE && pipeline == sub.pipeline && phase++ == 5); }
static VKAPI_ATTR void VKAPI_CALL sub_sets(VkCommandBuffer command, VkPipelineBindPoint point,
      VkPipelineLayout layout, uint32_t first, uint32_t count, const VkDescriptorSet *sets,
      uint32_t dynamics, const uint32_t *offsets)
{
   assert(command == sub.command && point == VK_PIPELINE_BIND_POINT_COMPUTE && layout == sub.layout
         && !first && count == 1 && sets[0] == sub.set && !dynamics && !offsets && phase++ == 6);
}
static VKAPI_ATTR void VKAPI_CALL sub_push(VkCommandBuffer command, VkPipelineLayout layout,
      VkShaderStageFlags flags, uint32_t offset, uint32_t size, const void *data)
{
   const uint32_t *words = (const uint32_t*)data;
   float coefficient;
   assert(command == sub.command && layout == sub.layout && flags == VK_SHADER_STAGE_COMPUTE_BIT && !offset && size == 80 && phase++ == 7);
   assert(words[0] == 60 && words[1] == 58 && words[2] == 2 && words[3] == 2
         && words[14] == (ex_depth == 10 ? 2u : 1u) && words[15] == 1);
   memcpy(&coefficient, words+8, sizeof(coefficient)); assert(fabs(coefficient-1.0) < 0.000001);
   memcpy(&coefficient, words+9, sizeof(coefficient)); assert(fabs(coefficient) < 0.000001);
}
static VKAPI_ATTR void VKAPI_CALL sub_dispatch(VkCommandBuffer command, uint32_t x, uint32_t y, uint32_t z)
{ assert(command == sub.command && x == 8 && y == 8 && z == 1 && phase++ == 8); }
static VKAPI_ATTR VkResult VKAPI_CALL sub_end(VkCommandBuffer command)
{ assert(command == sub.command && phase++ == 11); return sub_fault == 3 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS; }
static VKAPI_ATTR VkResult VKAPI_CALL sub_reset_fences(VkDevice device, uint32_t count, const VkFence *fences)
{
   assert(device == sub.device && count == 1 && fences[0] == sub.fence && phase++ == 12);
   return sub_fault == 4 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VkResult sub_submit(void *context, VkQueue queue, uint32_t count, const VkSubmitInfo *info, VkFence fence)
{
   assert(context == &sub && queue == sub.queue && count == 1 && fence == sub.fence && phase++ == 13 && vf.pending);
   assert(info->sType == VK_STRUCTURE_TYPE_SUBMIT_INFO && !info->pNext && !info->waitSemaphoreCount
         && !info->signalSemaphoreCount && info->commandBufferCount == 1 && info->pCommandBuffers[0] == sub.command);
   submissions++; return sub_fault == 5 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
static const vaapi_hevc_vulkan_submit_ops_t sub_ops = { vf_status, sub_reset_fences, sub_reset, sub_begin,
   sub_end, sub_update, sub_barrier, sub_pipeline, sub_sets, sub_push, sub_dispatch, sub_submit, &sub };
static void sub_setup(void)
{
   video_color_metadata_t color;
   vf_reset(); assert(vf_create()); memset(&sub, 0, sizeof(sub)); memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1; color.transfer = 13; color.range = 2;
   assert(vaapi_hevc_vulkan_submission_configure(&sub, &vf, 2, 2, 60, 58, 0, &color, false));
   sub.ops = sub_ops; sub.queue = (VkQueue)(uintptr_t)7; sub.command = (VkCommandBuffer)(uintptr_t)8;
   sub.fence = (VkFence)(uintptr_t)9; sub.pipeline = (VkPipeline)(uintptr_t)600;
   sub.layout = (VkPipelineLayout)(uintptr_t)601; sub.set = (VkDescriptorSet)(uintptr_t)602;
   sub.sampler = (VkSampler)(uintptr_t)603; sub.family = 2;
   phase = sub_fault = submissions = 0;
}
static int sub_present(void)
{
   return vaapi_hevc_vulkan_submission_present(&sub, (VkImage)(uintptr_t)500,
         (VkImageView)(uintptr_t)501, VK_FORMAT_R8G8B8A8_UNORM, 60, 58, destination_layout);
}
int main(void)
{
   static const VkImageLayout layouts[] = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
   video_color_metadata_t color;
   unsigned i, fault;
   assert(!frame_unit_main());
   for (ex_depth = 8; ex_depth <= 10; ex_depth += 2)
      for (i = 0; i < 3; i++)
      {
         ex_mode = 0; sub_setup(); destination_layout = layouts[i];
         sub.ticket++; assert(sub_present() == -1 && !phase); sub.ticket--;
         assert(sub_present() == 1 && phase == 14 && submissions == 1 && vf.pending && ex_pool.exports == 1);
         phase = 0; assert(sub_present() == 0 && !phase && vf_fence_calls == 1);
         vf_fence_result = VK_SUCCESS;
         assert(sub_present() == 1 && submissions == 2 && vf.pending);
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1);
         assert(vaapi_hevc_vulkan_frame_detach(&vf, &vf_returned));
         phase = 0; assert(sub_present() == -1 && !phase);
         assert(vaapi_hevc_export_close(&vf_returned)); ex_closes = 0;
         ex_pool.generation[0]++;
         assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
         assert(vaapi_hevc_vulkan_frame_adopt(&vf, &ex_frame));
         assert(sub_present() == -1 && !phase);
         memset(&color, 0, sizeof(color));
         color.primaries = color.matrix = 1; color.transfer = 13; color.range = 2;
         assert(vaapi_hevc_vulkan_submission_configure(&sub, &vf, 2, 2, 60, 58, 0, &color, false));
         assert(sub_present() == 1 && submissions == 3 && vf.pending);
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1);
         assert(vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned)); vf_finish();
      }
   ex_depth = 10; ex_mode = 0;
   for (fault = 1; fault <= 5; fault++)
   {
      sub_setup(); sub_fault = fault; destination_layout = VK_IMAGE_LAYOUT_UNDEFINED;
      assert(sub_present() == (fault == 5 ? -2 : -1) && ex_pool.exports == 1);
      if (fault == 5)
      {
         assert(vf.pending && vf.broken && submissions == 1 && !vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned));
         vf_images = vf_views = vf_allocations = 0;
         assert(vaapi_hevc_vulkan_frame_after_device_destroy(&vf, vf_device, &vf_returned));
      }
      else assert(!vf.pending && !submissions && vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned));
      vf_finish();
   }
   sub_setup(); memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 9; color.transfer = 16; color.range = 1; color.max_cll = 1000;
   for (i = 0; i < 2; i++)
   {
      float transfer, mode;
      color.transfer = i ? 18 : 16;
      assert(vaapi_hevc_vulkan_submission_configure(&sub, &vf, 2, 2, 60, 58, 0, &color, true));
      memcpy(&transfer, sub.header+16, sizeof(transfer)); memcpy(&mode, sub.header+19, sizeof(mode));
      assert(transfer == (float)color.transfer && mode == -1.0f && sub.output_format == VK_FORMAT_R16G16B16A16_SFLOAT);
   }
   assert(!vaapi_hevc_vulkan_submission_configure(&sub, &vf, 1, 2, 60, 58, 0, &color, true));
   assert(!vaapi_hevc_vulkan_submission_configure(&sub, &vf, 2, 2, 64, 58, 0, &color, true));
   assert(sub_present() == -1 && !phase);
   assert(vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned)); vf_finish();
   puts("PASS: warm borrowed YUV conversion commands, cropped NV12/P010 uniforms, destination layouts, busy/stale refusal and uncertain-submit retention; no GPU execution");
   return 0;
}
