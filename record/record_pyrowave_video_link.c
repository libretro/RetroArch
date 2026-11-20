/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_video_link.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
struct record_pyrowave_video_link
{
   record_pyrowave_video_t backend;
   uint64_t serial;
   retro_atomic_int_t paused, reader, closed;
};
/* Seq-cst fences after reader publication and pause form a two-variable
 * handshake. Either pause observes the reader, or the reader observes pause
 * before touching backend. No owner waits or spins for the other. A packet
 * loan keeps reader published through its final release. */
static int pyrowave_link_take(void *user, record_gpu_frame_t *frame)
{
   record_pyrowave_video_link_t *link = (record_pyrowave_video_link_t*)user;
   int result;
   retro_atomic_exchange_int(&link->reader, 1);
   retro_atomic_thread_fence_seq_cst();
   if (retro_atomic_load_seq_cst_int(&link->paused))
   {
      retro_atomic_exchange_int(&link->reader, 0);
      return retro_atomic_load_acquire_int(&link->closed) ? RECORD_GPU_QUEUE_END : RECORD_GPU_QUEUE_BUSY;
   }
   if (link->serial == UINT64_MAX)
   {
      retro_atomic_exchange_int(&link->reader, 0);
      return RECORD_GPU_QUEUE_ERROR;
   }
   result = link->backend.take(link->backend.user, frame);
   if (result == RECORD_GPU_QUEUE_OK) frame->ready_value = ++link->serial;
   else retro_atomic_exchange_int(&link->reader, 0);
   return result;
}
static int pyrowave_link_packet(void *user, unsigned slot, record_pyrowave_packet_t *packet)
{
   record_pyrowave_video_link_t *link = (record_pyrowave_video_link_t*)user;
   return link->backend.packet(link->backend.user, slot, packet);
}
static bool pyrowave_link_release(void *user, unsigned slot)
{
   record_pyrowave_video_link_t *link = (record_pyrowave_video_link_t*)user;
   if (!link->backend.release(link->backend.user, slot)) return false;
   retro_atomic_exchange_int(&link->reader, 0);
   return true;
}
record_pyrowave_video_link_t *record_pyrowave_video_link_new(void)
{
   record_pyrowave_video_link_t *link = (record_pyrowave_video_link_t*)calloc(1, sizeof(*link));
   if (link)
   {
      retro_atomic_int_init(&link->paused, 1);
      retro_atomic_int_init(&link->reader, 0);
      retro_atomic_int_init(&link->closed, 0);
   }
   return link;
}
void record_pyrowave_video_link_video(record_pyrowave_video_link_t *link, record_pyrowave_video_t *video)
{
   memset(video, 0, sizeof(*video));
   video->user = link; video->take = pyrowave_link_take;
   video->packet = pyrowave_link_packet; video->release = pyrowave_link_release;
}
void record_pyrowave_video_link_pause(record_pyrowave_video_link_t *link)
{
   if (!link) return;
   retro_atomic_exchange_int(&link->paused, 1);
   retro_atomic_thread_fence_seq_cst();
}
bool record_pyrowave_video_link_idle(record_pyrowave_video_link_t *link)
{
   return link && retro_atomic_load_seq_cst_int(&link->paused)
      && !retro_atomic_load_seq_cst_int(&link->reader);
}
bool record_pyrowave_video_link_bind(record_pyrowave_video_link_t *link, const record_pyrowave_video_t *video)
{
   if (!video || !video->take || !video->packet || !video->release
         || !record_pyrowave_video_link_idle(link)
         || retro_atomic_load_acquire_int(&link->closed)) return false;
   link->backend = *video;
   retro_atomic_exchange_int(&link->paused, 0);
   return true;
}
void record_pyrowave_video_link_close(record_pyrowave_video_link_t *link)
{
   if (!link) return;
   retro_atomic_store_release_int(&link->closed, 1);
   record_pyrowave_video_link_pause(link);
}
void record_pyrowave_video_link_free(record_pyrowave_video_link_t *link)
{ free(link); }
