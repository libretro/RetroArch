/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_audio_queue.h"
#include <stdlib.h>
#include <retro_spsc.h>
typedef struct record_audio_packet
{ uint64_t timestamp,frames; } record_audio_packet_t;
struct record_audio_queue
{
   retro_spsc_t data;
   retro_atomic_int_t closed;
   int16_t *staging;
   size_t max_frames,frame_bytes,budget,borrowed;
   uint64_t last_end,timestamp;
   bool submitted;
};
record_audio_queue_t *record_audio_queue_new(unsigned channels,
      size_t max_frames,size_t capacity_bytes)
{
   record_audio_queue_t *q;
   size_t frame_bytes=channels*2u;
   if (!channels || channels>8 || !max_frames || max_frames>65536
         || max_frames>((size_t)-1-sizeof(record_audio_packet_t))/frame_bytes
         || capacity_bytes<sizeof(record_audio_packet_t)+max_frames*frame_bytes
         || capacity_bytes>(size_t)-1/2) return NULL;
   q=(record_audio_queue_t*)calloc(1,sizeof(*q)); if (!q) return NULL;
   q->max_frames=max_frames; q->frame_bytes=frame_bytes; q->budget=capacity_bytes;
   q->staging=(int16_t*)malloc(max_frames*frame_bytes);
   if (!q->staging || !retro_spsc_init(&q->data,capacity_bytes))
   { free(q->staging); free(q); return NULL; }
   retro_atomic_int_init(&q->closed,0); return q;
}
int record_audio_queue_push(record_audio_queue_t *q,const int16_t *samples,
      size_t frames,uint64_t timestamp)
{
   record_audio_packet_t header;
   size_t bytes,space,used;
   if (!q || !samples || !frames || frames>q->max_frames
         || timestamp>((uint64_t)1<<63)-1-frames
         || (q->submitted && timestamp<q->last_end)
         || retro_atomic_load_acquire_int(&q->closed)) return RECORD_AUDIO_ERROR;
   bytes=frames*q->frame_bytes;
   space=retro_spsc_write_avail(&q->data); used=q->data.capacity-space;
   if (used>q->budget || sizeof(header)+bytes>q->budget-used) return RECORD_AUDIO_BUSY;
   header.timestamp=timestamp; header.frames=frames;
   /* The worker peeks framing and waits for the entire payload publication.
    * Only this producer can spend the space reserved by the above check. */
   if (retro_spsc_write(&q->data,&header,sizeof(header))!=sizeof(header)
         || retro_spsc_write(&q->data,samples,bytes)!=bytes) return RECORD_AUDIO_ERROR;
   q->last_end=timestamp+frames; q->submitted=true; return RECORD_AUDIO_OK;
}
bool record_audio_queue_finish(record_audio_queue_t *q)
{
   if (!q) return false;
   retro_atomic_store_release_int(&q->closed,1); return true;
}
int record_audio_queue_peek(record_audio_queue_t *q,const int16_t **samples,
      size_t *frames,uint64_t *timestamp)
{
   record_audio_packet_t header;
   size_t available,bytes;
   int closed;
   if (samples) *samples=NULL;
   if (frames) *frames=0;
   if (timestamp) *timestamp=0;
   if (!q || !samples || !frames || !timestamp) return RECORD_AUDIO_ERROR;
   if (!q->borrowed)
   {
      closed=retro_atomic_load_acquire_int(&q->closed);
      available=retro_spsc_read_avail(&q->data);
      if (!available) return closed?RECORD_AUDIO_END:RECORD_AUDIO_BUSY;
      if (available<sizeof(header)) return closed?RECORD_AUDIO_ERROR:RECORD_AUDIO_BUSY;
      if (retro_spsc_peek(&q->data,&header,sizeof(header))!=sizeof(header)
            || !header.frames || header.frames>q->max_frames
            || header.timestamp>((uint64_t)1<<63)-1-header.frames) return RECORD_AUDIO_ERROR;
      bytes=(size_t)header.frames*q->frame_bytes;
      if (available<sizeof(header)+bytes) return closed?RECORD_AUDIO_ERROR:RECORD_AUDIO_BUSY;
      if (retro_spsc_skip(&q->data,sizeof(header))!=sizeof(header)
            || retro_spsc_read(&q->data,q->staging,bytes)!=bytes) return RECORD_AUDIO_ERROR;
      q->borrowed=(size_t)header.frames; q->timestamp=header.timestamp;
   }
   *samples=q->staging; *frames=q->borrowed; *timestamp=q->timestamp;
   return RECORD_AUDIO_OK;
}
bool record_audio_queue_ack(record_audio_queue_t *q)
{
   if (!q || !q->borrowed) return false;
   q->borrowed=0; return true;
}
bool record_audio_queue_free(record_audio_queue_t *q)
{
   if (!q) return true;
   if (q->borrowed || retro_spsc_read_avail(&q->data)) return false;
   retro_spsc_free(&q->data); free(q->staging); free(q); return true;
}
