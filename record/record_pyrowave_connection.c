/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_connection.h"
#include <stdlib.h>

struct record_pyrowave_connection
{
   rpyrowave_endpoint_t *endpoint;
   rpyrowave_socket_t *socket;
   record_pyrowave_session_t *session;
   rpyrowave_stream_io_t io;
   bool listen, stopped, failed;
};

static int record_pyrowave_connection_read(void *user, void *data, size_t bytes)
{
   record_pyrowave_connection_t *c = (record_pyrowave_connection_t*)user;
   return c->socket ? c->io.read(c->io.user, data, bytes) : 0;
}

static int record_pyrowave_connection_write(void *user, const void *data, size_t bytes)
{
   record_pyrowave_connection_t *c = (record_pyrowave_connection_t*)user;
   return c->socket ? c->io.write(c->io.user, data, bytes) : 0;
}

record_pyrowave_connection_t *record_pyrowave_connection_new(const void *address, size_t address_bytes,
      bool listen, const rpyrowave_stream_config_t *cfg, uint64_t generation,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   rpyrowave_stream_io_t io;
   record_pyrowave_connection_t *c = (record_pyrowave_connection_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->listen = listen;
   c->endpoint = rpyrowave_endpoint_new(address, address_bytes, listen ? 1 : 0);
   io.user = c; io.read = record_pyrowave_connection_read; io.write = record_pyrowave_connection_write;
   c->session = cfg ? record_pyrowave_session_sender_new(cfg, generation, &io, video, pcm)
      : record_pyrowave_session_receiver_new(&io);
   if (!c->endpoint || !c->session)
   {
      record_pyrowave_connection_free(c);
      return NULL;
   }
   return c;
}

uintptr_t record_pyrowave_connection_handle(const record_pyrowave_connection_t *c)
{
   if (!c || c->stopped) return (uintptr_t)-1;
   return c->endpoint ? rpyrowave_endpoint_handle(c->endpoint) : rpyrowave_socket_handle(c->socket);
}

unsigned record_pyrowave_connection_interest(const record_pyrowave_connection_t *c)
{
   record_pyrowave_session_status_t status;
   if (!c || c->stopped) return 0;
   if (c->endpoint) return c->listen ? RPYROWAVE_ENDPOINT_READABLE
      : RPYROWAVE_ENDPOINT_WRITABLE | RPYROWAVE_ENDPOINT_EXCEPTION;
   record_pyrowave_session_status(c->session, &status);
   if (status.state >= RECORD_PYROWAVE_ENDED) return 0;
   return RPYROWAVE_ENDPOINT_READABLE | RPYROWAVE_ENDPOINT_EXCEPTION
      | (record_pyrowave_session_wants_write(c->session) ? RPYROWAVE_ENDPOINT_WRITABLE : 0);
}

bool record_pyrowave_connection_listening(const record_pyrowave_connection_t *c)
{
   return c && !c->stopped && c->endpoint && c->listen;
}

int record_pyrowave_connection_step(record_pyrowave_connection_t *c, unsigned readiness)
{
   int result;
   if (!c || readiness & ~7U) return RPYROWAVE_STREAM_ERROR;
   if (!c->stopped && c->endpoint)
   {
      result = rpyrowave_endpoint_step(c->endpoint, readiness, &c->socket);
      if (result == RPYROWAVE_ENDPOINT_PENDING) return RPYROWAVE_STREAM_BUSY;
      if (result == RPYROWAVE_ENDPOINT_ERROR)
      {
         record_pyrowave_connection_abort(c); c->failed = true;
         return RPYROWAVE_STREAM_ERROR;
      }
      rpyrowave_socket_io(c->socket, &c->io);
      rpyrowave_endpoint_free(c->endpoint); c->endpoint = NULL;
   }
   else if (!c->stopped && (readiness & RPYROWAVE_ENDPOINT_EXCEPTION))
   {
      record_pyrowave_connection_abort(c); c->failed = true;
      return RPYROWAVE_STREAM_ERROR;
   }
   result = record_pyrowave_session_pump(c->session);
   return c->failed ? RPYROWAVE_STREAM_ERROR : result;
}

record_pyrowave_session_t *record_pyrowave_connection_session(record_pyrowave_connection_t *c)
{
   return c ? c->session : NULL;
}

void record_pyrowave_connection_abort(record_pyrowave_connection_t *c)
{
   if (!c) return;
   record_pyrowave_session_abort(c->session);
   rpyrowave_endpoint_free(c->endpoint); c->endpoint = NULL;
   rpyrowave_socket_free(c->socket); c->socket = NULL;
   c->stopped = true;
}

bool record_pyrowave_connection_free(record_pyrowave_connection_t *c)
{
   if (!c) return true;
   record_pyrowave_connection_abort(c);
   if (!record_pyrowave_session_free(c->session)) return false;
   free(c);
   return true;
}
