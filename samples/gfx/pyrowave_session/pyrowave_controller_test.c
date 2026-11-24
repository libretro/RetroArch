/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_session.h"
#include "../../../record/record_pyrowave_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Session controller failed line %u: %s\n", \
   (unsigned)__LINE__, #x); exit(EXIT_FAILURE); } } while (0)

typedef struct test_controller_video
{
   record_gpu_queue_t *queue;
   uint8_t bytes[3][10000];
   size_t sizes[3];
   unsigned reserved, released, cancelled, committed, fail_commit;
   unsigned short_span, fail_cancel;
   uint64_t next;
   bool ready;
} test_controller_video_t;

typedef struct test_controller_wire
{
   uint8_t bytes[32768];
   size_t read_at, write_at, direct;
   bool blocked, eof;
} test_controller_wire_t;

typedef struct test_controller_peer
{
   record_pyrowave_session_t *session;
   record_pyrowave_pcm_t *pcm;
   test_controller_video_t video;
   test_controller_wire_t *input, *output;
   size_t read_direct, write_direct;
} test_controller_peer_t;

typedef struct test_controller
{
   test_controller_peer_t peers[2];
   test_controller_wire_t wire[2];
   rpyrowave_stream_config_t config;
} test_controller_t;

static int test_controller_take(void *user, record_gpu_frame_t *frame)
{
   return record_gpu_queue_take(((test_controller_video_t*)user)->queue, frame);
}

static int test_controller_packet(void *user, unsigned slot, record_pyrowave_packet_t *packet)
{
   test_controller_video_t *v = (test_controller_video_t*)user;
   if (!v->ready) return RECORD_GPU_QUEUE_BUSY;
   CHECK(slot < 3);
   packet->data = v->bytes[slot]; packet->bytes = v->sizes[slot];
   return RECORD_GPU_QUEUE_OK;
}

static bool test_controller_release(void *user, unsigned slot)
{
   test_controller_video_t *v = (test_controller_video_t*)user;
   CHECK(v->ready && record_gpu_queue_release(v->queue, slot));
   memset(v->bytes[slot], 0xcc, sizeof(v->bytes[slot]));
   v->released++;
   return true;
}

static int test_controller_acquire(void *user, void **data, size_t *capacity)
{
   test_controller_video_t *v = (test_controller_video_t*)user;
   int result = record_gpu_queue_acquire(v->queue, &v->reserved);
   if (result == RECORD_GPU_QUEUE_OK)
   {
      *data = v->bytes[v->reserved]; *capacity = v->short_span ? 1 : sizeof(v->bytes[0]);
   }
   return result;
}

static int test_controller_submit(void *user, size_t bytes, uint64_t timestamp, uint64_t duration)
{
   test_controller_video_t *v = (test_controller_video_t*)user;
   record_gpu_frame_t frame;
   if (v->fail_commit) return RECORD_GPU_QUEUE_ERROR;
   CHECK(v->reserved < 3 && bytes <= sizeof(v->bytes[0]));
   memset(&frame, 0, sizeof(frame));
   frame.slot = v->reserved; frame.timestamp = timestamp; frame.duration = duration; frame.ready_value = ++v->next;
   v->sizes[frame.slot] = bytes;
   CHECK(record_gpu_queue_submit(v->queue, &frame));
   v->reserved = RECORD_GPU_QUEUE_SLOTS; v->committed++;
   return RECORD_GPU_QUEUE_OK;
}

static bool test_controller_cancel(void *user)
{
   test_controller_video_t *v = (test_controller_video_t*)user;
   if (v->fail_cancel) return false;
   CHECK(record_gpu_queue_cancel(v->queue));
   v->reserved = RECORD_GPU_QUEUE_SLOTS; v->cancelled++;
   return true;
}

static bool test_controller_finish(void *user)
{
   return record_gpu_queue_finish(((test_controller_video_t*)user)->queue);
}

static record_pyrowave_video_t test_controller_binding(test_controller_video_t *v)
{
   record_pyrowave_video_t result;
   memset(&result, 0, sizeof(result)); result.user = v;
   result.take = test_controller_take; result.packet = test_controller_packet; result.release = test_controller_release;
   result.acquire = test_controller_acquire; result.submit = test_controller_submit;
   result.cancel = test_controller_cancel; result.finish = test_controller_finish;
   return result;
}

static int test_controller_read(void *user, void *data, size_t bytes)
{
   test_controller_peer_t *p = (test_controller_peer_t*)user;
   test_controller_wire_t *w = p->input;
   unsigned i;
   if (w->eof) return -2;
   if (w->blocked) return 0;
   if (bytes > 29) bytes = 29;
   if (bytes > w->write_at - w->read_at) bytes = w->write_at - w->read_at;
   for (i = 0; i < 3; i++)
      if ((uintptr_t)data >= (uintptr_t)p->video.bytes[i]
            && (uintptr_t)data < (uintptr_t)p->video.bytes[i] + sizeof(p->video.bytes[i]))
      {
         CHECK(p->video.reserved == i && (uintptr_t)data + bytes <= (uintptr_t)p->video.bytes[i] + sizeof(p->video.bytes[i]));
         w->direct += bytes; p->read_direct += bytes;
      }
   memcpy(data, w->bytes + w->read_at, bytes); w->read_at += bytes;
   return (int)bytes;
}

static int test_controller_write(void *user, const void *data, size_t bytes)
{
   test_controller_peer_t *p = (test_controller_peer_t*)user;
   test_controller_wire_t *w = p->output;
   unsigned i;
   if (w->blocked) return 0;
   if (w->write_at == w->read_at) w->write_at = w->read_at = 0;
   if (bytes > 31) bytes = 31;
   if (bytes > sizeof(w->bytes) - w->write_at) bytes = sizeof(w->bytes) - w->write_at;
   for (i = 0; i < 3; i++)
      if ((uintptr_t)data >= (uintptr_t)p->video.bytes[i]
            && (uintptr_t)data < (uintptr_t)p->video.bytes[i] + sizeof(p->video.bytes[i]))
      {
         CHECK((uintptr_t)data + bytes <= (uintptr_t)p->video.bytes[i] + p->video.sizes[i]);
         w->direct += bytes; p->write_direct += bytes;
      }
   memcpy(w->bytes + w->write_at, data, bytes); w->write_at += bytes;
   return (int)bytes;
}

static unsigned test_controller_state(record_pyrowave_session_t *session)
{
   record_pyrowave_session_status_t status;
   record_pyrowave_session_status(session, &status);
   return status.state;
}

static test_controller_t *test_controller_new(unsigned receiver_slots)
{
   test_controller_t *t = (test_controller_t*)calloc(1, sizeof(*t));
   rpyrowave_stream_io_t io;
   record_pyrowave_video_t binding;
   record_pyrowave_pcm_t *wrong;
   unsigned i;
   CHECK(t);
   t->config.sequence.width = t->config.sequence.height = 128;
   t->config.maximum_frame_bytes = 10000; t->config.bit_depth = 8;
   t->config.fps_numerator = 60000; t->config.fps_denominator = 1001;
   t->config.audio_rate = 48000; t->config.audio_channels = 2; t->config.audio_channel_mask = 3;
   t->config.maximum_audio_frames = 32;
   for (i = 0; i < 2; i++)
   {
      test_controller_peer_t *p = &t->peers[i];
      p->input = &t->wire[i ^ 1]; p->output = &t->wire[i];
      p->video.reserved = RECORD_GPU_QUEUE_SLOTS;
      p->video.queue = record_gpu_queue_new(i ? receiver_slots : 3);
      p->pcm = record_pyrowave_pcm_new(2, 32, 2);
      CHECK(p->video.queue && p->pcm);
      io.user = p; io.read = test_controller_read; io.write = test_controller_write;
      binding = test_controller_binding(&p->video);
      p->session = i ? record_pyrowave_session_receiver_new(&io)
         : record_pyrowave_session_sender_new(&t->config, ((uint64_t)1 << 32) + 0x23456789U, &io, &binding, p->pcm);
      CHECK(p->session);
   }
   for (i = 0; i < 1000 && test_controller_state(t->peers[1].session) != RECORD_PYROWAVE_CONFIGURING; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      CHECK(record_pyrowave_session_pump(t->peers[1].session) >= 0);
   }
   CHECK(i < 1000);
   wrong = record_pyrowave_pcm_new(1, 32, 1); CHECK(wrong);
   binding = test_controller_binding(&t->peers[1].video);
   CHECK(!record_pyrowave_session_accept(t->peers[1].session, &binding, wrong));
   CHECK(record_pyrowave_pcm_free(wrong));
   CHECK(record_pyrowave_session_accept(t->peers[1].session, &binding, t->peers[1].pcm));
   for (i = 0; i < 1000 && test_controller_state(t->peers[0].session) != RECORD_PYROWAVE_ACTIVE; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      CHECK(record_pyrowave_session_pump(t->peers[1].session) >= 0);
   }
   CHECK(i < 1000 && test_controller_state(t->peers[1].session) == RECORD_PYROWAVE_ACTIVE);
   return t;
}

static uint8_t test_controller_byte(unsigned index, unsigned serial)
{
   uint32_t dimensions = 127 | (127U << 14) | ((serial & 7) << 28) | 0x80000000U;
   if (index < 4) return (uint8_t)(dimensions >> (index * 8));
   if (index < 8) return 0;
   return (uint8_t)(index * 37 + serial);
}

static void test_controller_publish(test_controller_t *t, unsigned serial)
{
   test_controller_video_t *v = &t->peers[0].video;
   record_gpu_frame_t frame;
   unsigned i;
   memset(&frame, 0, sizeof(frame));
   CHECK(record_gpu_queue_acquire(v->queue, &frame.slot) == RECORD_GPU_QUEUE_OK);
   frame.ready_value = serial; frame.timestamp = (serial - 1) * (uint64_t)16683; frame.duration = 16683;
   v->sizes[frame.slot] = 9000;
   for (i = 0; i < 9000; i++) v->bytes[frame.slot][i] = test_controller_byte(i, serial);
   CHECK(record_gpu_queue_submit(v->queue, &frame));
}

static void test_controller_free(test_controller_t *t)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      record_gpu_frame_t frame;
      record_pyrowave_pcm_view_t audio;
      test_controller_peer_t *p = &t->peers[i];
      CHECK(record_pyrowave_session_free(p->session));
      while (record_gpu_queue_take(p->video.queue, &frame) == RECORD_GPU_QUEUE_OK)
         CHECK(record_gpu_queue_release(p->video.queue, frame.slot));
      while (record_pyrowave_pcm_take(p->pcm, &audio) == RECORD_GPU_QUEUE_OK)
         CHECK(record_pyrowave_pcm_release(p->pcm, audio.slot));
      CHECK(record_gpu_queue_free(p->video.queue) && record_pyrowave_pcm_free(p->pcm));
   }
   free(t);
}

static void test_controller_roundtrip(unsigned slots)
{
   test_controller_t *t = test_controller_new(slots);
   record_pyrowave_session_status_t status;
   record_gpu_frame_t frame;
   record_pyrowave_pcm_view_t audio;
   void *data;
   size_t capacity;
   unsigned i, received = 0;
   test_controller_publish(t, 1); test_controller_publish(t, 2);
   CHECK(record_pyrowave_pcm_acquire(t->peers[0].pcm, &data, &capacity) == RECORD_GPU_QUEUE_OK && capacity == 128);
   memset(data, 0x5a, 128); CHECK(record_pyrowave_pcm_submit(t->peers[0].pcm, 0, 32));
   CHECK(record_gpu_queue_finish(t->peers[0].video.queue) && record_pyrowave_pcm_finish(t->peers[0].pcm));
   CHECK(record_pyrowave_session_finish(t->peers[0].session));
   for (i = 0; i < 100; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      CHECK(record_pyrowave_session_pump(t->peers[1].session) >= 0);
   }
   record_pyrowave_session_status(t->peers[1].session, &status);
   CHECK(status.audio_received == 1 && !status.video_received && !t->peers[0].video.released);
   CHECK(record_pyrowave_pcm_take(t->peers[1].pcm, &audio) == RECORD_GPU_QUEUE_OK && audio.frames == 32 && !audio.first_sample);
   for (i = 0; i < 128; i++) CHECK(((const uint8_t*)audio.data)[i] == 0x5a);
   CHECK(record_pyrowave_pcm_release(t->peers[1].pcm, audio.slot));
   t->peers[0].video.ready = true;
   t->wire[0].blocked = true;
   CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0 && !t->peers[0].video.released);
   t->wire[0].blocked = false;
   for (i = 0; i < 10000 && test_controller_state(t->peers[1].session) != RECORD_PYROWAVE_ENDED; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      CHECK(record_pyrowave_session_pump(t->peers[1].session) >= 0);
   }
   CHECK(i < 10000 && test_controller_state(t->peers[0].session) == RECORD_PYROWAVE_ENDED);
   while (record_gpu_queue_take(t->peers[1].video.queue, &frame) == RECORD_GPU_QUEUE_OK)
   {
      unsigned byte;
      received++;
      CHECK(t->peers[1].video.sizes[frame.slot] == 9000);
      for (byte = 0; byte < 9000; byte++) CHECK(t->peers[1].video.bytes[frame.slot][byte] == test_controller_byte(byte, received));
      CHECK(record_gpu_queue_release(t->peers[1].video.queue, frame.slot));
   }
   record_pyrowave_session_status(t->peers[1].session, &status);
   CHECK(received == slots && status.video_received == slots && status.video_dropped == 2 - slots);
   CHECK(status.generation == ((uint64_t)1 << 32) + 0x23456789U && t->peers[0].video.released == 2);
   CHECK(t->peers[0].write_direct == 18000 && t->peers[1].read_direct == slots * 9000);
   test_controller_free(t);
}

static void test_controller_abort_pending(bool abandon)
{
   test_controller_t *t = test_controller_new(1);
   record_gpu_frame_t frame;
   test_controller_publish(t, 1);
   CHECK(!record_pyrowave_session_abandon_video(t->peers[0].session));
   CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
   record_pyrowave_session_abort(t->peers[0].session);
   CHECK(!record_pyrowave_session_free(t->peers[0].session) && !t->peers[0].video.released);
   if (abandon)
   {
      CHECK(record_pyrowave_session_abandon_video(t->peers[0].session));
      /* The cold backend abandonment retires the forgotten taken slot. */
      frame.slot = 0;
      CHECK(record_gpu_queue_release(t->peers[0].video.queue, frame.slot));
   }
   else
   {
      t->peers[0].video.ready = true;
      CHECK(record_pyrowave_session_pump(t->peers[0].session) == RPYROWAVE_STREAM_FINISHED);
      CHECK(t->peers[0].video.released == 1);
   }
   test_controller_free(t);
}

static void test_controller_abort_partial(bool fail_commit)
{
   test_controller_t *t = test_controller_new(1);
   unsigned i;
   test_controller_publish(t, 1); t->peers[0].video.ready = true;
   t->peers[1].video.fail_commit = fail_commit;
   for (i = 0; i < 10000; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      if (record_pyrowave_session_pump(t->peers[1].session) < 0) break;
      if (!fail_commit && t->peers[1].video.reserved != RECORD_GPU_QUEUE_SLOTS) break;
   }
   CHECK(i < 10000);
   if (fail_commit) CHECK(test_controller_state(t->peers[1].session) == RECORD_PYROWAVE_FAILED);
   record_pyrowave_session_abort(t->peers[0].session);
   record_pyrowave_session_abort(t->peers[1].session);
   record_pyrowave_session_abort(t->peers[0].session);
   record_pyrowave_session_abort(t->peers[1].session);
   CHECK(t->peers[0].video.released == 1 && t->peers[1].video.cancelled == 1 && !t->peers[1].video.committed);
   test_controller_free(t);
}

static void test_controller_bad_span(bool cancel_failure)
{
   test_controller_t *t = test_controller_new(1);
   unsigned i;
   test_controller_publish(t, 1); t->peers[0].video.ready = true;
   t->peers[1].video.short_span = 1; t->peers[1].video.fail_cancel = cancel_failure;
   for (i = 0; i < 1000; i++)
   {
      CHECK(record_pyrowave_session_pump(t->peers[0].session) >= 0);
      if (record_pyrowave_session_pump(t->peers[1].session) < 0) break;
   }
   CHECK(i < 1000 && test_controller_state(t->peers[1].session) == RECORD_PYROWAVE_FAILED);
   if (cancel_failure)
   {
      CHECK(!record_pyrowave_session_free(t->peers[1].session));
      CHECK(record_pyrowave_session_abandon_video(t->peers[1].session));
      t->peers[1].video.fail_cancel = 0;
      CHECK(test_controller_cancel(&t->peers[1].video));
   }
   CHECK(t->peers[1].video.cancelled == 1);
   test_controller_free(t);
}

static void test_controller_clock(void)
{
   record_pyrowave_clock_t *c = record_pyrowave_clock_new(2);
   record_pyrowave_clock_position_t p;
   unsigned offset;
   CHECK(c && !record_pyrowave_clock_room(c));
   CHECK(record_pyrowave_clock_reset(c, 7, 1));
   CHECK(!record_pyrowave_clock_drained(c, 0));
   CHECK(record_pyrowave_clock_commit(c, 100, 10, 50, 10, 10, 0));
   CHECK(record_pyrowave_clock_commit(c, 120, 10, 70, 10, 10, 0));
   CHECK(!record_pyrowave_clock_room(c));
   CHECK(!record_pyrowave_clock_poll(c, 99, &p) && p.generation == 7 && p.epoch == 1);
   CHECK(record_pyrowave_clock_poll(c, 105, &p) && p.source_sample == 55);
   CHECK(record_pyrowave_clock_poll(c, 110, &p) && p.source_sample == 60 && record_pyrowave_clock_room(c));
   CHECK(!record_pyrowave_clock_poll(c, 115, &p));
   CHECK(record_pyrowave_clock_poll(c, 120, &p) && p.source_sample == 70);
   CHECK(!record_pyrowave_clock_drained(c, 80));
   CHECK(record_pyrowave_clock_poll(c, 130, &p) && p.source_sample == 80);
   CHECK(record_pyrowave_clock_poll(c, 130, &p) && p.source_sample == 80);
   CHECK(!record_pyrowave_clock_poll(c, 131, &p));
   CHECK(!p.valid && record_pyrowave_clock_drained(c, 80));
   CHECK(!record_pyrowave_clock_drained(c, 81));
   CHECK(!record_pyrowave_clock_poll(c, 130, &p) && !record_pyrowave_clock_room(c));
   CHECK(!record_pyrowave_clock_drained(c, 80));
   CHECK(!record_pyrowave_clock_reset(c, 7, 1));
   CHECK(record_pyrowave_clock_reset(c, 7, 2));
   CHECK(!record_pyrowave_clock_drained(c, 80));
   for (offset = 0; offset < 480; offset += 7)
   {
      unsigned frames = 480 - offset < 7 ? 480 - offset : 7;
      CHECK(record_pyrowave_clock_commit(c, 1000 + offset, frames, 0, 441, 480, offset));
      CHECK(record_pyrowave_clock_poll(c, 1000 + offset, &p) && p.source_sample == (uint64_t)offset * 441 / 480);
      CHECK(!record_pyrowave_clock_drained(c, 441));
      CHECK(record_pyrowave_clock_poll(c, 1000 + offset + frames, &p)
            && p.source_sample == (uint64_t)(offset + frames) * 441 / 480);
   }
   CHECK(p.source_sample == 441);
   CHECK(record_pyrowave_clock_drained(c, 441));
   CHECK(!record_pyrowave_clock_poll(c, 1500, &p) && !p.valid);
   CHECK(record_pyrowave_clock_drained(c, 441));
   CHECK(record_pyrowave_clock_commit(c, 1600, 4, 441, 4, 4, 0));
   CHECK(!record_pyrowave_clock_drained(c, 441));
   CHECK(!record_pyrowave_clock_drained(c, 445));
   CHECK(!record_pyrowave_clock_commit(c, 1500, 2, 441, 10, 10, 9));
   CHECK(!record_pyrowave_clock_commit(c, (uint64_t)-1, 1, 441, 1, 1, 0));
   CHECK(record_pyrowave_clock_reset(c, 7, 3));
   CHECK(record_pyrowave_clock_commit(c, 0, 0xffffffffU, 0, 0xffffffffU, 0xffffffffU, 0));
   CHECK(record_pyrowave_clock_poll(c, 0xfffffffeU, &p) && p.source_sample == 0xfffffffeU);
   record_pyrowave_clock_free(c);
}

#include "pyrowave_connection_test.h"
#include "pyrowave_audio_clock_test.h"
#ifdef RPY_TEST_DISPATCH
#include "pyrowave_dispatch_test.h"
#endif

int main(void)
{
   test_controller_clock();
   test_controller_audio_clock();
   test_controller_roundtrip(2); test_controller_roundtrip(1);
   test_controller_abort_pending(false); test_controller_abort_pending(true);
   test_controller_abort_partial(false); test_controller_abort_partial(true);
   test_controller_bad_span(false); test_controller_bad_span(true);
   test_controller_tcp(0, false); test_controller_tcp(1, false); test_controller_tcp(0, true);
#ifdef RPY_TEST_DISPATCH
   test_controller_dispatch(0, false); test_controller_dispatch(1, false); test_controller_dispatch(2, false);
   test_controller_dispatch(0, true); test_controller_dispatch(1, true);
   test_controller_dispatch_receiver(false, false); test_controller_dispatch_receiver(true, false);
   test_controller_dispatch_receiver(false, true); test_controller_dispatch_receiver(true, true);
#endif
   puts("PyroWave session controller: fragmented direct loans, audio priority, drops, EOS, GPU-pending cancellation and failed commit PASS");
   return EXIT_SUCCESS;
}
