/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <formats/rpyrowave_stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef RPYROWAVE_STREAM_SOCKET_TEST
#include <net/rpyrowave_socket.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket_t;
#define TEST_SOCKET_INVALID INVALID_SOCKET
#define test_socket_close closesocket
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
typedef int test_socket_t;
#define TEST_SOCKET_INVALID (-1)
#define test_socket_close close
#endif
#endif

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Stream failed line %u: %s\n", \
   (unsigned)__LINE__, #expr); exit(EXIT_FAILURE); } } while (0)
#define TEST_FRAME_CAPACITY 65536
#define TEST_WIRE_CAPACITY 131072

typedef struct test_wire
{
   uint8_t bytes[TEST_WIRE_CAPACITY];
   size_t written, read;
   int closed;
} test_wire_t;

typedef struct test_peer
{
   rpyrowave_stream_t *stream;
   test_wire_t *input, *output;
   uint8_t video[TEST_FRAME_CAPACITY];
   uint8_t audio[1024];
   uint64_t generation;
   size_t write_budget, maximum_io, direct_bytes;
   unsigned reads, writes, block_period;
   unsigned events[7], acquired[5], committed[5], cancelled[5], dropped[5], sent[5], unsent[5];
   unsigned order[8], order_count;
   uint32_t video_hash, audio_hash;
   unsigned reason;
   int configured, started, reject, drop_video, commit_failure, short_span, mutate;
} test_peer_t;

typedef struct test_pair
{
   test_peer_t peer[2];
   test_wire_t wire[2];
   rpyrowave_stream_config_t config;
} test_pair_t;

typedef struct test_fixture
{
   uint8_t *bytes;
   size_t size;
   uint32_t hash;
} test_fixture_t;

static uint32_t test_hash(const void *data, size_t bytes)
{
   const uint8_t *p = (const uint8_t*)data;
   uint32_t hash = 2166136261U;
   size_t i;
   for (i = 0; i < bytes; i++) hash = (hash ^ p[i]) * 16777619U;
   return hash;
}

static int test_read(void *user, void *data, size_t bytes)
{
   test_peer_t *p = (test_peer_t*)user;
   test_wire_t *w = p->input;
   size_t count = w->written - w->read;
   uintptr_t address = (uintptr_t)data;
   p->reads++;
   if (p->block_period && !(p->reads % p->block_period)) return 0;
   if (!count) return w->closed ? -2 : 0;
   if (count > bytes) count = bytes;
   if (count > p->maximum_io) count = p->maximum_io;
   if ((address >= (uintptr_t)p->video && address + count <= (uintptr_t)p->video + sizeof(p->video))
         || (address >= (uintptr_t)p->audio && address + count <= (uintptr_t)p->audio + sizeof(p->audio)))
      p->direct_bytes += count;
   memcpy(data, w->bytes + w->read, count);
   w->read += count;
   return (int)count;
}

static int test_write(void *user, const void *data, size_t bytes)
{
   test_peer_t *p = (test_peer_t*)user;
   test_wire_t *w = p->output;
   size_t count = bytes;
   p->writes++;
   if (p->block_period && !(p->writes % p->block_period)) return 0;
   if (count > p->maximum_io) count = p->maximum_io;
   if (count > p->write_budget) count = p->write_budget;
   if (count > sizeof(w->bytes) - w->written) count = sizeof(w->bytes) - w->written;
   if (!count) return 0;
   memcpy(w->bytes + w->written, data, count);
   w->written += count;
   p->write_budget -= count;
   return (int)count;
}

static int test_acquire(void *user, const rpyrowave_stream_record_t *record, void **data, size_t *capacity)
{
   test_peer_t *p = (test_peer_t*)user;
   CHECK(record->kind == RPYROWAVE_STREAM_VIDEO || record->kind == RPYROWAVE_STREAM_AUDIO);
   CHECK(record->generation == p->generation);
   if (record->kind == RPYROWAVE_STREAM_VIDEO && p->drop_video) return 0;
   p->acquired[record->kind]++;
   *data = record->kind == RPYROWAVE_STREAM_VIDEO ? p->video : p->audio;
   *capacity = record->kind == RPYROWAVE_STREAM_VIDEO ? sizeof(p->video) : sizeof(p->audio);
   if (p->short_span) *capacity = 1;
   return 1;
}

static int test_commit(void *user, const rpyrowave_stream_record_t *record, void *data)
{
   test_peer_t *p = (test_peer_t*)user;
   if (p->commit_failure) return 0;
   CHECK(data == (record->kind == RPYROWAVE_STREAM_VIDEO ? (void*)p->video : (void*)p->audio));
   CHECK(test_hash(data, record->bytes) == (record->kind == RPYROWAVE_STREAM_VIDEO ? p->video_hash : p->audio_hash));
   if (record->kind == RPYROWAVE_STREAM_VIDEO)
      CHECK(record->serial == ((uint64_t)1 << 40) + 1 && record->timestamp == (uint64_t)1 << 44 && record->duration == 16683);
   CHECK(p->order_count < 8);
   p->order[p->order_count++] = record->kind;
   p->committed[record->kind]++;
   return 1;
}

static void test_cancel(void *user, const rpyrowave_stream_record_t *record, void *data)
{
   test_peer_t *p = (test_peer_t*)user;
   CHECK(data == (record->kind == RPYROWAVE_STREAM_VIDEO ? (void*)p->video : (void*)p->audio));
   p->cancelled[record->kind]++;
}

static void test_drop(void *user, const rpyrowave_stream_record_t *record)
{
   test_peer_t *p = (test_peer_t*)user;
   p->dropped[record->kind]++;
}

static void test_sent(void *user, const rpyrowave_stream_record_t *record, const void *data, void *token, int sent)
{
   test_peer_t *p = (test_peer_t*)user;
   CHECK(data == (record->kind == RPYROWAVE_STREAM_VIDEO ? (void*)p->video : (void*)p->audio));
   CHECK(token == p);
   if (sent) p->sent[record->kind]++;
   else p->unsent[record->kind]++;
   if (p->mutate) memset((void*)data, 0xa5, record->bytes);
}

static void test_event(void *user, unsigned event, const rpyrowave_stream_config_t *config,
      uint64_t generation, unsigned reason)
{
   test_peer_t *p = (test_peer_t*)user;
   CHECK(event >= 1 && event <= 6);
   p->events[event]++;
   p->reason = reason;
   if (event == RPYROWAVE_STREAM_CONFIGURE)
   {
      CHECK(generation == p->generation && !rpyrowave_stream_config_valid(config));
      CHECK(config->sequence.width == 130 && config->sequence.height == 98 && config->audio_rate == 48000);
      p->configured = 1;
   }
   if (event == RPYROWAVE_STREAM_STARTED) p->started = 1;
}

static void test_callbacks(test_peer_t *p, rpyrowave_stream_callbacks_t *callbacks)
{
   memset(callbacks, 0, sizeof(*callbacks));
   callbacks->user = p; callbacks->acquire = test_acquire; callbacks->commit = test_commit;
   callbacks->cancel = test_cancel; callbacks->dropped = test_drop; callbacks->sent = test_sent;
   callbacks->event = test_event;
}

static test_fixture_t test_fixture_new(void)
{
   test_fixture_t fixture;
   rpyrowave_encoder_t *encoder = rpyrowave_encoder_new(130, 98, 0, 1024 * 1024);
   rpyrowave_image_t image;
   float *planes = (float*)calloc(3 * 130 * 98, sizeof(float));
   uint32_t random = 1;
   unsigned component;
   fixture.bytes = (uint8_t*)malloc(TEST_FRAME_CAPACITY);
   CHECK(encoder && planes && fixture.bytes);
   memset(&image, 0, sizeof(image));
   image.sequence.width = 130; image.sequence.height = 98; image.stride = 130;
   for (component = 0; component < 3; component++)
   {
      float *plane = planes + component * 130 * 98;
      unsigned width = component ? 65 : 130;
      unsigned height = component ? 49 : 98;
      unsigned x;
      unsigned y;
      image.planes[component] = plane;
      for (y = 0; y < height; y++) for (x = 0; x < width; x++)
      {
         random = random * 1664525U + 1013904223U;
         plane[y * 130 + x] = (float)(random >> 16) / 65535.0f;
      }
   }
   CHECK(!rpyrowave_encode_frame(encoder, &image, 32768, fixture.bytes, TEST_FRAME_CAPACITY, &fixture.size));
   CHECK(fixture.size > RPYROWAVE_STREAM_FRAGMENT_BYTES * 2 && fixture.size <= 32768);
   fixture.hash = test_hash(fixture.bytes, fixture.size);
   rpyrowave_encoder_free(encoder);
   free(planes);
   return fixture;
}

static test_pair_t *test_pair_new(const test_fixture_t *fixture, size_t maximum_io)
{
   test_pair_t *pair = (test_pair_t*)calloc(1, sizeof(*pair));
   unsigned i;
   CHECK(pair);
   pair->config.sequence.width = 130; pair->config.sequence.height = 98;
   pair->config.maximum_frame_bytes = TEST_FRAME_CAPACITY;
   pair->config.fps_numerator = 60000; pair->config.fps_denominator = 1001;
   pair->config.audio_rate = 48000; pair->config.audio_channels = 2; pair->config.audio_channel_mask = 3;
   pair->config.maximum_audio_frames = 256; pair->config.bit_depth = 8;
   for (i = 0; i < 2; i++)
   {
      test_peer_t *p = &pair->peer[i];
      rpyrowave_stream_io_t io;
      rpyrowave_stream_callbacks_t callbacks;
      unsigned j;
      p->generation = ((uint64_t)0x10203040U << 32) | 0x50607080U;
      p->input = &pair->wire[i ^ 1]; p->output = &pair->wire[i];
      p->maximum_io = maximum_io;
      p->write_budget = (size_t)-1;
      p->video_hash = fixture->hash;
      memcpy(p->video, fixture->bytes, fixture->size);
      for (j = 0; j < sizeof(p->audio); j++) p->audio[j] = (uint8_t)(j * 17 + 3);
      p->audio_hash = test_hash(p->audio, sizeof(p->audio));
      io.user = p; io.read = test_read; io.write = test_write;
      test_callbacks(p, &callbacks);
      p->stream = i ? rpyrowave_stream_receiver_new(&io, &callbacks)
         : rpyrowave_stream_sender_new(&pair->config, p->generation, &io, &callbacks);
      CHECK(p->stream);
   }
   return pair;
}

static int test_pump(test_peer_t *p)
{
   unsigned reads = p->reads;
   unsigned writes = p->writes;
   int result = rpyrowave_stream_pump(p->stream);
   CHECK(p->reads - reads <= 8 && p->writes - writes <= 8);
   return result;
}

static void test_handshake(test_pair_t *pair, unsigned reject)
{
   unsigned i;
   for (i = 0; i < 4096; i++)
   {
      CHECK(test_pump(&pair->peer[0]) >= 0);
      CHECK(test_pump(&pair->peer[1]) >= 0);
      if (pair->peer[1].configured == 1)
      {
         CHECK(rpyrowave_stream_accept(pair->peer[1].stream, reject) == RPYROWAVE_STREAM_PROGRESS);
         pair->peer[1].configured = 2;
      }
      if ((!reject && pair->peer[0].started && pair->peer[1].started)
            || (reject && pair->peer[0].events[RPYROWAVE_STREAM_REJECTED])) break;
   }
   CHECK(i < 4096 && pair->peer[1].events[RPYROWAVE_STREAM_CONFIGURE] == 1);
}

static void test_pair_free(test_pair_t *pair)
{
   rpyrowave_stream_free(pair->peer[0].stream);
   rpyrowave_stream_free(pair->peer[1].stream);
   free(pair);
}

static void test_offer_video(test_pair_t *pair, size_t bytes)
{
   rpyrowave_stream_record_t record;
   memset(&record, 0, sizeof(record));
   record.generation = pair->peer[0].generation;
   record.kind = RPYROWAVE_STREAM_VIDEO;
   record.serial = ((uint64_t)1 << 40) + 1;
   record.timestamp = (uint64_t)1 << 44;
   record.duration = 16683;
   record.bytes = (uint32_t)bytes;
   CHECK(rpyrowave_stream_offer(pair->peer[0].stream, &record, pair->peer[0].video, &pair->peer[0])
         == RPYROWAVE_STREAM_PROGRESS);
}

static void test_offer_audio(test_pair_t *pair, uint64_t serial)
{
   rpyrowave_stream_record_t record;
   uint64_t end;
   memset(&record, 0, sizeof(record));
   record.generation = pair->peer[0].generation;
   record.kind = RPYROWAVE_STREAM_AUDIO;
   record.serial = serial;
   record.bytes = sizeof(pair->peer[0].audio);
   CHECK(!rpyrowave_stream_time(serial, 48000, 1, &record.timestamp));
   CHECK(!rpyrowave_stream_time(serial + 256, 48000, 1, &end));
   record.duration = end - record.timestamp;
   CHECK(rpyrowave_stream_offer(pair->peer[0].stream, &record, pair->peer[0].audio, &pair->peer[0])
         == RPYROWAVE_STREAM_PROGRESS);
}

static void test_roundtrip(const test_fixture_t *fixture, size_t maximum_io, unsigned drop)
{
   test_pair_t *pair = test_pair_new(fixture, maximum_io);
   unsigned i;
   pair->peer[0].block_period = pair->peer[1].block_period = 7;
   test_handshake(pair, 0);
   pair->peer[0].mutate = 1;
   pair->peer[1].drop_video = (int)drop;
   test_offer_video(pair, fixture->size);
   test_offer_audio(pair, ((uint64_t)1 << 35) + 17);
   CHECK(rpyrowave_stream_finish(pair->peer[0].stream) == RPYROWAVE_STREAM_PROGRESS);
   for (i = 0; i < 100000; i++)
   {
      CHECK(test_pump(&pair->peer[0]) >= 0);
      CHECK(test_pump(&pair->peer[1]) >= 0);
      if (pair->peer[1].events[RPYROWAVE_STREAM_ENDED]) break;
   }
   CHECK(i < 100000);
   CHECK(pair->peer[0].sent[RPYROWAVE_STREAM_VIDEO] == 1 && pair->peer[0].sent[RPYROWAVE_STREAM_AUDIO] == 1);
   CHECK(pair->peer[1].committed[RPYROWAVE_STREAM_AUDIO] == 1);
   CHECK(pair->peer[1].committed[RPYROWAVE_STREAM_VIDEO] == (drop ? 0U : 1U));
   CHECK(pair->peer[1].dropped[RPYROWAVE_STREAM_VIDEO] == drop);
   CHECK(pair->peer[1].direct_bytes == sizeof(pair->peer[0].audio) + (drop ? 0 : fixture->size));
   CHECK(!pair->peer[1].cancelled[RPYROWAVE_STREAM_VIDEO]);
   CHECK(pair->peer[1].order[0] == RPYROWAVE_STREAM_AUDIO);
   test_pair_free(pair);
}

static void test_preemption(const test_fixture_t *fixture)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   size_t boundary;
   unsigned i;
   test_handshake(pair, 0);
   boundary = pair->wire[0].written + 64 + RPYROWAVE_STREAM_FRAGMENT_BYTES;
   pair->peer[0].write_budget = 64 + RPYROWAVE_STREAM_FRAGMENT_BYTES;
   test_offer_video(pair, fixture->size);
   CHECK(test_pump(&pair->peer[0]) > 0);
   CHECK(pair->wire[0].written == boundary);
   CHECK(test_pump(&pair->peer[1]) > 0 && pair->peer[1].acquired[RPYROWAVE_STREAM_VIDEO] == 1);
   test_offer_audio(pair, 0);
   pair->peer[0].write_budget = (size_t)-1;
   CHECK(test_pump(&pair->peer[0]) > 0);
   CHECK(pair->wire[0].bytes[boundary + 6] == RPYROWAVE_STREAM_AUDIO);
   for (i = 0; i < 1000; i++)
   {
      CHECK(test_pump(&pair->peer[0]) >= 0 && test_pump(&pair->peer[1]) >= 0);
      if (pair->peer[1].committed[RPYROWAVE_STREAM_VIDEO]) break;
   }
   CHECK(i < 1000 && pair->peer[1].committed[RPYROWAVE_STREAM_AUDIO] == 1);
   test_pair_free(pair);
}

static void test_cancellation(const test_fixture_t *fixture, unsigned eof)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   unsigned writes;
   test_handshake(pair, 0);
   pair->peer[0].write_budget = 64 + RPYROWAVE_STREAM_FRAGMENT_BYTES;
   test_offer_video(pair, fixture->size);
   CHECK(test_pump(&pair->peer[0]) > 0 && test_pump(&pair->peer[1]) > 0);
   CHECK(pair->peer[1].acquired[RPYROWAVE_STREAM_VIDEO] == 1 && !pair->peer[1].committed[RPYROWAVE_STREAM_VIDEO]);
   if (eof)
   {
      pair->wire[0].closed = 1;
      CHECK(test_pump(&pair->peer[1]) == RPYROWAVE_STREAM_ERROR);
      CHECK(pair->peer[1].events[RPYROWAVE_STREAM_BROKEN] == 1);
   }
   else rpyrowave_stream_abort(pair->peer[1].stream);
   CHECK(pair->peer[1].cancelled[RPYROWAVE_STREAM_VIDEO] == 1);
   rpyrowave_stream_abort(pair->peer[1].stream);
   rpyrowave_stream_abort(pair->peer[0].stream);
   CHECK(pair->peer[0].unsent[RPYROWAVE_STREAM_VIDEO] == 1);
   writes = pair->peer[0].writes;
   CHECK(test_pump(&pair->peer[0]) == RPYROWAVE_STREAM_FINISHED && pair->peer[0].writes == writes);
   CHECK(pair->peer[1].cancelled[RPYROWAVE_STREAM_VIDEO] == 1);
   test_pair_free(pair);
}

static void test_bad_wire(const test_fixture_t *fixture, unsigned kind)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   size_t start;
   test_handshake(pair, 0);
   start = pair->wire[0].written;
   pair->peer[0].write_budget = 64;
   test_offer_video(pair, fixture->size);
   CHECK(test_pump(&pair->peer[0]) > 0);
   if (kind == 0) pair->wire[0].bytes[start + 24] ^= 1;
   if (kind == 1) pair->wire[0].bytes[start + 7] = 0;
   if (kind == 2) pair->wire[0].bytes[start + 20] = 1;
   CHECK(test_pump(&pair->peer[1]) == RPYROWAVE_STREAM_ERROR);
   CHECK(pair->peer[1].events[RPYROWAVE_STREAM_BROKEN] == 1 && !pair->peer[1].acquired[RPYROWAVE_STREAM_VIDEO]);
   test_pair_free(pair);
}

static void test_bad_sink(const test_fixture_t *fixture, unsigned kind)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   unsigned i;
   test_handshake(pair, 0);
   pair->peer[1].short_span = kind == 0;
   pair->peer[1].commit_failure = kind == 1;
   test_offer_video(pair, fixture->size);
   for (i = 0; i < 1000; i++)
   {
      CHECK(test_pump(&pair->peer[0]) >= 0);
      if (test_pump(&pair->peer[1]) < 0) break;
   }
   CHECK(i < 1000 && pair->peer[1].cancelled[RPYROWAVE_STREAM_VIDEO] == 1);
   CHECK(pair->peer[1].reason == RPYROWAVE_STREAM_SINK_ERROR);
   test_pair_free(pair);
}

static void test_times(void)
{
   uint64_t value = 99;
   CHECK(!rpyrowave_stream_time(48000, 48000, 1, &value) && value == 1000000);
   CHECK(!rpyrowave_stream_time(3, 48000, 1, &value) && value == 62);
   CHECK(!rpyrowave_stream_time(60000, 60000, 1001, &value) && value == 1001000000);
   CHECK(!rpyrowave_stream_time((uint64_t)-1, 1000000, 1, &value) && value == (uint64_t)-1);
   value = 99;
   CHECK(rpyrowave_stream_time((uint64_t)-1, 1, 1, &value) == RPYROWAVE_INVALID && value == 99);
   CHECK(rpyrowave_stream_time(0, 0, 1, &value) == RPYROWAVE_INVALID);
}

static void test_config_vector(const test_fixture_t *fixture)
{
   static const uint8_t expected[128] =
   {
      'R','P','W','S',1,0,1,0,64,0,0,0,64,0,0,0,
      0,0,0,0,0,0,0,0,0x80,0x70,0x60,0x50,0x40,0x30,0x20,0x10,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      130,0,0,0,98,0,0,0,8,0,0,0,0,0,0,0,
      0,0,1,0,0x60,0xea,0,0,0xe9,3,0,0,0x80,0xbb,0,0,
      2,0,0,0,3,0,0,0,1,0,0,0,0,1,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
   };
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   CHECK(test_pump(&pair->peer[0]) > 0 && pair->wire[0].written == sizeof(expected));
   CHECK(!memcmp(pair->wire[0].bytes, expected, sizeof(expected)));
   pair->wire[0].bytes[64 + 48] = 1;
   CHECK(test_pump(&pair->peer[1]) == RPYROWAVE_STREAM_ERROR);
   CHECK(!pair->peer[1].events[RPYROWAVE_STREAM_CONFIGURE]);
   test_pair_free(pair);
}

static void test_audio_gap(const test_fixture_t *fixture)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   unsigned i;
   size_t boundary;
   test_handshake(pair, 0);
   test_offer_audio(pair, 0);
   for (i = 0; i < 10 && !pair->peer[1].committed[RPYROWAVE_STREAM_AUDIO]; i++)
      CHECK(test_pump(&pair->peer[0]) >= 0 && test_pump(&pair->peer[1]) >= 0);
   CHECK(i < 10);
   boundary = pair->wire[0].written;
   test_offer_audio(pair, 512);
   CHECK(test_pump(&pair->peer[0]) > 0 && pair->wire[0].bytes[boundary + 7] == 4);
   CHECK(test_pump(&pair->peer[1]) > 0 && pair->peer[1].committed[RPYROWAVE_STREAM_AUDIO] == 2);
   test_pair_free(pair);
}

#ifdef RPYROWAVE_STREAM_SOCKET_TEST
static void test_socket_wait(test_socket_t a, test_socket_t b)
{
   fd_set read_set;
   struct timeval timeout;
   int nfds = 0;
#ifndef _WIN32
   CHECK(a < FD_SETSIZE && b < FD_SETSIZE);
#endif
   FD_ZERO(&read_set);
   FD_SET(a, &read_set);
   FD_SET(b, &read_set);
   timeout.tv_sec = 0;
   timeout.tv_usec = 10000;
#ifndef _WIN32
   nfds = (a > b ? a : b) + 1;
#endif
   CHECK(select(nfds, &read_set, NULL, NULL, &timeout) >= 0);
}

static void test_endpoint_wait(rpyrowave_endpoint_t *connector,
      rpyrowave_endpoint_t *listener, unsigned *events)
{
   fd_set read_set, write_set, error_set;
   struct timeval timeout;
   test_socket_t handles[2];
   unsigned i;
   int nfds = 0;
   handles[0] = (test_socket_t)rpyrowave_endpoint_handle(connector);
   handles[1] = (test_socket_t)rpyrowave_endpoint_handle(listener);
   FD_ZERO(&read_set);
   FD_ZERO(&write_set);
   FD_ZERO(&error_set);
   for (i = 0; i < 2; i++)
   {
      events[i] = 0;
      if (handles[i] == TEST_SOCKET_INVALID) continue;
#ifndef _WIN32
      CHECK(handles[i] < FD_SETSIZE);
      if (handles[i] >= nfds) nfds = handles[i] + 1;
#endif
      if (i) FD_SET(handles[i], &read_set);
      else FD_SET(handles[i], &write_set);
      FD_SET(handles[i], &error_set);
   }
   timeout.tv_sec = 0;
   timeout.tv_usec = 10000;
   CHECK(select(nfds, &read_set, &write_set, &error_set, &timeout) >= 0);
   for (i = 0; i < 2; i++)
   {
      if (handles[i] == TEST_SOCKET_INVALID) continue;
      if (FD_ISSET(handles[i], &read_set)) events[i] |= RPYROWAVE_ENDPOINT_READABLE;
      if (FD_ISSET(handles[i], &write_set)) events[i] |= RPYROWAVE_ENDPOINT_WRITABLE;
      if (FD_ISSET(handles[i], &error_set)) events[i] |= RPYROWAVE_ENDPOINT_EXCEPTION;
   }
}

static void test_endpoint_failures(void)
{
   struct sockaddr_in address;
   rpyrowave_endpoint_t *endpoint;
   rpyrowave_socket_t *connected = NULL;
   test_socket_t reserved;
   unsigned events[2] = { 0, 0 };
   unsigned i;
   int result = RPYROWAVE_ENDPOINT_PENDING;
#ifdef _WIN32
   int bytes = sizeof(address);
#else
   socklen_t bytes = sizeof(address);
#endif
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   reserved = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   CHECK(reserved != TEST_SOCKET_INVALID);
   CHECK(!bind(reserved, (struct sockaddr*)&address, sizeof(address)));
   CHECK(!getsockname(reserved, (struct sockaddr*)&address, &bytes));
   CHECK(!rpyrowave_endpoint_new(&address, sizeof(address), 1));
   endpoint = rpyrowave_endpoint_new(&address, sizeof(address), 0);
   /* A bound, non-listening port is reserved throughout the refusal check. */
   if (endpoint)
   {
      for (i = 0; i < 2000; i++)
      {
         result = rpyrowave_endpoint_step(endpoint, events[0], &connected);
         if (result != RPYROWAVE_ENDPOINT_PENDING) break;
         test_endpoint_wait(endpoint, NULL, events);
      }
      CHECK(i < 2000 && result == RPYROWAVE_ENDPOINT_ERROR && !connected);
      CHECK(rpyrowave_endpoint_handle(endpoint) == (uintptr_t)-1);
      CHECK(rpyrowave_endpoint_step(endpoint, 0, &connected) == RPYROWAVE_ENDPOINT_ERROR);
      rpyrowave_endpoint_free(endpoint);
   }
   CHECK(!test_socket_close(reserved));
   address.sin_port = 0;
   endpoint = rpyrowave_endpoint_new(&address, sizeof(address), 1);
   CHECK(endpoint);
   reserved = (test_socket_t)rpyrowave_endpoint_handle(endpoint);
   rpyrowave_endpoint_free(endpoint);
   CHECK(getsockname(reserved, (struct sockaddr*)&address, &bytes) != 0);
   reserved = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   CHECK(reserved != TEST_SOCKET_INVALID);
   CHECK(!rpyrowave_socket_new((uintptr_t)reserved));
   /* Failed adoption leaves the native descriptor with its caller. */
   CHECK(!test_socket_close(reserved));
   rpyrowave_endpoint_free(NULL);
}

static void test_tcp(const test_fixture_t *fixture, unsigned ipv6)
{
   test_pair_t *pair = test_pair_new(fixture, TEST_WIRE_CAPACITY);
   struct sockaddr_storage address;
   test_socket_t sockets[2];
   rpyrowave_socket_t *adapters[2] = { NULL, NULL };
   rpyrowave_endpoint_t *endpoints[2];
   unsigned events[2] = { 0, 0 };
   unsigned i;
   int send_buffer = 1024;
#ifdef _WIN32
   WSADATA startup;
   int address_bytes = sizeof(address);
   CHECK(!WSAStartup(MAKEWORD(2, 2), &startup));
#else
   socklen_t address_bytes = sizeof(address);
#endif
   if (!ipv6) test_endpoint_failures();
   memset(&address, 0, sizeof(address));
   if (ipv6)
   {
      struct sockaddr_in6 *v6 = (struct sockaddr_in6*)&address;
      v6->sin6_family = AF_INET6;
      v6->sin6_addr.s6_addr[15] = 1;
   }
   else
   {
      struct sockaddr_in *v4 = (struct sockaddr_in*)&address;
      v4->sin_family = AF_INET;
      v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   }
   CHECK(!rpyrowave_endpoint_new(NULL, sizeof(address), 1));
   CHECK(!rpyrowave_endpoint_new(&address, 1, 1));
   CHECK(!rpyrowave_endpoint_new(&address, sizeof(address), 2));
   endpoints[1] = rpyrowave_endpoint_new(&address, sizeof(address), 1);
   CHECK(endpoints[1]);
   CHECK(rpyrowave_endpoint_step(endpoints[1], RPYROWAVE_ENDPOINT_READABLE, &adapters[1])
         == RPYROWAVE_ENDPOINT_PENDING && !adapters[1]);
   CHECK(!getsockname((test_socket_t)rpyrowave_endpoint_handle(endpoints[1]),
            (struct sockaddr*)&address, &address_bytes));
   endpoints[0] = rpyrowave_endpoint_new(&address, sizeof(address), 0);
   CHECK(endpoints[0]);
   for (i = 0; i < 2000; i++)
   {
      unsigned peer;
      for (peer = 0; peer < 2; peer++)
      {
         int result;
         if (adapters[peer]) continue;
         result = rpyrowave_endpoint_step(endpoints[peer], events[peer], &adapters[peer]);
         CHECK(result >= 0);
         CHECK((result == RPYROWAVE_ENDPOINT_READY) == (adapters[peer] != NULL));
      }
      if (adapters[0] && adapters[1]) break;
      test_endpoint_wait(endpoints[0], endpoints[1], events);
   }
   CHECK(i < 2000);
   for (i = 0; i < 2; i++)
   {
      rpyrowave_socket_t *unexpected = adapters[i];
      CHECK(rpyrowave_endpoint_handle(endpoints[i]) == (uintptr_t)-1);
      CHECK(rpyrowave_endpoint_step(endpoints[i], 0, &unexpected) == RPYROWAVE_ENDPOINT_ERROR && !unexpected);
      rpyrowave_endpoint_free(endpoints[i]);
      sockets[i] = (test_socket_t)rpyrowave_socket_handle(adapters[i]);
   }
   for (i = 0; i < 2; i++)
   {
      rpyrowave_stream_io_t io;
      rpyrowave_stream_callbacks_t callbacks;
      CHECK(!setsockopt(sockets[i], SOL_SOCKET, SO_SNDBUF, (const char*)&send_buffer, sizeof(send_buffer)));
      rpyrowave_socket_io(adapters[i], &io);
      rpyrowave_stream_free(pair->peer[i].stream);
      memset(pair->peer[i].events, 0, sizeof(pair->peer[i].events));
      test_callbacks(&pair->peer[i], &callbacks);
      pair->peer[i].stream = i ? rpyrowave_stream_receiver_new(&io, &callbacks)
         : rpyrowave_stream_sender_new(&pair->config, pair->peer[i].generation, &io, &callbacks);
      CHECK(pair->peer[i].stream);
   }
   for (i = 0; i < 2000; i++)
   {
      int sent = test_pump(&pair->peer[0]);
      int received = test_pump(&pair->peer[1]);
      CHECK(sent >= 0 && received >= 0);
      if (pair->peer[1].configured == 1)
      {
         CHECK(rpyrowave_stream_accept(pair->peer[1].stream, 0) == RPYROWAVE_STREAM_PROGRESS);
         pair->peer[1].configured = 2;
         continue;
      }
      if (pair->peer[0].started && pair->peer[1].started) break;
      if (!sent && !received) test_socket_wait(sockets[0], sockets[1]);
   }
   CHECK(i < 2000);
   pair->peer[0].mutate = 1;
   test_offer_video(pair, fixture->size);
   test_offer_audio(pair, 0);
   CHECK(rpyrowave_stream_finish(pair->peer[0].stream) == RPYROWAVE_STREAM_PROGRESS);
   for (i = 0; i < 2000; i++)
   {
      int sent = test_pump(&pair->peer[0]);
      int received = test_pump(&pair->peer[1]);
      CHECK(sent >= 0 && received >= 0);
      if (pair->peer[1].events[RPYROWAVE_STREAM_ENDED]) break;
      if (!sent && !received) test_socket_wait(sockets[0], sockets[1]);
   }
   CHECK(i < 2000 && pair->peer[0].sent[RPYROWAVE_STREAM_VIDEO] == 1
         && pair->peer[0].sent[RPYROWAVE_STREAM_AUDIO] == 1);
   CHECK(pair->peer[1].committed[RPYROWAVE_STREAM_VIDEO] == 1 && pair->peer[1].committed[RPYROWAVE_STREAM_AUDIO] == 1);
   test_pair_free(pair);
   for (i = 0; i < 2; i++) rpyrowave_socket_free(adapters[i]);
#ifdef _WIN32
   WSACleanup();
#endif
   printf("Native IPv%u TCP PyroWave video/PCM session roundtrip PASS\n", ipv6 ? 6 : 4);
}
#endif

int main(void)
{
   static const unsigned maximum_io[] = { 1, 7, 31, 4096, 16384 };
   test_fixture_t fixture = test_fixture_new();
   unsigned i;
   test_times();
   test_config_vector(&fixture);
   test_audio_gap(&fixture);
   for (i = 0; i < sizeof(maximum_io) / sizeof(maximum_io[0]); i++) test_roundtrip(&fixture, maximum_io[i], 0);
   test_roundtrip(&fixture, 7, 1);
   test_preemption(&fixture);
   test_cancellation(&fixture, 0);
   test_cancellation(&fixture, 1);
   for (i = 0; i < 3; i++) test_bad_wire(&fixture, i);
   test_bad_sink(&fixture, 0);
   test_bad_sink(&fixture, 1);
   {
      test_pair_t *pair = test_pair_new(&fixture, 7);
      test_handshake(pair, 123);
      CHECK(pair->peer[0].reason == 123 && pair->peer[1].reason == 123);
      test_pair_free(pair);
   }
#ifdef RPYROWAVE_STREAM_SOCKET_TEST
   test_tcp(&fixture, 0);
   test_tcp(&fixture, 1);
#endif
   free(fixture.bytes);
   puts("PyroWave stream framing, audio timing, direct lends and cancellation PASS");
   return EXIT_SUCCESS;
}
