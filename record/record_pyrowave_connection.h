/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_CONNECTION_H
#define RECORD_PYROWAVE_CONNECTION_H
#include <net/rpyrowave_socket.h>
#include "record_pyrowave_session.h"
RETRO_BEGIN_DECLS
typedef struct record_pyrowave_connection record_pyrowave_connection_t;
/* Cold setup after network initialization and resolution. cfg selects sender;
 * NULL selects receiver. listen selects one-peer listen rather than connect,
 * independently of sender/receiver role. Resources are borrowed. All session
 * and endpoint storage is allocated here, before readiness scheduling. */
record_pyrowave_connection_t *record_pyrowave_connection_new(const void *address, size_t address_bytes,
      bool listen, const rpyrowave_stream_config_t *cfg, uint64_t generation,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm);
/* One I/O owner. Unregister the previous handle BEFORE step/abort/free, then
 * re-register handle/interest if nonzero. Accept/connect can replace or close
 * a handle. step(0) also handles producer/GPU wakes and immediate connects.
 * No allocation, clock, wait or thread creation occurs in these methods. */
uintptr_t record_pyrowave_connection_handle(const record_pyrowave_connection_t *connection);
unsigned record_pyrowave_connection_interest(const record_pyrowave_connection_t *connection);
bool record_pyrowave_connection_listening(const record_pyrowave_connection_t *connection);
int record_pyrowave_connection_step(record_pyrowave_connection_t *connection, unsigned readiness);
/* Borrowed I/O-owner access for status, cold accept/reject, finish and cold
 * video abandonment. Do not independently pump/free this owned session. */
record_pyrowave_session_t *record_pyrowave_connection_session(record_pyrowave_connection_t *connection);
void record_pyrowave_connection_abort(record_pyrowave_connection_t *connection);
/* Refuses a GPU-pending session loan; keep servicing render completion and
 * step(0), or use explicit cold backend abandonment, before retrying. */
bool record_pyrowave_connection_free(record_pyrowave_connection_t *connection);
RETRO_END_DECLS
#endif
