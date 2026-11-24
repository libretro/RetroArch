/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_video_link.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct link_backend { unsigned loans; unsigned char byte; } link_backend_t;
static int link_take(void *user, record_gpu_frame_t *frame)
{
   link_backend_t *backend = (link_backend_t*)user;
   assert(!backend->loans); backend->loans++;
   memset(frame, 0, sizeof(*frame)); frame->ready_value = 1;
   return RECORD_GPU_QUEUE_OK;
}
static int link_packet(void *user, unsigned slot, record_pyrowave_packet_t *packet)
{
   link_backend_t *backend = (link_backend_t*)user;
   assert(!slot && backend->loans == 1);
   packet->data = &backend->byte; packet->bytes = 1;
   return RECORD_GPU_QUEUE_OK;
}
static bool link_release(void *user, unsigned slot)
{
   link_backend_t *backend = (link_backend_t*)user;
   assert(!slot && backend->loans == 1); backend->loans--;
   return true;
}
int main(void)
{
   record_pyrowave_video_link_t *link = record_pyrowave_video_link_new();
   record_pyrowave_video_t first, second, endpoint;
   record_gpu_frame_t frame;
   record_pyrowave_packet_t packet;
   link_backend_t a = { 0, 42 }, b = { 0, 99 };
   memset(&first, 0, sizeof(first));
   first.user = &a; first.take = link_take; first.packet = link_packet; first.release = link_release;
   second = first; second.user = &b;
   assert(link && record_pyrowave_video_link_idle(link));
   record_pyrowave_video_link_video(link, &endpoint);
   assert(endpoint.take(endpoint.user, &frame) == RECORD_GPU_QUEUE_BUSY);
   assert(record_pyrowave_video_link_bind(link, &first));
   assert(endpoint.take(endpoint.user, &frame) == RECORD_GPU_QUEUE_OK && frame.ready_value == 1);
   record_pyrowave_video_link_pause(link);
   assert(!record_pyrowave_video_link_idle(link));
   assert(!record_pyrowave_video_link_bind(link, &second));
   assert(endpoint.packet(endpoint.user, 0, &packet) == RECORD_GPU_QUEUE_OK && packet.data == &a.byte);
   assert(endpoint.release(endpoint.user, 0));
   assert(record_pyrowave_video_link_idle(link));
   /* The old backend could now be destroyed. Its ticket space may restart;
    * the wire serial must not restart with the replacement backend. */
   assert(record_pyrowave_video_link_bind(link, &second));
   assert(endpoint.take(endpoint.user, &frame) == RECORD_GPU_QUEUE_OK && frame.ready_value == 2);
   record_pyrowave_video_link_close(link);
   assert(!record_pyrowave_video_link_idle(link));
   assert(endpoint.packet(endpoint.user, 0, &packet) == RECORD_GPU_QUEUE_OK && packet.data == &b.byte);
   assert(endpoint.release(endpoint.user, 0));
   assert(record_pyrowave_video_link_idle(link));
   assert(endpoint.take(endpoint.user, &frame) == RECORD_GPU_QUEUE_END);
   assert(!record_pyrowave_video_link_bind(link, &first));
   assert(!a.loans && !b.loans);
   record_pyrowave_video_link_free(link);
   puts("PASS: renderer replacement waits for the last I/O loan; wire serial and close survive handoff");
   return 0;
}
