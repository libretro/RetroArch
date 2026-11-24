/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_pcm.h"
#include <formats/rpyrowave_stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "PCM session failed line %u: %s\n", \
   (unsigned)__LINE__, #test); exit(EXIT_FAILURE); } } while (0)

typedef struct pyrowave_pcm_wire
{
   size_t read_at, write_at;
   unsigned char bytes[4096];
} pyrowave_pcm_wire_t;

typedef struct pyrowave_pcm_peer
{
   rpyrowave_stream_t *stream;
   record_pyrowave_pcm_t *pcm;
   pyrowave_pcm_wire_t *input, *output;
   record_pyrowave_pcm_view_t loan;
   void *incoming;
   size_t direct_bytes;
   unsigned configured, started, ended, completed, dropped, cancelled;
} pyrowave_pcm_peer_t;

static int pyrowave_pcm_read(void *user, void *data, size_t bytes)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   pyrowave_pcm_wire_t *w = p->input;
   if (bytes > 7) bytes = 7;
   if (bytes > w->write_at - w->read_at) bytes = w->write_at - w->read_at;
   if (p->incoming)
   {
      CHECK((uintptr_t)data >= (uintptr_t)p->incoming
            && (uintptr_t)data + bytes <= (uintptr_t)p->incoming + 128);
      p->direct_bytes += bytes;
   }
   memcpy(data, w->bytes + w->read_at, bytes);
   w->read_at += bytes;
   return (int)bytes;
}

static int pyrowave_pcm_write(void *user, const void *data, size_t bytes)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   pyrowave_pcm_wire_t *w = p->output;
   if (w->write_at == w->read_at) w->write_at = w->read_at = 0;
   if (bytes > 7) bytes = 7;
   if (bytes > sizeof(w->bytes) - w->write_at) bytes = sizeof(w->bytes) - w->write_at;
   memcpy(w->bytes + w->write_at, data, bytes);
   w->write_at += bytes;
   return (int)bytes;
}

static int pyrowave_pcm_acquire(void *user, const rpyrowave_stream_record_t *record,
      void **data, size_t *capacity)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   int result;
   CHECK(record->kind == RPYROWAVE_STREAM_AUDIO && record->bytes == 128 && !p->incoming);
   result = record_pyrowave_pcm_acquire(p->pcm, data, capacity);
   if (result == RECORD_GPU_QUEUE_BUSY) return 0;
   CHECK(result == RECORD_GPU_QUEUE_OK);
   p->incoming = *data;
   return 1;
}

static int pyrowave_pcm_commit(void *user, const rpyrowave_stream_record_t *record, void *data)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   CHECK(data == p->incoming && record->bytes == 128);
   CHECK(record_pyrowave_pcm_submit(p->pcm, record->serial, record->bytes / 4));
   p->incoming = NULL;
   p->completed++;
   return 1;
}

static void pyrowave_pcm_cancel(void *user, const rpyrowave_stream_record_t *record, void *data)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   (void)record;
   CHECK(data == p->incoming && record_pyrowave_pcm_cancel(p->pcm));
   p->incoming = NULL;
   p->cancelled++;
}

static void pyrowave_pcm_dropped(void *user, const rpyrowave_stream_record_t *record)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   CHECK(record->kind == RPYROWAVE_STREAM_AUDIO);
   p->dropped++;
}

static void pyrowave_pcm_sent(void *user, const rpyrowave_stream_record_t *record,
      const void *data, void *token, int sent)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   CHECK(data == p->loan.data && token == &p->loan && record->serial == p->loan.first_sample);
   CHECK(record_pyrowave_pcm_release(p->pcm, p->loan.slot));
   p->loan.data = NULL;
   if (sent) p->completed++;
   else p->cancelled++;
}

static void pyrowave_pcm_event(void *user, unsigned event,
      const rpyrowave_stream_config_t *config, uint64_t generation, unsigned reason)
{
   pyrowave_pcm_peer_t *p = (pyrowave_pcm_peer_t*)user;
   CHECK(generation == 7 && !reason);
   if (event == RPYROWAVE_STREAM_CONFIGURE)
   {
      CHECK(config->audio_channels == 2 && config->audio_rate == 48000 && config->maximum_audio_frames == 32);
      p->configured = 1;
   }
   else if (event == RPYROWAVE_STREAM_STARTED) p->started = 1;
   else if (event == RPYROWAVE_STREAM_ENDED) p->ended = 1;
   else CHECK(event == RPYROWAVE_STREAM_CANCELLED);
}

static void pyrowave_pcm_pump(pyrowave_pcm_peer_t *peers)
{
   CHECK(rpyrowave_stream_pump(peers[0].stream) >= 0);
   CHECK(rpyrowave_stream_pump(peers[1].stream) >= 0);
   if (peers[1].configured == 1)
   {
      CHECK(rpyrowave_stream_accept(peers[1].stream, 0) == RPYROWAVE_STREAM_PROGRESS);
      peers[1].configured = 2;
   }
}

static void pyrowave_pcm_check(const record_pyrowave_pcm_view_t *view, unsigned packet)
{
   const unsigned char *bytes = (const unsigned char*)view->data;
   unsigned i;
   CHECK(view->first_sample == packet * 32 && view->frames == 32);
   for (i = 0; i < 128; i++) CHECK(bytes[i] == (unsigned char)(packet * 17 + i));
}

static void pyrowave_pcm_offer(pyrowave_pcm_peer_t *p, unsigned packet)
{
   void *span;
   size_t capacity;
   unsigned i;
   rpyrowave_stream_record_t record;
   uint64_t end;
   CHECK(record_pyrowave_pcm_acquire(p->pcm, &span, &capacity) == RECORD_GPU_QUEUE_OK && capacity == 128);
   for (i = 0; i < 128; i++) ((unsigned char*)span)[i] = (unsigned char)(packet * 17 + i);
   CHECK(record_pyrowave_pcm_submit(p->pcm, packet * 32, 32));
   CHECK(record_pyrowave_pcm_take(p->pcm, &p->loan) == RECORD_GPU_QUEUE_OK && p->loan.data == span);
   memset(&record, 0, sizeof(record));
   record.generation = 7;
   record.kind = RPYROWAVE_STREAM_AUDIO;
   record.serial = packet * 32;
   record.bytes = 128;
   CHECK(!rpyrowave_stream_time(record.serial, 48000, 1, &record.timestamp));
   CHECK(!rpyrowave_stream_time(record.serial + 32, 48000, 1, &end));
   record.duration = end - record.timestamp;
   CHECK(rpyrowave_stream_offer(p->stream, &record, p->loan.data, &p->loan) == RPYROWAVE_STREAM_PROGRESS);
   CHECK(record_pyrowave_pcm_acquire(p->pcm, &span, &capacity) == RECORD_GPU_QUEUE_BUSY && !span && !capacity);
}

static void pyrowave_pcm_bounds(void)
{
   record_pyrowave_pcm_t *p = record_pyrowave_pcm_new(2, 32, 1);
   record_pyrowave_pcm_view_t view;
   record_pyrowave_pcm_view_t empty;
   void *span;
   void *again;
   size_t capacity;
   uint64_t origin = (uint64_t)1 << 60;
   CHECK(p);
   CHECK(record_pyrowave_pcm_acquire(p, &span, &capacity) == RECORD_GPU_QUEUE_OK && capacity == 128);
   memset(span, 0xa5, capacity);
   CHECK(record_pyrowave_pcm_acquire(p, &again, &capacity) == RECORD_GPU_QUEUE_ERROR && !again && !capacity);
   CHECK(!record_pyrowave_pcm_submit(p, 0, 0) && !record_pyrowave_pcm_submit(p, 0, 33));
   CHECK(!record_pyrowave_pcm_submit(p, (uint64_t)-1 - 30, 32));
   CHECK(!record_pyrowave_pcm_finish(p) && !record_pyrowave_pcm_free(p));
   CHECK(record_pyrowave_pcm_cancel(p) && !record_pyrowave_pcm_cancel(p));
   CHECK(record_pyrowave_pcm_acquire(p, &again, &capacity) == RECORD_GPU_QUEUE_OK && again == span);
   CHECK(record_pyrowave_pcm_submit(p, origin, 32));
   CHECK(record_pyrowave_pcm_take(p, &view) == RECORD_GPU_QUEUE_OK
         && view.first_sample == origin && view.frames == 32 && view.data == span);
   CHECK(!record_pyrowave_pcm_free(p));
   CHECK(record_pyrowave_pcm_release(p, view.slot));
   CHECK(record_pyrowave_pcm_acquire(p, &again, &capacity) == RECORD_GPU_QUEUE_OK);
   CHECK(!record_pyrowave_pcm_submit(p, origin + 31, 32));
   CHECK(record_pyrowave_pcm_submit(p, (uint64_t)-1 - 32, 32));
   CHECK(record_pyrowave_pcm_finish(p));
   CHECK(record_pyrowave_pcm_take(p, &view) == RECORD_GPU_QUEUE_OK && view.first_sample == (uint64_t)-1 - 32);
   CHECK(record_pyrowave_pcm_take(p, &empty) == RECORD_GPU_QUEUE_END && !empty.data);
   CHECK(((const unsigned char*)view.data)[127] == 0xa5 && !record_pyrowave_pcm_free(p));
   CHECK(record_pyrowave_pcm_acquire(p, &again, &capacity) == RECORD_GPU_QUEUE_END);
   CHECK(record_pyrowave_pcm_release(p, view.slot) && record_pyrowave_pcm_free(p));
}

static void pyrowave_pcm_setup(pyrowave_pcm_peer_t *peers, pyrowave_pcm_wire_t *wires,
      const rpyrowave_stream_config_t *config)
{
   unsigned i;
   memset(peers, 0, 2 * sizeof(*peers));
   memset(wires, 0, 2 * sizeof(*wires));
   for (i = 0; i < 2; i++)
   {
      rpyrowave_stream_io_t io;
      rpyrowave_stream_callbacks_t callbacks;
      peers[i].pcm = record_pyrowave_pcm_new(2, 32, i ? 2 : 1);
      CHECK(peers[i].pcm);
      peers[i].input = &wires[i ^ 1];
      peers[i].output = &wires[i];
      io.user = &peers[i]; io.read = pyrowave_pcm_read; io.write = pyrowave_pcm_write;
      memset(&callbacks, 0, sizeof(callbacks));
      callbacks.user = &peers[i]; callbacks.acquire = pyrowave_pcm_acquire;
      callbacks.commit = pyrowave_pcm_commit; callbacks.cancel = pyrowave_pcm_cancel;
      callbacks.sent = pyrowave_pcm_sent; callbacks.event = pyrowave_pcm_event;
      callbacks.dropped = pyrowave_pcm_dropped;
      peers[i].stream = i ? rpyrowave_stream_receiver_new(&io, &callbacks)
         : rpyrowave_stream_sender_new(config, 7, &io, &callbacks);
      CHECK(peers[i].stream);
   }
   for (i = 0; i < 1000 && (!peers[0].started || !peers[1].started); i++) pyrowave_pcm_pump(peers);
   CHECK(i < 1000);
}

static void pyrowave_pcm_cleanup(pyrowave_pcm_peer_t *peers)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      rpyrowave_stream_free(peers[i].stream);
      CHECK(record_pyrowave_pcm_free(peers[i].pcm));
   }
}

int main(void)
{
   pyrowave_pcm_peer_t *peers = (pyrowave_pcm_peer_t*)calloc(2, sizeof(*peers));
   pyrowave_pcm_wire_t *wires = (pyrowave_pcm_wire_t*)calloc(2, sizeof(*wires));
   rpyrowave_stream_config_t config;
   record_pyrowave_pcm_view_t held[2];
   unsigned i, packet;
   pyrowave_pcm_bounds();
   CHECK(peers && wires);
   CHECK(!record_pyrowave_pcm_new(0, 32, 2) && !record_pyrowave_pcm_new(8, 4096, 2));
   memset(&config, 0, sizeof(config));
   config.sequence.width = config.sequence.height = 2;
   config.maximum_frame_bytes = 65536;
   config.bit_depth = 8;
   config.fps_numerator = 60;
   config.fps_denominator = 1;
   config.audio_rate = 48000;
   config.audio_channels = 2;
   config.audio_channel_mask = 3;
   config.maximum_audio_frames = 32;
   pyrowave_pcm_setup(peers, wires, &config);
   for (packet = 0; packet < 24; packet++)
   {
      pyrowave_pcm_offer(&peers[0], packet);
      for (i = 0; i < 1000; i++)
      {
         pyrowave_pcm_pump(peers);
         if (peers[0].completed == packet + 1 && peers[1].completed + peers[1].dropped == packet + 1) break;
      }
      CHECK(i < 1000);
      if (packet == 2)
      {
         CHECK(peers[1].dropped == 1);
         CHECK(record_pyrowave_pcm_release(peers[1].pcm, held[1].slot));
      }
      else
      {
         record_pyrowave_pcm_view_t view;
         CHECK(record_pyrowave_pcm_take(peers[1].pcm, &view) == RECORD_GPU_QUEUE_OK);
         pyrowave_pcm_check(&view, packet);
         if (packet < 2) held[packet] = view;
         else CHECK(record_pyrowave_pcm_release(peers[1].pcm, view.slot));
      }
      pyrowave_pcm_check(&held[0], 0);
   }
   CHECK(peers[1].direct_bytes == 23 * 128);
   CHECK(rpyrowave_stream_finish(peers[0].stream) == RPYROWAVE_STREAM_PROGRESS);
   for (i = 0; i < 1000 && !peers[1].ended; i++) pyrowave_pcm_pump(peers);
   CHECK(i < 1000 && record_pyrowave_pcm_finish(peers[1].pcm));
   CHECK(!record_pyrowave_pcm_free(peers[1].pcm));
   CHECK(record_pyrowave_pcm_release(peers[1].pcm, held[0].slot));
   CHECK(!record_pyrowave_pcm_release(peers[1].pcm, held[0].slot));
   pyrowave_pcm_cleanup(peers);
   pyrowave_pcm_setup(peers, wires, &config);
   pyrowave_pcm_offer(&peers[0], 0);
   for (i = 0; i < 1000 && !peers[1].direct_bytes; i++) pyrowave_pcm_pump(peers);
   CHECK(i < 1000 && peers[1].incoming && !peers[1].completed);
   CHECK(!record_pyrowave_pcm_free(peers[1].pcm));
   for (i = 0; i < 2; i++)
   {
      rpyrowave_stream_abort(peers[i].stream);
      rpyrowave_stream_abort(peers[i].stream);
      CHECK(peers[i].cancelled == 1 && !peers[i].completed && !peers[i].incoming);
   }
   pyrowave_pcm_cleanup(peers);
   free(wires); free(peers);
   puts("PyroWave direct PCM stream lends, backpressure and retirement PASS");
   return EXIT_SUCCESS;
}
