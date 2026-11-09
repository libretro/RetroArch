/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_h264_submit.h"

static void veht_image(VkImageMemoryBarrier2 *barrier, VkImage image,
      VkImageLayout old_layout, VkImageLayout layout,
      VkPipelineStageFlags2 source, VkAccessFlags2 read,
      VkPipelineStageFlags2 destination, VkAccessFlags2 write)
{
   barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   barrier->pNext = NULL;
   barrier->srcStageMask = source;
   barrier->srcAccessMask = read;
   barrier->dstStageMask = destination;
   barrier->dstAccessMask = write;
   barrier->oldLayout = old_layout;
   barrier->newLayout = layout;
   barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier->image = image;
   barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier->subresourceRange.baseMipLevel = 0;
   barrier->subresourceRange.levelCount = 1;
   barrier->subresourceRange.baseArrayLayer = 0;
   barrier->subresourceRange.layerCount = 1;
}
static void veht_resource(VkVideoPictureResourceInfoKHR *out,
      const vulkan_encode_h264_session_t *s, VkImageView view)
{
   out->sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
   out->pNext = NULL;
   out->codedOffset.x = out->codedOffset.y = 0;
   out->codedExtent.width = s->params.coded_width;
   out->codedExtent.height = s->params.coded_height;
   out->baseArrayLayer = 0;
   out->imageViewBinding = view;
}
static int veht_error(vulkan_encode_h264_submit_t *t, VkResult error, bool uncertain)
{
   t->error = error;
   t->poisoned = uncertain;
   t->gpu_complete = !uncertain;
   return VULKAN_ENCODE_H264_ERROR;
}
int vulkan_encode_h264_submit_start(vulkan_encode_h264_submit_t *t,
      const vulkan_encode_h264_session_t *s,
      const vulkan_encode_h264_submit_ops_t *ops,
      const vulkan_encode_h264_resources_t *r,
      const vulkan_encode_h264_picture_t *picture,
      const vulkan_encode_h264_picture_t *previous,
      unsigned initialized_mask, bool reset)
{
   VkResult result;
   unsigned target, reference, count;
   bool keyframe, initialized;
   if (!t)
      return VULKAN_ENCODE_H264_ERROR;
   if (t->active)
      return VULKAN_ENCODE_H264_BUSY;
   if (!s || !s->device || !s->session || !s->parameters || !r || !ops || !picture ||
       picture->slot > 1 || initialized_mask > 3 ||
       !ops->reset_command || !ops->begin || !ops->end || !ops->barrier ||
       !ops->reset_query || !ops->begin_query || !ops->end_query ||
       !ops->begin_coding || !ops->control || !ops->encode || !ops->end_coding ||
       !ops->reset_fence || !ops->submit || !ops->status || !ops->feedback ||
       !r->dpb || !r->mapped || !r->capacity || r->capacity > 64u * 1024u * 1024u ||
       !s->capabilities.minBitstreamBufferSizeAlignment ||
       r->capacity % s->capabilities.minBitstreamBufferSizeAlignment ||
       r->query == UINT32_MAX || !r->queue || !r->command || !r->fence || !r->queries || !r->bitstream ||
       !r->input || !r->input_view || !r->ready || !r->done || r->ready == r->done ||
       !r->ready_value || !r->done_value ||
       !r->dpb->image[0] || !r->dpb->image[1] || !r->dpb->view[0] || !r->dpb->view[1] ||
       r->dpb->image[0] == r->dpb->image[1] || r->dpb->view[0] == r->dpb->view[1] ||
       r->input == r->dpb->image[0] || r->input == r->dpb->image[1] ||
       r->input_view == r->dpb->view[0] || r->input_view == r->dpb->view[1] ||
       picture->header.pRefLists != &picture->lists ||
       !picture->header.flags.is_reference ||
       picture->position > 65535 ||
       picture->header.frame_num != (picture->position & 15) ||
       picture->header.PicOrderCnt != (int32_t)(2 * picture->position) ||
       picture->header.flags.IdrPicFlag != !picture->position ||
       picture->info.naluSliceEntryCount != 1 ||
       picture->info.pStdPictureInfo != &picture->header ||
       picture->info.pNaluSliceEntries != &picture->nalu ||
       picture->nalu.pStdSliceHeader != &picture->slice ||
       picture->dpb.pStdReferenceInfo != &picture->reference ||
       picture->nalu.constantQp < s->codec.minQp || picture->nalu.constantQp > s->codec.maxQp)
      return VULKAN_ENCODE_H264_ERROR;
   target = picture->slot;
   reference = target ^ 1;
   keyframe = picture->header.flags.IdrPicFlag;
   if ((!keyframe && (!previous || previous->slot != reference ||
              previous->dpb.pStdReferenceInfo != &previous->reference ||
              picture->position != previous->position + 1 ||
              picture->header.idr_pic_id != previous->header.idr_pic_id ||
              !(initialized_mask & (1u << reference)) ||
              picture->lists.RefPicList0[0] != reference)) ||
         (reset && !keyframe))
      return VULKAN_ENCODE_H264_ERROR;
   t->session = s;
   t->ops = ops;
   t->resources = r;
   t->packet = NULL;
   t->packet_size = 0;
   t->active = true;
   t->gpu_complete = t->packet_ready = t->poisoned = false;
   t->error = VK_SUCCESS;
   t->completion_status = 0;
   result = ops->reset_command(r->command, 0);
   if (result != VK_SUCCESS)
      return veht_error(t, result, false);
   t->command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   t->command.pNext = NULL;
   t->command.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   t->command.pInheritanceInfo = NULL;
   result = ops->begin(r->command, &t->command);
   if (result != VK_SUCCESS)
      return veht_error(t, result, false);
   veht_image(&t->images[0], r->input, VK_IMAGE_LAYOUT_GENERAL,
         VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0,
         VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR, VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR);
   initialized = !!(initialized_mask & (1u << target));
   veht_image(&t->images[1], r->dpb->image[target], initialized ?
         VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR,
         initialized ? VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR : VK_PIPELINE_STAGE_2_NONE,
         initialized ? VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR | VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR : 0,
         VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR, VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR);
   count = 2;
   if (!keyframe)
   {
      veht_image(&t->images[2], r->dpb->image[reference],
            VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR, VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR,
            VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR, VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR,
            VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR, VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR);
      count++;
   }
   t->buffer.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
   t->buffer.pNext = NULL;
   t->buffer.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
   t->buffer.srcAccessMask = VK_ACCESS_2_HOST_READ_BIT;
   t->buffer.dstStageMask = VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   t->buffer.dstAccessMask = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR;
   t->buffer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   t->buffer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   t->buffer.buffer = r->bitstream;
   t->buffer.offset = 0;
   t->buffer.size = r->capacity;
   t->dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   t->dependency.pNext = NULL;
   t->dependency.dependencyFlags = 0;
   t->dependency.memoryBarrierCount = 0;
   t->dependency.pMemoryBarriers = NULL;
   t->dependency.bufferMemoryBarrierCount = 1;
   t->dependency.pBufferMemoryBarriers = &t->buffer;
   t->dependency.imageMemoryBarrierCount = count;
   t->dependency.pImageMemoryBarriers = t->images;
   ops->barrier(r->command, &t->dependency);
   ops->reset_query(r->command, r->queries, r->query, 1);
   veht_resource(&t->encode.srcPictureResource, s, r->input_view);
   veht_resource(&t->dpb[target], s, r->dpb->view[target]);
   t->bound[0].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
   t->bound[0].pNext = &picture->dpb;
   t->bound[0].slotIndex = -1;
   t->bound[0].pPictureResource = &t->dpb[target];
   if (!keyframe)
   {
      veht_resource(&t->dpb[reference], s, r->dpb->view[reference]);
      t->bound[1].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
      t->bound[1].pNext = &previous->dpb;
      t->bound[1].slotIndex = (int32_t)reference;
      t->bound[1].pPictureResource = &t->dpb[reference];
   }
   t->rate.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR;
   t->rate.pNext = reset ? NULL : &picture->remaining;
   t->rate.flags = 0;
   t->rate.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
   t->rate.layerCount = 0;
   t->rate.pLayers = NULL;
   t->rate.virtualBufferSizeInMs = t->rate.initialVirtualBufferSizeInMs = 0;
   t->begin.sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
   t->begin.pNext = reset ? (const void*)&picture->remaining : (const void*)&t->rate;
   t->begin.flags = 0;
   t->begin.videoSession = s->session;
   t->begin.videoSessionParameters = s->parameters;
   t->begin.referenceSlotCount = keyframe ? 1 : 2;
   t->begin.pReferenceSlots = t->bound;
   ops->begin_coding(r->command, &t->begin);
   if (reset)
   {
      t->control.sType = VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR;
      t->control.pNext = &t->rate;
      t->control.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR |
            VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR;
      ops->control(r->command, &t->control);
   }
   t->bound[0].slotIndex = (int32_t)target;
   t->encode.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR;
   t->encode.pNext = &picture->info;
   t->encode.flags = 0;
   t->encode.dstBuffer = r->bitstream;
   t->encode.dstBufferOffset = 0;
   t->encode.dstBufferRange = r->capacity;
   t->encode.pSetupReferenceSlot = &t->bound[0];
   t->encode.referenceSlotCount = keyframe ? 0 : 1;
   t->encode.pReferenceSlots = keyframe ? NULL : &t->bound[1];
   t->encode.precedingExternallyEncodedBytes = 0;
   ops->begin_query(r->command, r->queries, r->query, 0);
   ops->encode(r->command, &t->encode);
   ops->end_query(r->command, r->queries, r->query);
   t->end.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
   t->end.pNext = NULL;
   t->end.flags = 0;
   ops->end_coding(r->command, &t->end);
   veht_image(&t->images[0], r->input, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR,
         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR,
         VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR, VK_PIPELINE_STAGE_2_NONE, 0);
   t->buffer.srcStageMask = VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   t->buffer.srcAccessMask = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR;
   t->buffer.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
   t->buffer.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
   t->dependency.imageMemoryBarrierCount = 1;
   ops->barrier(r->command, &t->dependency);
   result = ops->end(r->command);
   if (result != VK_SUCCESS)
      return veht_error(t, result, false);
   result = ops->reset_fence(s->device, 1, &r->fence);
   if (result != VK_SUCCESS)
      return veht_error(t, result, false);
   t->timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   t->timeline.pNext = NULL;
   t->timeline.waitSemaphoreValueCount = 1;
   t->timeline.pWaitSemaphoreValues = &r->ready_value;
   t->timeline.signalSemaphoreValueCount = 1;
   t->timeline.pSignalSemaphoreValues = &r->done_value;
   t->wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   t->submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   t->submit.pNext = &t->timeline;
   t->submit.waitSemaphoreCount = 1;
   t->submit.pWaitSemaphores = &r->ready;
   t->submit.pWaitDstStageMask = &t->wait_stage;
   t->submit.commandBufferCount = 1;
   t->submit.pCommandBuffers = &r->command;
   t->submit.signalSemaphoreCount = 1;
   t->submit.pSignalSemaphores = &r->done;
   result = ops->submit(r->queue, 1, &t->submit, r->fence);
   if (result != VK_SUCCESS)
   {
      /* Queue OOM leaves submitted resources untouched. */
      bool uncertain = result != VK_ERROR_OUT_OF_HOST_MEMORY &&
            result != VK_ERROR_OUT_OF_DEVICE_MEMORY;
      return veht_error(t, result, uncertain);
   }
   return VULKAN_ENCODE_H264_OK;
}
int vulkan_encode_h264_submit_poll(vulkan_encode_h264_submit_t *t,
      const void **data, size_t *size)
{
   uint32_t feedback[3];
   VkResult result;
   if (!t || !data || !size)
      return VULKAN_ENCODE_H264_ERROR;
   *data = NULL;
   *size = 0;
   if (!t->active || (t->error != VK_SUCCESS && !t->poisoned))
      return VULKAN_ENCODE_H264_ERROR;
   if (!t->gpu_complete)
   {
      result = t->ops->status(t->session->device, t->resources->fence);
      if (result == VK_NOT_READY)
         return t->error == VK_SUCCESS ? VULKAN_ENCODE_H264_BUSY : VULKAN_ENCODE_H264_ERROR;
      if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
         return veht_error(t, result, true);
      t->gpu_complete = true;
      t->poisoned = false;
      if (result == VK_ERROR_DEVICE_LOST)
         return veht_error(t, result, false);
      if (t->error != VK_SUCCESS)
         return VULKAN_ENCODE_H264_ERROR;
   }
   if (!t->packet_ready)
   {
      result = t->ops->feedback(t->session->device, t->resources->queries,
            t->resources->query, 1, sizeof(feedback), feedback, sizeof(feedback),
            VK_QUERY_RESULT_WITH_STATUS_BIT_KHR);
      if (result == VK_NOT_READY)
         return VULKAN_ENCODE_H264_BUSY;
      if (result != VK_SUCCESS)
         return veht_error(t, result, false);
      t->completion_status = (int32_t)feedback[2];
      if (!t->completion_status)
         return VULKAN_ENCODE_H264_BUSY;
      if (t->completion_status < 0 || !feedback[1] ||
          feedback[0] > t->resources->capacity ||
          feedback[1] > t->resources->capacity - feedback[0])
         return veht_error(t, VK_ERROR_UNKNOWN, false);
      t->packet = (const uint8_t*)t->resources->mapped + feedback[0];
      t->packet_size = feedback[1];
      t->packet_ready = true;
   }
   *data = t->packet;
   *size = t->packet_size;
   return VULKAN_ENCODE_H264_OK;
}
bool vulkan_encode_h264_submit_retire(vulkan_encode_h264_submit_t *t)
{
   if (!t || (t->active && (!t->gpu_complete || t->poisoned)))
      return false;
   t->active = t->packet_ready = false;
   t->session = NULL;
   t->ops = NULL;
   t->resources = NULL;
   t->packet = NULL;
   t->packet_size = 0;
   return true;
}
