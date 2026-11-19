/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_DISPATCH_H
#define RECORD_PYROWAVE_DISPATCH_H
#include "record_pyrowave_connection.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_dispatch record_pyrowave_dispatch_t;
enum record_pyrowave_dispatch_state
{
   RECORD_PYROWAVE_DISPATCH_STARTING,
   RECORD_PYROWAVE_DISPATCH_CONFIGURING,
   RECORD_PYROWAVE_DISPATCH_READY,
   RECORD_PYROWAVE_DISPATCH_DRAINING,
   RECORD_PYROWAVE_DISPATCH_DONE,
   RECORD_PYROWAVE_DISPATCH_CANCELLED,
   RECORD_PYROWAVE_DISPATCH_FAILED
};
/* Cold construction; resolved native address and initialized networking.
 * Borrows backend/pools through successful free. Owns one readiness-driven
 * I/O worker. Renderer/audio producers still own their GPU/PCM operations.
 * Unsupported platforms and threadless builds return NULL. */
record_pyrowave_dispatch_t *record_pyrowave_dispatch_new(const void *address, size_t address_bytes,
      bool listen, const rpyrowave_stream_config_t *cfg, uint64_t generation,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm);
/* Immutable native listening address captured before worker startup. This
 * reports an ephemeral port without exposing a worker-owned descriptor.
 * bytes is capacity on entry and required size on return. */
bool record_pyrowave_dispatch_bound_address(const record_pyrowave_dispatch_t *job,
      void *address, size_t *bytes);
/* Any producer: notify AFTER publishing data, GPU completion or closure.
 * No waits, allocations or clock reads. Does not poll a GPU fence itself. */
void record_pyrowave_dispatch_notify(record_pyrowave_dispatch_t *job);
/* Atomic state/error observations may advance during a call. Terminal states
 * require retired I/O loans; GPU/display and unread backend queues remain
 * separately owned. Details is a protocol/backend reason, not an OS errno. */
unsigned record_pyrowave_dispatch_state(const record_pyrowave_dispatch_t *job, unsigned *details);
/* Worker has stopped touching backend/pools. Storage remains borrowed until
 * free: renderer notifications may continue while GPU/display work retires. */
bool record_pyrowave_dispatch_quiesced(const record_pyrowave_dispatch_t *job);
/* One controller; immutable config after CONFIGURING. Sender config is
 * available immediately. Build compatible resources before accept. */
bool record_pyrowave_dispatch_config(const record_pyrowave_dispatch_t *job,
      rpyrowave_stream_config_t *cfg, uint64_t *generation);
bool record_pyrowave_dispatch_accept(record_pyrowave_dispatch_t *job,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm);
bool record_pyrowave_dispatch_reject(record_pyrowave_dispatch_t *job, unsigned reason);
/* Sender: first close each producer on its owner, then request finish.
 * Cancel does not close producer queues on another thread's behalf. */
bool record_pyrowave_dispatch_finish(record_pyrowave_dispatch_t *job);
void record_pyrowave_dispatch_cancel(record_pyrowave_dispatch_t *job);
/* Cold lost/quiesced backend recovery, producers stopped. The worker forgets
 * its video loan; abandon the backend only AFTER dispatcher free succeeds. */
void record_pyrowave_dispatch_abandon_video(record_pyrowave_dispatch_t *job);
/* Nonblocking collection. Refuses an active worker or pending GPU loan.
 * All controller/producer calls must be quiescent before destruction. */
bool record_pyrowave_dispatch_free(record_pyrowave_dispatch_t *job);
RETRO_END_DECLS
#endif
