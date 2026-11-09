/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_encode_h264_submit.h"
#include "../../../gfx/common/vulkan_encode_h264_submit.c"
#define HANDLE(type,n) ((type)(uintptr_t)(n))
static vulkan_encode_h264_session_t session;
static vulkan_encode_h264_resources_t resources;
static vulkan_encode_h264_dpb_t dpb;
static vulkan_encode_h264_picture_t picture, previous;
static vulkan_encode_h264_submit_t transaction, saved;
static uint8_t payload[256];
static unsigned calls, results, fail_result, barriers, statuses, queries, controls;
static unsigned mask;
static bool reset_coding;
static VkResult fence_status, query_status, failure;
static uint32_t feedback_offset, feedback_bytes;
static int32_t feedback_status;
static VkResult result(void)
{
   calls++;
   return ++results == fail_result ? failure : VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_command(VkCommandBuffer command,
      VkCommandBufferResetFlags flags)
{
   assert(command == resources.command && !flags);
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL begin_command(VkCommandBuffer command,
      const VkCommandBufferBeginInfo *info)
{
   assert(command == resources.command && info->flags == VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL end_command(VkCommandBuffer command)
{
   assert(command == resources.command);
   return result();
}
static VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer command,
      const VkDependencyInfo *info)
{
   const VkImageMemoryBarrier2 *image = info->pImageMemoryBarriers;
   const VkBufferMemoryBarrier2 *buffer = info->pBufferMemoryBarriers;
   calls++;
   assert(command == resources.command);
   assert(info->bufferMemoryBarrierCount == 1 && buffer->buffer == resources.bitstream);
   assert(!buffer->offset && buffer->size == resources.capacity);
   assert(image[0].image == resources.input);
   assert(image[0].srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
   assert(image[0].dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
   if (!barriers++)
   {
      assert(info->imageMemoryBarrierCount == (picture.header.flags.IdrPicFlag ? 2 : 3));
      assert(image[0].oldLayout == VK_IMAGE_LAYOUT_GENERAL);
      assert(image[0].newLayout == VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR);
      assert(image[1].image == dpb.image[picture.slot]);
      assert(image[1].oldLayout == ((mask & (1u << picture.slot)) ?
               VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR : VK_IMAGE_LAYOUT_UNDEFINED));
      assert(buffer->srcAccessMask == VK_ACCESS_2_HOST_READ_BIT);
      assert(buffer->dstAccessMask == VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR);
      if (!picture.header.flags.IdrPicFlag)
         assert(image[2].image == dpb.image[previous.slot]);
   }
   else
   {
      assert(barriers == 2 && info->imageMemoryBarrierCount == 1);
      assert(image[0].oldLayout == VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR);
      assert(image[0].newLayout == VK_IMAGE_LAYOUT_GENERAL);
      assert(buffer->srcAccessMask == VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR);
      assert(buffer->dstAccessMask == VK_ACCESS_2_HOST_READ_BIT);
   }
}
static VKAPI_ATTR void VKAPI_CALL reset_query(VkCommandBuffer command,
      VkQueryPool pool, uint32_t first, uint32_t count)
{
   calls++;
   assert(command == resources.command && pool == resources.queries);
   assert(first == resources.query && count == 1);
}
static VKAPI_ATTR void VKAPI_CALL begin_query(VkCommandBuffer command,
      VkQueryPool pool, uint32_t query, VkQueryControlFlags flags)
{
   calls++;
   assert(command == resources.command && pool == resources.queries);
   assert(query == resources.query && !flags);
}
static VKAPI_ATTR void VKAPI_CALL end_query(VkCommandBuffer command,
      VkQueryPool pool, uint32_t query)
{
   calls++;
   assert(command == resources.command && pool == resources.queries && query == resources.query);
}
static VKAPI_ATTR void VKAPI_CALL begin_coding(VkCommandBuffer command,
      const VkVideoBeginCodingInfoKHR *info)
{
   calls++;
   assert(command == resources.command && info->videoSession == session.session);
   assert(info->videoSessionParameters == session.parameters);
   assert(info->referenceSlotCount == (picture.header.flags.IdrPicFlag ? 1 : 2));
   assert(info->pReferenceSlots[0].slotIndex == -1);
   assert(info->pReferenceSlots[0].pPictureResource->imageViewBinding == dpb.view[picture.slot]);
   if (reset_coding)
      assert(info->pNext == &picture.remaining);
   else
   {
      const VkVideoEncodeRateControlInfoKHR *rate = (const VkVideoEncodeRateControlInfoKHR*)info->pNext;
      assert(rate->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR);
      assert(rate->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR);
      assert(rate->pNext == &picture.remaining);
   }
   if (!picture.header.flags.IdrPicFlag)
   {
      assert(info->pReferenceSlots[1].slotIndex == (int32_t)previous.slot);
      assert(info->pReferenceSlots[1].pNext == &previous.dpb);
   }
}
static VKAPI_ATTR void VKAPI_CALL control(VkCommandBuffer command,
      const VkVideoCodingControlInfoKHR *info)
{
   const VkVideoEncodeRateControlInfoKHR *rate = (const VkVideoEncodeRateControlInfoKHR*)info->pNext;
   calls++; controls++;
   assert(command == resources.command && reset_coding);
   assert(info->flags == (VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR |
            VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR));
   assert(rate->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR && !rate->pNext);
}
static VKAPI_ATTR void VKAPI_CALL encode(VkCommandBuffer command,
      const VkVideoEncodeInfoKHR *info)
{
   calls++;
   assert(command == resources.command && info->pNext == &picture.info);
   assert(info->pSetupReferenceSlot->slotIndex == (int32_t)picture.slot);
   assert(info->pSetupReferenceSlot->pNext == &picture.dpb);
   assert(info->srcPictureResource.imageViewBinding == resources.input_view);
   assert(info->srcPictureResource.codedExtent.width == 192 && info->srcPictureResource.codedExtent.height == 128);
   assert(!info->dstBufferOffset && info->dstBufferRange == resources.capacity);
   assert(info->referenceSlotCount == (picture.header.flags.IdrPicFlag ? 0 : 1));
   if (!picture.header.flags.IdrPicFlag)
      assert(info->pReferenceSlots->slotIndex == (int32_t)previous.slot);
}
static VKAPI_ATTR void VKAPI_CALL end_coding(VkCommandBuffer command,
      const VkVideoEndCodingInfoKHR *info)
{
   calls++;
   assert(command == resources.command && !info->flags);
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_fence(VkDevice device,
      uint32_t count, const VkFence *fence)
{
   assert(device == session.device && count == 1 && *fence == resources.fence);
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue queue, uint32_t count,
      const VkSubmitInfo *info, VkFence fence)
{
   const VkTimelineSemaphoreSubmitInfo *timeline = (const VkTimelineSemaphoreSubmitInfo*)info->pNext;
   assert(queue == resources.queue && count == 1 && fence == resources.fence);
   assert(info->waitSemaphoreCount == 1 && *info->pWaitSemaphores == resources.ready);
   assert(*info->pWaitDstStageMask == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   assert(info->signalSemaphoreCount == 1 && *info->pSignalSemaphores == resources.done);
   assert(info->commandBufferCount == 1 && *info->pCommandBuffers == resources.command);
   assert(timeline->waitSemaphoreValueCount == 1 && *timeline->pWaitSemaphoreValues == resources.ready_value);
   assert(timeline->signalSemaphoreValueCount == 1 && *timeline->pSignalSemaphoreValues == resources.done_value);
   return result();
}
static VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice device, VkFence fence)
{
   calls++; statuses++;
   assert(device == session.device && fence == resources.fence);
   return fence_status;
}
static VKAPI_ATTR VkResult VKAPI_CALL feedback(VkDevice device, VkQueryPool pool,
      uint32_t first, uint32_t count, size_t bytes, void *data, VkDeviceSize stride,
      VkQueryResultFlags flags)
{
   uint32_t *values = (uint32_t*)data;
   calls++; queries++;
   assert(device == session.device && pool == resources.queries);
   assert(first == resources.query && count == 1 && bytes == 12 && stride == 12);
   assert(flags == VK_QUERY_RESULT_WITH_STATUS_BIT_KHR);
   if (query_status != VK_SUCCESS)
      return query_status;
   values[0] = feedback_offset; values[1] = feedback_bytes;
   values[2] = (uint32_t)feedback_status;
   return VK_SUCCESS;
}
static const vulkan_encode_h264_submit_ops_t ops = {
   reset_command, begin_command, end_command, barrier, reset_query,
   begin_query, end_query, begin_coding, control, encode, end_coding,
   reset_fence, submit, status, feedback
};
static void setup(bool predictive)
{
   assert(!transaction.active);
   memset(&transaction, 0, sizeof(transaction));
   memset(&session, 0, sizeof(session));
   memset(&resources, 0, sizeof(resources));
   calls = results = fail_result = barriers = statuses = queries = controls = 0;
   fence_status = query_status = VK_SUCCESS; failure = VK_ERROR_OUT_OF_DEVICE_MEMORY;
   feedback_offset = 16; feedback_bytes = 32; feedback_status = 1;
   session.device = HANDLE(VkDevice, 1); session.session = HANDLE(VkVideoSessionKHR, 2);
   session.parameters = HANDLE(VkVideoSessionParametersKHR, 3);
   session.params.coded_width = 192; session.params.coded_height = 128;
   session.capabilities.minBitstreamBufferSizeAlignment = 64;
   session.codec.minQp = 0; session.codec.maxQp = 51;
   resources.queue = HANDLE(VkQueue, 4); resources.command = HANDLE(VkCommandBuffer, 5);
   resources.fence = HANDLE(VkFence, 6); resources.queries = HANDLE(VkQueryPool, 7);
   resources.bitstream = HANDLE(VkBuffer, 8); resources.input = HANDLE(VkImage, 9);
   resources.input_view = HANDLE(VkImageView, 10);
   dpb.image[0] = HANDLE(VkImage, 11); dpb.image[1] = HANDLE(VkImage, 12);
   dpb.view[0] = HANDLE(VkImageView, 13); dpb.view[1] = HANDLE(VkImageView, 14);
   resources.dpb = &dpb; resources.mapped = payload; resources.capacity = sizeof(payload);
   resources.ready = HANDLE(VkSemaphore, 15); resources.done = HANDLE(VkSemaphore, 16);
   resources.ready_value = 11; resources.done_value = 12;
   assert(vulkan_encode_h264_picture(&previous, NULL, 0, 32, 12, false));
   assert(vulkan_encode_h264_picture(&picture, predictive ? &previous : NULL,
            predictive ? 1 : 0, 32, 12, false));
   mask = predictive ? 1 : 0;
   reset_coding = !predictive;
}
static int start(void)
{
   return vulkan_encode_h264_submit_start(&transaction, &session, &ops, &resources,
         &picture, picture.header.flags.IdrPicFlag ? NULL : &previous, mask, reset_coding);
}
static void refused(void)
{
   memcpy(&saved, &transaction, sizeof(saved));
   assert(start() == VULKAN_ENCODE_H264_ERROR && !calls);
   assert(!memcmp(&saved, &transaction, sizeof(saved)));
}
int main(void)
{
   const void *data, *held;
   size_t size;
   unsigned i, before;
   vulkan_encode_h264_submit_ops_t missing;
   for (i = 0; i < 2; i++)
   {
      setup(i != 0);
      assert(start() == VULKAN_ENCODE_H264_OK && results == 5 && barriers == 2);
      assert(controls == (unsigned)reset_coding);
      before = calls; memcpy(&saved, &transaction, sizeof(saved));
      assert(start() == VULKAN_ENCODE_H264_BUSY && calls == before);
      assert(!memcmp(&saved, &transaction, sizeof(saved)));
      assert(!vulkan_encode_h264_submit_retire(&transaction));
      fence_status = VK_NOT_READY;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_BUSY);
      assert(!data && !size && !queries && statuses == 1);
      fence_status = VK_SUCCESS; query_status = VK_NOT_READY;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_BUSY);
      assert(transaction.gpu_complete && !data && !size);
      query_status = VK_SUCCESS; feedback_status = 0;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_BUSY);
      feedback_status = 1;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_OK);
      assert(data == payload + 16 && size == 32); held = data; before = calls;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_OK);
      assert(data == held && size == 32 && calls == before);
      assert(vulkan_encode_h264_submit_retire(&transaction));
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
   }
   for (i = 1; i <= 5; i++)
   {
      setup(false); fail_result = i;
      assert(start() == VULKAN_ENCODE_H264_ERROR && transaction.active);
      assert(transaction.gpu_complete && !transaction.poisoned);
      assert(vulkan_encode_h264_submit_retire(&transaction));
   }
   for (i = 0; i < 2; i++)
   {
      setup(false); fail_result = 5;
      failure = i ? VK_ERROR_DEVICE_LOST : VK_ERROR_UNKNOWN;
      assert(start() == VULKAN_ENCODE_H264_ERROR && transaction.poisoned);
      fence_status = VK_NOT_READY; before = calls;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
      assert(calls == before + 1 && !queries && !vulkan_encode_h264_submit_retire(&transaction));
      fence_status = i ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
      assert(!queries && transaction.gpu_complete && !transaction.poisoned);
      assert(vulkan_encode_h264_submit_retire(&transaction));
   }
   setup(false); assert(start() == VULKAN_ENCODE_H264_OK);
   fence_status = VK_ERROR_DEVICE_LOST;
   assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
   assert(transaction.gpu_complete && vulkan_encode_h264_submit_retire(&transaction));
   setup(false); assert(start() == VULKAN_ENCODE_H264_OK);
   fence_status = VK_ERROR_OUT_OF_HOST_MEMORY;
   assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
   assert(!vulkan_encode_h264_submit_retire(&transaction));
   fence_status = VK_SUCCESS;
   assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
   assert(vulkan_encode_h264_submit_retire(&transaction));
   for (i = 0; i < 5; i++)
   {
      setup(false); assert(start() == VULKAN_ENCODE_H264_OK);
      if (!i) query_status = VK_ERROR_UNKNOWN;
      if (i == 1) feedback_status = -2;
      if (i == 2) feedback_bytes = 0;
      if (i == 3) feedback_offset = 257;
      if (i == 4) { feedback_offset = 250; feedback_bytes = UINT32_MAX; }
      assert(vulkan_encode_h264_submit_poll(&transaction, &data, &size) == VULKAN_ENCODE_H264_ERROR);
      assert(!data && !size && transaction.gpu_complete);
      assert(vulkan_encode_h264_submit_retire(&transaction));
   }
   setup(false); resources.capacity = 257; refused();
   setup(false); resources.ready = resources.done; refused();
   setup(false); resources.ready_value = 0; refused();
   setup(false); resources.input = dpb.image[0]; refused();
   setup(false); dpb.view[0] = dpb.view[1]; refused();
   setup(false); resources.mapped = NULL; refused();
   setup(false); resources.query = UINT32_MAX; refused();
   setup(false); session.capabilities.minBitstreamBufferSizeAlignment = 0; refused();
   setup(false); picture.nalu.constantQp = 52; refused();
   setup(false); picture.header.pRefLists = NULL; refused();
   setup(true); mask = 0; refused();
   setup(true); reset_coding = true; refused();
   setup(true); previous.slot = picture.slot; refused();
#define MISSING(member) do { setup(false); missing = ops; missing.member = NULL; \
   assert(vulkan_encode_h264_submit_start(&transaction, &session, &missing, &resources, \
      &picture, NULL, mask, true) == VULKAN_ENCODE_H264_ERROR && !calls); } while (0)
   MISSING(reset_command); MISSING(begin); MISSING(end); MISSING(barrier);
   MISSING(reset_query); MISSING(begin_query); MISSING(end_query);
   MISSING(begin_coding); MISSING(control); MISSING(encode); MISSING(end_coding);
   MISSING(reset_fence); MISSING(submit); MISSING(status); MISSING(feedback);
   puts("Vulkan H264 submit/timeline/fence/packet/error ownership PASS");
   return 0;
}
