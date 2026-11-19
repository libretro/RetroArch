/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_D3D12_SESSION_TEST_H
#define PYROWAVE_D3D12_SESSION_TEST_H
typedef struct test_pyrowave_d12_wire
{
   uint8_t bytes[16384];
   size_t read_at, write_at;
} test_pyrowave_d12_wire_t;

typedef struct test_pyrowave_d12_peer
{
   test_pyrowave_d12_wire_t *input, *output;
   record_pyrowave_session_t *session;
} test_pyrowave_d12_peer_t;

typedef struct test_pyrowave_d12_transport
{
   test_pyrowave_d12_wire_t wire[2];
   test_pyrowave_d12_peer_t peers[2];
} test_pyrowave_d12_transport_t;

static int test_pyrowave_d12_read(void *user, void *data, size_t bytes)
{
   test_pyrowave_d12_wire_t *wire = ((test_pyrowave_d12_peer_t*)user)->input;
   if (bytes > 113) bytes = 113;
   if (bytes > wire->write_at - wire->read_at) bytes = wire->write_at - wire->read_at;
   memcpy(data, wire->bytes + wire->read_at, bytes); wire->read_at += bytes;
   return (int)bytes;
}

static int test_pyrowave_d12_write(void *user, const void *data, size_t bytes)
{
   test_pyrowave_d12_wire_t *wire = ((test_pyrowave_d12_peer_t*)user)->output;
   if (wire->write_at == wire->read_at) wire->write_at = wire->read_at = 0;
   if (bytes > 127) bytes = 127;
   if (bytes > sizeof(wire->bytes) - wire->write_at) bytes = sizeof(wire->bytes) - wire->write_at;
   memcpy(wire->bytes + wire->write_at, data, bytes); wire->write_at += bytes;
   return (int)bytes;
}

static unsigned test_pyrowave_d12_session_state(record_pyrowave_session_t *session)
{
   record_pyrowave_session_status_t status;
   record_pyrowave_session_status(session, &status);
   return status.state;
}

#define SESSION_CHECK(x) do { if (!(x)) { fprintf(stderr, "D3D12 session failed line %u: %s\n", \
   (unsigned)__LINE__, #x); goto end; } } while (0)

static int test_pyrowave_d3d12_session(test_pyrowave_d3d12_t *t)
{
   test_pyrowave_d12_transport_t *wire = (test_pyrowave_d12_transport_t*)calloc(1, sizeof(*wire));
   rpyrowave_stream_config_t config;
   rpyrowave_stream_io_t io;
   record_pyrowave_video_t video;
   record_pyrowave_session_status_t status;
   record_pyrowave_d3d12_image_t image;
   uint64_t ticket = 0;
   unsigned i;
   int success = 0;
   if (!wire) return 0;
   memset(&config, 0, sizeof(config));
   config.sequence = t->cfg.sequence; config.maximum_frame_bytes = t->cfg.maximum_bytes;
   config.fps_numerator = 60; config.fps_denominator = 1; config.bit_depth = t->cfg.bit_depth;
   t->cfg.decode = false; t->send = record_pyrowave_d3d12_new(&t->cfg);
   t->cfg.decode = true; t->receive = record_pyrowave_d3d12_new(&t->cfg);
   SESSION_CHECK(t->send && t->receive);
   for (i = 0; i < 2; i++)
   {
      test_pyrowave_d12_peer_t *peer = &wire->peers[i];
      peer->input = &wire->wire[i ^ 1]; peer->output = &wire->wire[i];
      io.user = peer; io.read = test_pyrowave_d12_read; io.write = test_pyrowave_d12_write;
      record_pyrowave_d3d12_session_video(i ? t->receive : t->send, &video);
      peer->session = i ? record_pyrowave_session_receiver_new(&io)
         : record_pyrowave_session_sender_new(&config, 7, &io, &video, NULL);
      SESSION_CHECK(peer->session);
   }
   for (i = 0; i < 1000 && test_pyrowave_d12_session_state(wire->peers[1].session) != RECORD_PYROWAVE_CONFIGURING; i++)
   {
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[0].session) >= 0);
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[1].session) >= 0);
   }
   SESSION_CHECK(i < 1000);
   record_pyrowave_session_status(wire->peers[1].session, &status);
   SESSION_CHECK(status.generation == 7 && status.config.sequence.width == t->width && status.config.sequence.height == t->height);
   record_pyrowave_d3d12_session_video(t->receive, &video);
   SESSION_CHECK(record_pyrowave_session_accept(wire->peers[1].session, &video, NULL));
   for (i = 0; i < 1000 && test_pyrowave_d12_session_state(wire->peers[0].session) != RECORD_PYROWAVE_ACTIVE; i++)
   {
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[0].session) >= 0);
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[1].session) >= 0);
   }
   SESSION_CHECK(i < 1000 && test_pyrowave_d3d12_begin(t, 0));
   SESSION_CHECK(record_pyrowave_d3d12_push(t->send, t->lists[0], t->fences[0], t->values[0], t->source,
            0, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
   SESSION_CHECK(record_pyrowave_d3d12_finish(t->send) && record_pyrowave_session_finish(wire->peers[0].session));
   SESSION_CHECK(record_pyrowave_session_pump(wire->peers[0].session) >= 0);
   record_pyrowave_session_status(wire->peers[0].session, &status);
   SESSION_CHECK(!status.video_sent);
   SESSION_CHECK(test_pyrowave_d3d12_submit(t, 0) && record_pyrowave_d3d12_poll(t->send));
   for (i = 0; i < 10000; i++)
   {
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[0].session) >= 0);
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[1].session) >= 0);
      record_pyrowave_session_status(wire->peers[1].session, &status);
      if (status.video_received) break;
   }
   SESSION_CHECK(i < 10000 && status.video_received == 1 && test_pyrowave_d3d12_begin(t, 2));
   SESSION_CHECK(record_pyrowave_d3d12_decode(t->receive, t->lists[2], t->fences[2], t->values[2]) == RECORD_GPU_QUEUE_OK);
   SESSION_CHECK(test_pyrowave_d3d12_submit(t, 2) && record_pyrowave_d3d12_poll(t->receive));
   SESSION_CHECK(record_pyrowave_d3d12_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
   SESSION_CHECK(test_pyrowave_d3d12_pixels(t, &image));
   for (i = 0; i < 1000 && test_pyrowave_d12_session_state(wire->peers[1].session) != RECORD_PYROWAVE_ENDED; i++)
   {
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[0].session) >= 0);
      SESSION_CHECK(record_pyrowave_session_pump(wire->peers[1].session) >= 0);
   }
   record_pyrowave_session_status(wire->peers[0].session, &status);
   SESSION_CHECK(i < 1000 && status.state == RECORD_PYROWAVE_ENDED && status.video_sent == 1);
   success = 1;
end:
   if (!success && !test_pyrowave_d3d12_quiesce(t))
   {
      fputs("D3D12 session queue did not quiesce; leaving resources to process teardown\n", stderr);
      exit(EXIT_FAILURE);
   }
   for (i = 0; i < 2; i++)
   {
      record_pyrowave_session_t *session = wire->peers[i].session;
      if (!record_pyrowave_session_free(session))
      {
         /* Caller resets lists and abandons the backend on this failure. */
         record_pyrowave_session_abandon_video(session);
         if (!record_pyrowave_session_free(session)) success = 0;
      }
   }
   if (success)
   {
      if (!record_pyrowave_d3d12_free(t->send)) success = 0;
      else t->send = NULL;
      if (!record_pyrowave_d3d12_free(t->receive)) success = 0;
      else t->receive = NULL;
   }
   if (success) printf("D3D12 session %ux%u mode%u: handshake, GPU packet transport, decode/display and EOS PASS\n",
         t->width, t->height, t->mode);
   free(wire);
   return success;
}
#undef SESSION_CHECK
#endif
