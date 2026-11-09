/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_h264.h"
#include <stdlib.h>

struct vulkan_encode_h264
{
   const vulkan_encode_h264_ops_t *ops;
   vulkan_encode_h264_session_t *session;
   vulkan_encode_pool_t *pool;
   vulkan_encode_h264_submit_t transaction[VULKAN_ENCODE_POOL_SLOTS];
   vulkan_encode_h264_resources_t resources[VULKAN_ENCODE_POOL_SLOTS];
   vulkan_encode_packet_t packet[VULKAN_ENCODE_POOL_SLOTS];
   vulkan_encode_h264_picture_t picture[2];
   const vulkan_encode_h264_picture_t *previous;
   vulkan_encode_h264_dpb_t dpb;
   uint64_t last_ready, last_end;
   unsigned fifo[VULKAN_ENCODE_POOL_SLOTS];
   unsigned head, count, initialized, gop, qp;
   bool closing, failed, borrowed;
};

vulkan_encode_h264_t *vulkan_encode_h264_open(
      const vulkan_encode_h264_open_t *c, const vulkan_encode_h264_ops_t *ops)
{
   vulkan_encode_h264_t *e;
   vulkan_encode_pool_config_t pool;
   unsigned i;
   if (!c || !ops || !c->queue || !c->gop || c->gop > 65536 || c->qp > 51 ||
       !c->slots || c->slots > VULKAN_ENCODE_POOL_SLOTS ||
       !ops->submit.reset_command || !ops->submit.begin || !ops->submit.end ||
       !ops->submit.barrier || !ops->submit.reset_query || !ops->submit.begin_query ||
       !ops->submit.end_query || !ops->submit.begin_coding || !ops->submit.control ||
       !ops->submit.encode || !ops->submit.end_coding || !ops->submit.reset_fence ||
       !ops->submit.submit || !ops->submit.status || !ops->submit.feedback)
      return NULL;
   e = (vulkan_encode_h264_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->ops = ops;
   e->gop = c->gop;
   e->qp = c->qp;
   e->session = vulkan_encode_h264_session_new(&c->session, &ops->session);
   if (!e->session || (int32_t)c->qp < e->session->codec.minQp ||
       (int32_t)c->qp > e->session->codec.maxQp)
      goto fail;
   pool.profile = &e->session->profile;
   pool.capabilities = &e->session->capabilities;
   pool.physical = c->session.physical;
   pool.device = c->session.device;
   pool.extent.width = e->session->params.coded_width;
   pool.extent.height = e->session->params.coded_height;
   pool.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   pool.packet_size = c->packet_size;
   pool.slots = c->slots;
   pool.encode_family = c->session.family;
   pool.producer_family = c->producer_family;
   pool.storage_image_extended_formats = c->storage_image_extended_formats;
   e->pool = vulkan_encode_pool_new(&pool, &ops->pool);
   if (!e->pool)
      goto fail;
   for (i = 0; i < 2; i++)
   {
      e->dpb.image[i] = e->pool->dpb.image[i];
      e->dpb.view[i] = e->pool->dpb.view[i];
   }
   for (i = 0; i < e->pool->count; i++)
   {
      const vulkan_encode_pool_slot_t *slot = &e->pool->slot[i];
      vulkan_encode_h264_resources_t *r = &e->resources[i];
      r->dpb = &e->dpb;
      r->mapped = slot->output.mapped;
      r->capacity = (size_t)slot->output.size;
      r->queue = c->queue;
      r->command = slot->command;
      r->fence = slot->fence;
      r->queries = e->pool->queries;
      r->query = i;
      r->bitstream = slot->output.buffer;
      r->input = slot->image;
      r->input_view = slot->view;
      r->ready = e->pool->ready;
      r->done = e->pool->done;
   }
   return e;
fail:
   vulkan_encode_pool_free(e->pool);
   vulkan_encode_h264_session_free(e->session);
   free(e);
   return NULL;
}

const vulkan_encode_pool_t *vulkan_encode_h264_pool(const vulkan_encode_h264_t *e)
{
   return e ? e->pool : NULL;
}

bool vulkan_encode_h264_headers(vulkan_encode_h264_t *e,
      void *data, size_t capacity, size_t *size)
{
   return e && !e->previous && !e->closing && !e->failed &&
      vulkan_encode_h264_session_headers(e->session, data, capacity, size);
}

int vulkan_encode_h264_submit(vulkan_encode_h264_t *e, unsigned slot,
      uint64_t timestamp, uint64_t duration, uint64_t ready, bool keyframe)
{
   vulkan_encode_h264_picture_t *picture;
   vulkan_encode_h264_resources_t *r;
   vulkan_encode_packet_t *packet;
   unsigned target;
   int result;
   if (!e || e->closing || e->failed || slot >= e->pool->count || !duration ||
       timestamp > UINT64_MAX - duration || !ready)
      return VULKAN_ENCODE_ERROR;
   if (e->count == e->pool->count || e->transaction[slot].active)
      return VULKAN_ENCODE_BUSY;
   if (timestamp < e->last_end || ready <= e->last_ready)
      return VULKAN_ENCODE_ERROR;
   target = e->previous ? e->previous->slot ^ 1 : 0;
   picture = &e->picture[target];
   if (!vulkan_encode_h264_picture(picture, e->previous, target,
            e->gop, e->qp, keyframe))
      return VULKAN_ENCODE_ERROR;
   r = &e->resources[slot];
   r->ready_value = r->done_value = ready;
   result = vulkan_encode_h264_submit_start(&e->transaction[slot], e->session,
         &e->ops->submit, r, picture, e->previous, e->initialized, !e->previous);
   if (result != VULKAN_ENCODE_H264_OK)
   {
      e->failed = true;
      return VULKAN_ENCODE_ERROR;
   }
   e->previous = picture;
   e->initialized |= 1u << target;
   e->fifo[(e->head + e->count) % e->pool->count] = slot;
   e->count++;
   e->last_ready = ready;
   e->last_end = timestamp + duration;
   packet = &e->packet[slot];
   packet->timestamp = timestamp;
   packet->duration = duration;
   packet->slot = slot;
   packet->keyframe = picture->header.flags.IdrPicFlag;
   return VULKAN_ENCODE_OK;
}

int vulkan_encode_h264_poll(vulkan_encode_h264_t *e, vulkan_encode_packet_t *packet)
{
   vulkan_encode_packet_t *oldest;
   unsigned slot;
   int result;
   if (!e || !packet || e->failed)
      return VULKAN_ENCODE_ERROR;
   if (!e->count)
      return e->closing ? VULKAN_ENCODE_END : VULKAN_ENCODE_BUSY;
   slot = e->fifo[e->head];
   oldest = &e->packet[slot];
   result = vulkan_encode_h264_submit_poll(&e->transaction[slot],
         &oldest->data, &oldest->size);
   if (result == VULKAN_ENCODE_H264_ERROR)
   {
      e->failed = true;
      return VULKAN_ENCODE_ERROR;
   }
   if (result == VULKAN_ENCODE_H264_BUSY)
      return VULKAN_ENCODE_BUSY;
   e->borrowed = true;
   *packet = *oldest;
   return VULKAN_ENCODE_OK;
}

bool vulkan_encode_h264_release(vulkan_encode_h264_t *e)
{
   unsigned slot;
   if (!e || !e->borrowed || !e->count)
      return false;
   slot = e->fifo[e->head];
   if (!vulkan_encode_h264_submit_retire(&e->transaction[slot]))
      return false;
   e->packet[slot].data = NULL;
   e->packet[slot].size = 0;
   e->count--;
   e->head = (e->head + 1) % e->pool->count;
   e->borrowed = false;
   return true;
}

int vulkan_encode_h264_finish(vulkan_encode_h264_t *e)
{
   if (!e || e->failed)
      return VULKAN_ENCODE_ERROR;
   e->closing = true;
   return VULKAN_ENCODE_OK;
}

bool vulkan_encode_h264_drain(vulkan_encode_h264_t *e)
{
   unsigned i;
   bool complete = true;
   if (!e)
      return true;
   e->failed = e->closing = true;
   if (e->borrowed)
      return false;
   for (i = 0; i < e->pool->count; i++)
      if (e->transaction[i].active)
      {
         const void *data;
         size_t size;
         vulkan_encode_h264_submit_poll(&e->transaction[i], &data, &size);
         if (!vulkan_encode_h264_submit_retire(&e->transaction[i]))
            complete = false;
      }
   if (complete)
      e->head = e->count = 0;
   return complete;
}

bool vulkan_encode_h264_free(vulkan_encode_h264_t *e)
{
   unsigned i;
   if (!e)
      return true;
   if (e->count || e->borrowed)
      return false;
   for (i = 0; i < e->pool->count; i++)
      if (e->transaction[i].active)
         return false;
   vulkan_encode_pool_free(e->pool);
   vulkan_encode_h264_session_free(e->session);
   free(e);
   return true;
}
