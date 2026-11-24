/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_DISPATCH_TEST_H
#define PYROWAVE_DISPATCH_TEST_H
#include "../../../record/record_pyrowave_dispatch.h"
#include <retro_atomic.h>
#if !defined(_WIN32)
#include <poll.h>
#endif

static retro_atomic_int_t test_dispatch_gpu_ready;

static int test_dispatch_packet(void *user, unsigned slot, record_pyrowave_packet_t *packet)
{
   if (!retro_atomic_load_acquire_int(&test_dispatch_gpu_ready)) return RECORD_GPU_QUEUE_BUSY;
   return test_controller_packet(user, slot, packet);
}

static void test_dispatch_collect_wait(void)
{
   /* Offline watchdog/collection only; no playback is paced by this delay. */
#if defined(_WIN32)
   Sleep(1);
#else
   poll(NULL, 0, 1);
#endif
}

static void test_controller_dispatch(unsigned cancellation, bool listen_worker)
{
   test_controller_t *t = (test_controller_t*)calloc(1, sizeof(*t));
   record_pyrowave_connection_t *connections[2];
   record_pyrowave_dispatch_t *job;
   record_pyrowave_video_t source, receiver;
   rpyrowave_stream_config_t cfg;
   struct sockaddr_in address;
   record_gpu_frame_t frame;
   record_pyrowave_pcm_view_t audio;
   uint64_t generation;
   unsigned i, received = 0, expected = cancellation ? RECORD_PYROWAVE_DISPATCH_CANCELLED : RECORD_PYROWAVE_DISPATCH_DONE;
#if defined(_WIN32)
   int address_bytes = sizeof(address);
   WSADATA startup;
   CHECK(!WSAStartup(MAKEWORD(2, 2), &startup));
#else
   socklen_t address_bytes = sizeof(address);
#endif
   CHECK(t);
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   t->config.sequence.width = t->config.sequence.height = 128;
   t->config.maximum_frame_bytes = 10000; t->config.bit_depth = 8;
   t->config.fps_numerator = 60; t->config.fps_denominator = 1;
   for (i = 0; i < 2; i++)
   {
      t->peers[i].video.queue = record_gpu_queue_new(3);
      t->peers[i].video.reserved = RECORD_GPU_QUEUE_SLOTS;
      CHECK(t->peers[i].video.queue);
   }
   if (!cancellation)
   {
      void *samples;
      size_t capacity;
      t->config.audio_rate = 48000; t->config.audio_channels = 2;
      t->config.audio_channel_mask = 3; t->config.maximum_audio_frames = 32;
      t->peers[0].pcm = record_pyrowave_pcm_new(2, 32, 2);
      t->peers[1].pcm = record_pyrowave_pcm_new(2, 32, 2);
      CHECK(t->peers[0].pcm && t->peers[1].pcm);
      CHECK(record_pyrowave_pcm_acquire(t->peers[0].pcm, &samples, &capacity) == RECORD_GPU_QUEUE_OK && capacity >= 128);
      memset(samples, 0x5c, 128);
      CHECK(record_pyrowave_pcm_submit(t->peers[0].pcm, 0, 32) && record_pyrowave_pcm_finish(t->peers[0].pcm));
   }
   connections[0] = connections[1] = NULL;
   if (!listen_worker)
   {
      connections[0] = record_pyrowave_connection_new(&address, sizeof(address), true, NULL, 0, NULL, NULL);
      CHECK(connections[0]);
      CHECK(!getsockname((test_connection_socket_t)record_pyrowave_connection_handle(connections[0]),
               (struct sockaddr*)&address, &address_bytes));
   }
   t->peers[0].video.ready = true;
   retro_atomic_int_init(&test_dispatch_gpu_ready, cancellation ? 0 : 1);
   test_controller_publish(t, 1);
   if (!cancellation) test_controller_publish(t, 2);
   CHECK(record_gpu_queue_finish(t->peers[0].video.queue));
   source = test_controller_binding(&t->peers[0].video); source.packet = test_dispatch_packet;
   job = record_pyrowave_dispatch_new(&address, sizeof(address), listen_worker, &t->config, 99, &source, t->peers[0].pcm);
   CHECK(job && record_pyrowave_dispatch_config(job, &cfg, &generation) && generation == 99);
   if (listen_worker)
   {
      size_t capacity = 0;
      CHECK(!record_pyrowave_dispatch_bound_address(job, NULL, &capacity) && capacity && capacity <= sizeof(address));
      capacity = sizeof(address);
      CHECK(record_pyrowave_dispatch_bound_address(job, &address, &capacity) && address.sin_port);
      connections[0] = record_pyrowave_connection_new(&address, sizeof(address), false, NULL, 0, NULL, NULL);
      CHECK(connections[0]);
   }
   t->peers[1].session = record_pyrowave_connection_session(connections[0]);
   CHECK(record_pyrowave_dispatch_finish(job));
   for (i = 0; i < 256 && test_controller_state(t->peers[1].session) != RECORD_PYROWAVE_CONFIGURING; i++)
      test_connection_step(connections);
   CHECK(i < 256);
   receiver = test_controller_binding(&t->peers[1].video);
   CHECK(record_pyrowave_session_accept(t->peers[1].session, &receiver, t->peers[1].pcm));
   for (i = 0; i < 256 && record_pyrowave_dispatch_state(job, NULL) < RECORD_PYROWAVE_DISPATCH_READY; i++)
   {
      if (record_pyrowave_connection_interest(connections[0])) test_connection_step(connections);
      else test_dispatch_collect_wait();
   }
   CHECK(i < 256);
   if (cancellation)
   {
      record_pyrowave_dispatch_cancel(job);
      CHECK(!record_pyrowave_dispatch_free(job));
      for (i = 0; i < 1000 && record_pyrowave_dispatch_state(job, NULL) != RECORD_PYROWAVE_DISPATCH_DRAINING; i++)
         test_dispatch_collect_wait();
      CHECK(i < 1000 && !record_pyrowave_dispatch_free(job));
      if (cancellation == 2) record_pyrowave_dispatch_abandon_video(job);
      else
      {
         retro_atomic_store_release_int(&test_dispatch_gpu_ready, 1);
         record_pyrowave_dispatch_notify(job);
      }
   }
   for (i = 0; i < 1000; i++)
   {
      unsigned state = record_pyrowave_dispatch_state(job, NULL);
      CHECK(state != RECORD_PYROWAVE_DISPATCH_FAILED);
      if (state == expected && (cancellation || test_controller_state(t->peers[1].session) == RECORD_PYROWAVE_ENDED)) break;
      if (!cancellation && record_pyrowave_connection_interest(connections[0])) test_connection_step(connections);
      else test_dispatch_collect_wait();
   }
   CHECK(i < 1000);
   for (i = 0; i < 1000; i++)
   {
      if (record_pyrowave_dispatch_free(job)) break;
      test_dispatch_collect_wait();
   }
   CHECK(i < 1000);
   if (cancellation == 2) CHECK(record_gpu_queue_release(t->peers[0].video.queue, 0));
   else CHECK(t->peers[0].video.released == (cancellation ? 1U : 2U));
   while (record_gpu_queue_take(t->peers[1].video.queue, &frame) == RECORD_GPU_QUEUE_OK)
   {
      unsigned byte;
      received++;
      for (byte = 0; byte < 9000; byte++) CHECK(t->peers[1].video.bytes[frame.slot][byte] == test_controller_byte(byte, received));
      CHECK(record_gpu_queue_release(t->peers[1].video.queue, frame.slot));
   }
   CHECK(received == (cancellation ? 0U : 2U));
   if (!cancellation)
   {
      CHECK(record_pyrowave_pcm_take(t->peers[1].pcm, &audio) == RECORD_GPU_QUEUE_OK && !audio.first_sample && audio.frames == 32);
      for (i = 0; i < 128; i++) CHECK(((const uint8_t*)audio.data)[i] == 0x5c);
      CHECK(record_pyrowave_pcm_release(t->peers[1].pcm, audio.slot));
   }
   CHECK(record_pyrowave_connection_free(connections[0])); t->peers[1].session = NULL;
   test_controller_free(t);
#if defined(_WIN32)
   CHECK(!WSACleanup());
#endif
   printf("PyroWave threaded dispatcher mode%u listen%u: TCP readiness, producer wake and retired loans PASS\n", cancellation, listen_worker);
}

static void test_controller_dispatch_receiver(bool reject, bool listen_worker)
{
   test_controller_t *t = (test_controller_t*)calloc(1, sizeof(*t));
   record_pyrowave_connection_t *connections[2];
   record_pyrowave_dispatch_t *job;
   record_pyrowave_video_t binding;
   rpyrowave_stream_config_t config;
   struct sockaddr_in address;
   record_gpu_frame_t frame;
   uint64_t generation;
   unsigned i, received = 0, details;
#if defined(_WIN32)
   int bytes = sizeof(address);
   WSADATA startup;
   CHECK(!WSAStartup(MAKEWORD(2, 2), &startup));
#else
   socklen_t bytes = sizeof(address);
#endif
   CHECK(t);
   memset(&address, 0, sizeof(address)); address.sin_family = AF_INET;
   address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   t->config.sequence.width = t->config.sequence.height = 128;
   t->config.maximum_frame_bytes = 10000; t->config.bit_depth = 8;
   t->config.fps_numerator = 60; t->config.fps_denominator = 1;
   for (i = 0; i < 2; i++)
   {
      t->peers[i].video.queue = record_gpu_queue_new(3);
      t->peers[i].video.reserved = RECORD_GPU_QUEUE_SLOTS;
      CHECK(t->peers[i].video.queue);
   }
   t->peers[0].video.ready = true;
   binding = test_controller_binding(&t->peers[0].video);
   connections[0] = connections[1] = NULL;
   if (!listen_worker)
   {
      connections[0] = record_pyrowave_connection_new(&address, sizeof(address), true, &t->config, 123, &binding, NULL);
      CHECK(connections[0]);
      CHECK(!getsockname((test_connection_socket_t)record_pyrowave_connection_handle(connections[0]),
               (struct sockaddr*)&address, &bytes));
   }
   job = record_pyrowave_dispatch_new(&address, sizeof(address), listen_worker, NULL, 0, NULL, NULL);
   CHECK(job && !record_pyrowave_dispatch_config(job, &config, &generation));
   if (listen_worker)
   {
      size_t capacity = sizeof(address);
      CHECK(record_pyrowave_dispatch_bound_address(job, &address, &capacity) && address.sin_port);
      connections[0] = record_pyrowave_connection_new(&address, sizeof(address), false, &t->config, 123, &binding, NULL);
      CHECK(connections[0]);
   }
   t->peers[0].session = record_pyrowave_connection_session(connections[0]);
   for (i = 0; i < 256 && record_pyrowave_dispatch_state(job, NULL) != RECORD_PYROWAVE_DISPATCH_CONFIGURING; i++)
      test_connection_step(connections);
   CHECK(i < 256 && record_pyrowave_dispatch_config(job, &config, &generation));
   CHECK(generation == 123 && config.sequence.width == 128 && config.maximum_frame_bytes == 10000);
   if (reject) CHECK(record_pyrowave_dispatch_reject(job, 17));
   else
   {
      binding = test_controller_binding(&t->peers[1].video);
      CHECK(record_pyrowave_dispatch_accept(job, &binding, NULL));
      CHECK(!record_pyrowave_dispatch_reject(job, 17));
      for (i = 0; i < 256 && test_controller_state(t->peers[0].session) != RECORD_PYROWAVE_ACTIVE; i++)
         test_connection_step(connections);
      CHECK(i < 256);
      test_controller_publish(t, 1); test_controller_publish(t, 2);
      CHECK(record_gpu_queue_finish(t->peers[0].video.queue));
      CHECK(record_pyrowave_session_finish(t->peers[0].session));
      CHECK(record_pyrowave_connection_step(connections[0], 0) >= 0);
   }
   for (i = 0; i < 1000; i++)
   {
      unsigned state = record_pyrowave_dispatch_state(job, &details);
      if (state == (unsigned)(reject ? RECORD_PYROWAVE_DISPATCH_FAILED : RECORD_PYROWAVE_DISPATCH_DONE)) break;
      if (record_pyrowave_connection_interest(connections[0])) test_connection_step(connections);
      else test_dispatch_collect_wait();
   }
   CHECK(i < 1000 && details == (reject ? 17U : 0U));
   for (i = 0; i < 1000; i++)
   {
      if (record_pyrowave_dispatch_free(job)) break;
      test_dispatch_collect_wait();
   }
   CHECK(i < 1000);
   while (record_gpu_queue_take(t->peers[1].video.queue, &frame) == RECORD_GPU_QUEUE_OK)
   {
      unsigned byte;
      received++;
      for (byte = 0; byte < 9000; byte++) CHECK(t->peers[1].video.bytes[frame.slot][byte] == test_controller_byte(byte, received));
      CHECK(record_gpu_queue_release(t->peers[1].video.queue, frame.slot));
   }
   CHECK(received == (reject ? 0U : 2U));
   CHECK(record_pyrowave_connection_free(connections[0])); t->peers[0].session = NULL;
   test_controller_free(t);
#if defined(_WIN32)
   CHECK(!WSACleanup());
#endif
   printf("PyroWave receiver dispatcher listen%u: cold configuration %s PASS\n", listen_worker, reject ? "rejection" : "acceptance");
}
#endif
