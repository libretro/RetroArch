/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_SESSION_TEST
#include "hevc_params_test.c"
#include <rthreads/rthreads.h>
#include "../../../gfx/common/vulkan_video_decode.c"
#include "../d3d11_video_decode/frontend_stubs.h"
#include "h264_decode.h"
#ifdef HEVC_RENDER_TEST
#include "hevc_render.h"
static vulkan_context_t *render_context;
#endif

typedef struct oracle_readback
{
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkCommandPool pool;
   VkCommandBuffer cb;
   VkFence fence;
   uint8_t *map;
   unsigned width, height, sample_bytes;
   VkDeviceSize uv_offset;
   int coherent;
   video_hw_frame_t held[4];
   unsigned held_count, pressured;
#ifdef HEVC_RENDER_TEST
   void *renderer;
#endif
} oracle_readback_t;

static void oracle_release(oracle_readback_t *r)
{
   unsigned i;
   for (i = 0; i < r->held_count; i++)
      r->held[i].abandon(r->held[i].owner, r->held[i].done, r->held[i].done_value);
   r->held_count = 0;
}

static void oracle_free(oracle_readback_t *r)
{
   oracle_release(r);
#ifdef HEVC_RENDER_TEST
   hevc_render_close(r->renderer);
#endif
   if (r->map) vkUnmapMemory(vvd.device, r->memory);
   if (r->pool) vkDestroyCommandPool(vvd.device, r->pool, NULL);
   if (r->fence) vkDestroyFence(vvd.device, r->fence, NULL);
   if (r->buffer) vkDestroyBuffer(vvd.device, r->buffer, NULL);
   if (r->memory) vkFreeMemory(vvd.device, r->memory, NULL);
   memset(r, 0, sizeof(*r));
}

static int oracle_init(oracle_readback_t *r, unsigned width, unsigned height, unsigned depth)
{
   VkBufferCreateInfo buffer = {0};
   VkMemoryAllocateInfo memory = {0};
   VkCommandPoolCreateInfo pool = {0};
   VkCommandBufferAllocateInfo cb = {0};
   VkFenceCreateInfo fence = {0};
   VkMemoryRequirements requirements;
   uint32_t type;
   memset(r, 0, sizeof(*r)); r->width = width; r->height = height;
   r->sample_bytes = depth == 10 ? 2 : 1;
   r->uv_offset = ((VkDeviceSize)width * height * r->sample_bytes + 3) & ~(VkDeviceSize)3;
   buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer.size = r->uv_offset + (VkDeviceSize)((width + 1) / 2) * ((height + 1) / 2) * 2 * r->sample_bytes;
   buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   if (vkCreateBuffer(vvd.device, &buffer, NULL, &r->buffer) != VK_SUCCESS) return 0;
   vkGetBufferMemoryRequirements(vvd.device, r->buffer, &requirements);
   type = vvd_memory_type(requirements.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         VK_MEMORY_PROPERTY_PROTECTED_BIT);
   r->coherent = type != UINT32_MAX;
   if (!r->coherent) type = vvd_memory_type(requirements.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_PROTECTED_BIT);
   if (type == UINT32_MAX) return 0;
   memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   memory.allocationSize = requirements.size; memory.memoryTypeIndex = type;
   if (vkAllocateMemory(vvd.device, &memory, NULL, &r->memory) != VK_SUCCESS
         || vkBindBufferMemory(vvd.device, r->buffer, r->memory, 0) != VK_SUCCESS
         || vkMapMemory(vvd.device, r->memory, 0, VK_WHOLE_SIZE, 0, (void**)&r->map) != VK_SUCCESS) return 0;
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pool.queueFamilyIndex = vvd.gfx_family;
   if (vkCreateCommandPool(vvd.device, &pool, NULL, &r->pool) != VK_SUCCESS) return 0;
   cb.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cb.commandPool = r->pool; cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount = 1;
   if (vkAllocateCommandBuffers(vvd.device, &cb, &r->cb) != VK_SUCCESS) return 0;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (vkCreateFence(vvd.device, &fence, NULL, &r->fence) != VK_SUCCESS) return 0;
#ifdef HEVC_RENDER_TEST
   r->renderer = hevc_render_open(render_context, width, height);
   if (!r->renderer) return 0;
#endif
   return 1;
}

static int oracle_frame(oracle_readback_t *r, rh265_video *source,
      const hevc_fixture_t *f, unsigned *count)
{
   video_hw_frame_t frame;
   VkImage image;
   VkSemaphore wait;
   VkBufferImageCopy regions[2];
   VkBufferMemoryBarrier barrier = {0};
   VkSubmitInfo submit = {0};
   VkQueue queue;
#ifdef HEVC_RENDER_TEST
   VkTimelineSemaphoreSubmitInfoKHR timeline = {0};
   VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
#endif
   uint32_t hash = UINT32_C(2166136261);
   unsigned plane, x, y;
   int ok = 0;
   if (rh265_video_hw_frame(source, &frame)) return 0;
   memcpy(&image, &frame.image, sizeof(image)); memcpy(&wait, &frame.wait, sizeof(wait));
   if (!frame.image || !frame.abandon || frame.pixfmt != (f->depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12)
         || frame.width != r->width || frame.height != r->height || !frame.done) goto done;
#ifndef HEVC_RENDER_TEST
   if (!vvd_wait(wait, frame.wait_value, VVD_WAIT_NS)) goto done;
#endif
   if (!vvd_begin(r->cb)) goto done;
   memset(regions, 0, sizeof(regions));
   regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
   regions[0].imageSubresource.layerCount = 1;
   regions[0].imageExtent.width = r->width; regions[0].imageExtent.height = r->height;
   regions[0].imageExtent.depth = 1;
   regions[1] = regions[0]; regions[1].bufferOffset = r->uv_offset;
   regions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   regions[1].imageExtent.width = (r->width + 1) / 2; regions[1].imageExtent.height = (r->height + 1) / 2;
   vkCmdCopyImageToBuffer(r->cb, image, (VkImageLayout)frame.layout, r->buffer, 2, regions);
   barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.buffer = r->buffer; barrier.size = VK_WHOLE_SIZE;
   vkCmdPipelineBarrier(r->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
         0, 0, NULL, 1, &barrier, 0, NULL);
   if (vkEndCommandBuffer(r->cb) != VK_SUCCESS || vkResetFences(vvd.device, 1, &r->fence) != VK_SUCCESS) goto done;
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount = 1; submit.pCommandBuffers = &r->cb;
   vkGetDeviceQueue(vvd.device, vvd.gfx_family, 0, &queue);
#ifdef HEVC_RENDER_TEST
   timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
   timeline.waitSemaphoreValueCount = 1; timeline.pWaitSemaphoreValues = &frame.wait_value;
   submit.pNext = &timeline; submit.waitSemaphoreCount = 1;
   submit.pWaitSemaphores = &wait; submit.pWaitDstStageMask = &wait_stage;
#endif
   if (vkQueueSubmit(queue, 1, &submit, r->fence) != VK_SUCCESS) goto done;
#ifdef HEVC_RENDER_TEST
   if (!hevc_render_submit(r->renderer, &frame)) goto done;
#endif
   if (vkWaitForFences(vvd.device, 1, &r->fence, VK_TRUE, VVD_WAIT_NS) != VK_SUCCESS) goto done;
   if (!r->coherent)
   {
      VkMappedMemoryRange range = {0};
      range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; range.memory = r->memory; range.size = VK_WHOLE_SIZE;
      if (vkInvalidateMappedMemoryRanges(vvd.device, 1, &range) != VK_SUCCESS) goto done;
   }
   for (plane = 0; plane < 3; plane++)
      for (y = 0; y < r->height >> (plane ? 1 : 0); y++)
         for (x = 0; x < r->width >> (plane ? 1 : 0); x++)
         {
            size_t offset = plane ? r->uv_offset + ((y * ((r->width + 1) / 2) + x) * 2 + plane - 1) * r->sample_bytes
               : (y * r->width + x) * r->sample_bytes;
            unsigned sample = r->map[offset];
            if (r->sample_bytes == 2) sample = (sample | ((unsigned)r->map[offset + 1] << 8)) >> 6;
            hash = (hash ^ (sample & 255)) * UINT32_C(16777619);
            if (r->sample_bytes == 2) hash = (hash ^ (sample >> 8)) * UINT32_C(16777619);
         }
   ok = *count < f->frames && hash == f->hash[*count];
   if (!ok) printf("%s frame %u: GPU %08x, expected %08x\n", f->name,
         *count, hash, *count < f->frames ? f->hash[*count] : 0);
#ifdef HEVC_RENDER_TEST
   if (ok && hevc_render_frame(r->renderer, &frame, r->map, (size_t)r->uv_offset, r->sample_bytes))
   {
      (*count)++;
      return 1;
   }
#else
   if (ok && r->held_count < 4)
   {
      r->held[r->held_count++] = frame;
      (*count)++;
      return 1;
   }
#endif
   ok = 0;
done:
   if (frame.abandon) frame.abandon(frame.owner, frame.done, frame.done_value);
   return ok;
}

static int decode_fixture(const hevc_fixture_t *f)
{
   rmp4_t *mp4 = rmp4_open_memory(f->data, f->size);
   rh265_video *source = rh265_video_open();
   oracle_readback_t readback = {0};
   rmp4_packet packet;
   rh265_video_params_t *params = (rh265_video_params_t*)calloc(1, sizeof(*params));
   int i, selected = -1, ret, ok = 0;
   unsigned count = 0, retries;
   if (!mp4 || !source || !params) goto done;
   for (i = 0; i < rmp4_num_tracks(mp4); i++)
   {
      const rmp4_track *track = rmp4_get_track(mp4, i);
      if (track->codec != RMP4_CODEC_H265) continue;
      if (rh265_video_set_extradata(source, track->codec_private, track->codec_private_size)) goto done;
      selected = i; break;
   }
   if (selected < 0 || rh265_video_get_params(source, 0, params)) goto done;
   if (!rh265_video_set_hw(source, 1) || !rh265_video_is_hw(source))
   {
      rh265_sequence_info_t sequence;
      rh265_video_get_sequence(source, 0, &sequence);
      printf("backend refused %s: profile %u, constraints %08x, DPB %d\n",
            f->name, sequence.profile_idc, sequence.constraint_flags_hi, params->max_dec_pic_buffering);
      goto done;
   }
   if (!oracle_init(&readback, (unsigned)(params->width - params->crop_left - params->crop_right),
            (unsigned)(params->height - params->crop_top - params->crop_bottom), f->depth)) goto done;
   while ((ret = rmp4_read_packet(mp4, &packet)) == 1)
   {
      if (packet.track != selected) continue;
      retries = 0;
      do
      {
         ret = rh265_video_decode(source, packet.data, packet.size);
         if ((ret == 1 || ret == 3) && !oracle_frame(&readback, source, f, &count)) goto done;
         if (ret == 2)
         {
            if (readback.held_count) { oracle_release(&readback); readback.pressured++; }
            else vkDeviceWaitIdle(vvd.device);
         }
         if (++retries == 1024 || ret < 0)
         { printf("%s decode returned %d at output %u\n", f->name, ret, count); goto done; }
      } while (ret == 2 || ret == 3);
   }
   if (ret < 0) goto done;
   retries = 0;
   while ((ret = rh265_video_drain(source)) >= 0)
   {
      if (!ret && !oracle_frame(&readback, source, f, &count)) goto done;
      if (ret == 2)
      {
         if (readback.held_count) { oracle_release(&readback); readback.pressured++; }
         else vkDeviceWaitIdle(vvd.device);
      }
      if (++retries == 1024) goto done;
   }
   ok = count == f->frames;
#ifndef HEVC_RENDER_TEST
   ok = ok && readback.pressured;
#endif
done:
   vkDeviceWaitIdle(vvd.device);
   rh265_video_close(source);
   if (readback.held_count && (!retro_atomic_load_acquire_ptr(&vvd.grave)
            || retro_atomic_load_acquire_int(&vvd.live))) ok = 0;
   oracle_free(&readback); rmp4_close(mp4); free(params);
#ifdef HEVC_RENDER_TEST
   printf("HEVC Vulkan decoded/imported frames %s: %s (%u frames)\n",
#else
   printf("HEVC Vulkan exported frames %s: %s (%u frames)\n",
#endif
         f->name, ok ? "PASS" : "FAIL", count);
   return ok;
}

static uint8_t *oracle_load(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   long length;
   *size = 0;
   if (!file) return NULL;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 || length > 64 * 1024 * 1024
         || fseek(file, 0, SEEK_SET)) goto done;
   data = (uint8_t*)malloc((size_t)length);
   if (!data) goto done;
   if (fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
   else *size = (size_t)length;
done:
   fclose(file);
   return data;
}

static int decode_external(void)
{
   const char *mp4_path = getenv("HEVC_CONFORMANCE_MP4"), *raw_path = getenv("HEVC_CONFORMANCE_NV12");
   hevc_fixture_t f;
   uint8_t *mp4 = NULL, *raw = NULL;
   vulkan_hevc_params_t *p = NULL;
   size_t raw_size, frame_size;
   uint32_t hash[32];
   unsigned i, plane, x, y, width, height;
   int ok = 0;
   if (!mp4_path && !raw_path) return 1;
   if (!mp4_path || !raw_path) return 0;
   memset(&f, 0, sizeof(f)); f.name = "external"; f.depth = 8;
   mp4 = oracle_load(mp4_path, &f.size); raw = oracle_load(raw_path, &raw_size); f.data = mp4;
   p = (vulkan_hevc_params_t*)calloc(1, sizeof(*p));
   if (!mp4 || !raw || !p || !build_fixture(&f, p) || p->params.bit_depth != 8) goto done;
   width = (unsigned)(p->params.width - p->params.crop_left - p->params.crop_right);
   height = (unsigned)(p->params.height - p->params.crop_top - p->params.crop_bottom);
   if (!width || !height || ((width | height) & 1)) goto done;
   frame_size = (size_t)width * height * 3 / 2;
   if (raw_size % frame_size || raw_size / frame_size > 32) goto done;
   f.frames = (unsigned)(raw_size / frame_size); f.hash = hash;
   for (i = 0; i < f.frames; i++)
   {
      const uint8_t *data = raw + frame_size * i;
      hash[i] = UINT32_C(2166136261);
      for (plane = 0; plane < 3; plane++)
         for (y = 0; y < height >> (plane ? 1 : 0); y++)
            for (x = 0; x < width >> (plane ? 1 : 0); x++)
            {
               uint8_t sample = plane ? data[width * height + (y * (width / 2) + x) * 2 + plane - 1]
                  : data[y * width + x];
               hash[i] = (hash[i] ^ sample) * UINT32_C(16777619);
            }
   }
   ok = decode_fixture(&f);
done:
   free(mp4); free(raw); free(p);
   if (!ok) puts("HEVC Vulkan external conformance: FAIL");
   return ok;
}

static int hevc_decode_probe(VkInstance instance, VkPhysicalDevice gpu,
      uint32_t family, uint32_t graphics, uint32_t codecs, PFN_vkGetInstanceProcAddr get)
{
   static const char *extensions[] = {"VK_KHR_synchronization2", "VK_KHR_video_queue",
      "VK_KHR_video_decode_queue", "VK_KHR_video_decode_h265", "VK_KHR_timeline_semaphore", "VK_KHR_video_decode_h264"};
   VkDeviceQueueCreateInfo queues[2];
   VkDeviceCreateInfo info = {0};
   VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline = {0};
   VkPhysicalDeviceSynchronization2FeaturesKHR sync = {0};
   VkDevice device;
   vulkan_context_t *ctx = (vulkan_context_t*)calloc(1, sizeof(*ctx));
   float priority = 1.0f;
   unsigned i;
   int ok = 0;
   PFN_vkCreateDevice create = (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
   vulkan_symbol_wrapper_init(get);
   if (!ctx || !vulkan_symbol_wrapper_load_core_instance_symbols(instance)) { free(ctx); return 0; }
   memset(queues, 0, sizeof(queues));
   for (i = 0; i < 2; i++)
   {
      queues[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queues[i].queueFamilyIndex = i ? graphics : family;
      queues[i].queueCount = 1; queues[i].pQueuePriorities = &priority;
   }
   timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
   timeline.timelineSemaphore = VK_TRUE;
   sync.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
   sync.synchronization2 = VK_TRUE; sync.pNext = &timeline;
   info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; info.queueCreateInfoCount = 2;
   info.pNext = &sync; info.pQueueCreateInfos = queues;
   info.enabledExtensionCount = (codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) ? 6 : 5;
   info.ppEnabledExtensionNames = extensions;
   if (!create || create(gpu, &info, NULL, &device) != VK_SUCCESS) { free(ctx); return 0; }
   if (!vulkan_symbol_wrapper_load_core_device_symbols(device)) goto done;
   ctx->instance = instance; ctx->gpu = gpu; ctx->device = device;
   ctx->graphics_queue_index = graphics; ctx->video_decode_family = family;
   ctx->video_xfer_family = graphics; ctx->video_lanes = 1;
   ctx->video_codec_operations = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
   vkGetPhysicalDeviceMemoryProperties(gpu, &ctx->memory_properties);
#ifdef HEVC_RENDER_TEST
   ctx->flags |= VK_CTX_FLAG_TIMELINE_SEMAPHORE;
   vkGetPhysicalDeviceProperties(gpu, &ctx->gpu_properties);
   vkGetDeviceQueue(device, graphics, 0, &ctx->queue);
   render_context = ctx;
#endif
   if (!vulkan_video_decode_init(ctx)) goto done;
   ok = 1;
   for (i = 0; i < 10; i++) if (!decode_fixture(&fixtures[i])) ok = 0;
   if (!decode_external()) ok = 0;
   vkDeviceWaitIdle(device);
   vulkan_video_decode_deinit();
   if (codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
   {
      ctx->video_codec_operations = codecs;
      if (!vulkan_video_decode_init(ctx) || !h264_regression()) ok = 0;
      vkDeviceWaitIdle(device); vulkan_video_decode_deinit();
   }
done:
   vkDestroyDevice(device, NULL); free(ctx);
   return ok;
}
