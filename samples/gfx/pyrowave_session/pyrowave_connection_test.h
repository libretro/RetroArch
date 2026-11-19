/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_CONNECTION_TEST_H
#define PYROWAVE_CONNECTION_TEST_H
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_connection_socket_t;
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int test_connection_socket_t;
#endif
#include "../../../record/record_pyrowave_connection.h"

static void test_connection_step(record_pyrowave_connection_t **c)
{
   fd_set read_set, write_set, errors;
   struct timeval timeout;
   test_connection_socket_t handles[2];
   unsigned interests[2], i;
   int largest = 0, selected;
   FD_ZERO(&read_set); FD_ZERO(&write_set); FD_ZERO(&errors);
   for (i = 0; i < 2; i++)
   {
      handles[i] = (test_connection_socket_t)record_pyrowave_connection_handle(c[i]);
      interests[i] = record_pyrowave_connection_interest(c[i]);
      if (!interests[i]) continue;
      if (interests[i] & RPYROWAVE_ENDPOINT_READABLE) FD_SET(handles[i], &read_set);
      if (interests[i] & RPYROWAVE_ENDPOINT_WRITABLE) FD_SET(handles[i], &write_set);
      FD_SET(handles[i], &errors);
#if !defined(_WIN32)
      if (handles[i] > largest) largest = handles[i];
#endif
   }
   timeout.tv_sec = 0; timeout.tv_usec = 100000; /* Offline watchdog only. */
   selected = select(largest + 1, &read_set, &write_set, &errors, &timeout);
   CHECK(selected >= 0);
   for (i = 0; i < 2; i++)
   {
      unsigned ready = 0;
      if (!interests[i]) continue;
      if (FD_ISSET(handles[i], &read_set)) ready |= RPYROWAVE_ENDPOINT_READABLE;
      if (FD_ISSET(handles[i], &write_set)) ready |= RPYROWAVE_ENDPOINT_WRITABLE;
      if (FD_ISSET(handles[i], &errors)) ready |= RPYROWAVE_ENDPOINT_EXCEPTION;
      if (ready) CHECK(record_pyrowave_connection_step(c[i], ready) >= 0);
   }
}

static void test_controller_tcp(unsigned ipv6, bool abort_pending)
{
   test_controller_t *t = (test_controller_t*)calloc(1, sizeof(*t));
   record_pyrowave_connection_t *connections[2];
   struct sockaddr_storage address;
   record_pyrowave_video_t binding;
   record_pyrowave_session_status_t status;
   record_gpu_frame_t frame;
   record_pyrowave_pcm_view_t audio;
   void *data;
   size_t capacity;
   unsigned i, received = 0;
#if defined(_WIN32)
   int address_bytes;
   WSADATA startup;
   CHECK(!WSAStartup(MAKEWORD(2, 2), &startup));
#else
   socklen_t address_bytes;
#endif
   CHECK(t);
   memset(&address, 0, sizeof(address));
   if (ipv6)
   {
      struct sockaddr_in6 *v6 = (struct sockaddr_in6*)&address;
      v6->sin6_family = AF_INET6; v6->sin6_addr.s6_addr[15] = 1;
      address_bytes = sizeof(*v6);
   }
   else
   {
      struct sockaddr_in *v4 = (struct sockaddr_in*)&address;
      v4->sin_family = AF_INET; v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      address_bytes = sizeof(*v4);
   }
   t->config.sequence.width = t->config.sequence.height = 128;
   t->config.maximum_frame_bytes = 10000; t->config.bit_depth = 8;
   t->config.fps_numerator = 60000; t->config.fps_denominator = 1001;
   t->config.audio_rate = 48000; t->config.audio_channels = 2; t->config.audio_channel_mask = 3;
   t->config.maximum_audio_frames = 32;
   for (i = 0; i < 2; i++)
   {
      t->peers[i].video.queue = record_gpu_queue_new(3);
      t->peers[i].video.reserved = RECORD_GPU_QUEUE_SLOTS;
      t->peers[i].pcm = record_pyrowave_pcm_new(2, 32, 2);
      CHECK(t->peers[i].video.queue && t->peers[i].pcm);
   }
   connections[0] = record_pyrowave_connection_new(&address, address_bytes, true, NULL, 0, NULL, NULL);
   CHECK(connections[0]);
   CHECK(record_pyrowave_connection_step(connections[0], RPYROWAVE_ENDPOINT_EXCEPTION) == RPYROWAVE_STREAM_ERROR);
   CHECK(!record_pyrowave_connection_interest(connections[0])
         && record_pyrowave_connection_handle(connections[0]) == (uintptr_t)-1);
   CHECK(record_pyrowave_connection_step(connections[0], 0) == RPYROWAVE_STREAM_ERROR);
   CHECK(record_pyrowave_connection_free(connections[0]));
   binding = test_controller_binding(&t->peers[0].video);
   connections[0] = record_pyrowave_connection_new(&address, address_bytes, true, &t->config,
         77, &binding, t->peers[0].pcm);
   CHECK(connections[0] && record_pyrowave_connection_interest(connections[0]) == RPYROWAVE_ENDPOINT_READABLE);
   CHECK(!getsockname((test_connection_socket_t)record_pyrowave_connection_handle(connections[0]),
            (struct sockaddr*)&address, &address_bytes));
   connections[1] = record_pyrowave_connection_new(&address, address_bytes, false, NULL, 0, NULL, NULL);
   CHECK(connections[1]);
   for (i = 0; i < 2; i++) t->peers[i].session = record_pyrowave_connection_session(connections[i]);
   CHECK(record_pyrowave_connection_step(connections[1], 0) >= 0);
   for (i = 0; i < 256 && test_controller_state(t->peers[1].session) != RECORD_PYROWAVE_CONFIGURING; i++)
      test_connection_step(connections);
   CHECK(i < 256 && !record_pyrowave_session_wants_write(t->peers[1].session));
   binding = test_controller_binding(&t->peers[1].video);
   CHECK(record_pyrowave_session_accept(t->peers[1].session, &binding, t->peers[1].pcm));
   CHECK(record_pyrowave_connection_interest(connections[1]) & RPYROWAVE_ENDPOINT_WRITABLE);
   for (i = 0; i < 256 && test_controller_state(t->peers[0].session) != RECORD_PYROWAVE_ACTIVE; i++)
      test_connection_step(connections);
   CHECK(i < 256 && !record_pyrowave_session_wants_write(t->peers[0].session));
   if (abort_pending)
   {
      test_controller_publish(t, 1);
      CHECK(record_pyrowave_connection_step(connections[0], 0) >= 0);
      CHECK(!record_pyrowave_connection_free(connections[0]));
      CHECK(!record_pyrowave_connection_interest(connections[0])
            && record_pyrowave_connection_handle(connections[0]) == (uintptr_t)-1);
      t->peers[0].video.ready = true;
      CHECK(record_pyrowave_connection_step(connections[0], 0) == RPYROWAVE_STREAM_FINISHED);
      CHECK(t->peers[0].video.released == 1);
      for (i = 0; i < 2; i++)
      {
         CHECK(record_pyrowave_connection_free(connections[i])); t->peers[i].session = NULL;
      }
      test_controller_free(t);
#if defined(_WIN32)
      CHECK(!WSACleanup());
#endif
      puts("PyroWave connection cancellation retains GPU-pending loan PASS");
      return;
   }
   test_controller_publish(t, 1); test_controller_publish(t, 2); t->peers[0].video.ready = true;
   CHECK(record_pyrowave_pcm_acquire(t->peers[0].pcm, &data, &capacity) == RECORD_GPU_QUEUE_OK);
   memset(data, 0x6b, 128); CHECK(record_pyrowave_pcm_submit(t->peers[0].pcm, 0, 32));
   CHECK(record_gpu_queue_finish(t->peers[0].video.queue) && record_pyrowave_pcm_finish(t->peers[0].pcm));
   CHECK(record_pyrowave_session_finish(t->peers[0].session));
   CHECK(record_pyrowave_connection_step(connections[0], 0) >= 0); /* Producer/control wake. */
   for (i = 0; i < 256 && test_controller_state(t->peers[1].session) != RECORD_PYROWAVE_ENDED; i++)
      test_connection_step(connections);
   CHECK(i < 256 && !record_pyrowave_connection_interest(connections[0]) && !record_pyrowave_connection_interest(connections[1]));
   while (record_gpu_queue_take(t->peers[1].video.queue, &frame) == RECORD_GPU_QUEUE_OK)
   {
      unsigned byte;
      received++;
      for (byte = 0; byte < 9000; byte++) CHECK(t->peers[1].video.bytes[frame.slot][byte] == test_controller_byte(byte, received));
      CHECK(record_gpu_queue_release(t->peers[1].video.queue, frame.slot));
   }
   CHECK(received == 2 && t->peers[0].video.released == 2);
   CHECK(record_pyrowave_pcm_take(t->peers[1].pcm, &audio) == RECORD_GPU_QUEUE_OK && audio.frames == 32);
   for (i = 0; i < 128; i++) CHECK(((const uint8_t*)audio.data)[i] == 0x6b);
   CHECK(record_pyrowave_pcm_release(t->peers[1].pcm, audio.slot));
   record_pyrowave_session_status(t->peers[1].session, &status);
   CHECK(status.generation == 77 && status.video_received == 2 && status.audio_received == 1);
   for (i = 0; i < 2; i++)
   {
      CHECK(record_pyrowave_connection_free(connections[i])); t->peers[i].session = NULL;
   }
   test_controller_free(t);
#if defined(_WIN32)
   CHECK(!WSACleanup());
#endif
   printf("PyroWave IPv%u connection: readiness startup, packet/PCM loans and EOS PASS\n", ipv6 ? 6 : 4);
}
#endif
