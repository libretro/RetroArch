/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_SESSION_H
#define RECORD_PYROWAVE_SESSION_H
#include <formats/rpyrowave_stream.h>
#include "record_pyrowave.h"
#include "record_pyrowave_pcm.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_session record_pyrowave_session_t;

typedef struct record_pyrowave_video
{
   void *user;
   int (*take)(void*, record_gpu_frame_t*);
   int (*packet)(void*, unsigned, record_pyrowave_packet_t*);
   bool (*release)(void*, unsigned);
   int (*acquire)(void*, void**, size_t*);
   int (*submit)(void*, size_t, uint64_t, uint64_t);
   bool (*cancel)(void*);
   bool (*finish)(void*);
} record_pyrowave_video_t;

enum record_pyrowave_session_state
{
   RECORD_PYROWAVE_NEGOTIATING,
   RECORD_PYROWAVE_CONFIGURING,
   RECORD_PYROWAVE_ACTIVE,
   RECORD_PYROWAVE_DRAINING,
   RECORD_PYROWAVE_ENDED,
   RECORD_PYROWAVE_CANCELLED,
   RECORD_PYROWAVE_REJECTED,
   RECORD_PYROWAVE_FAILED
};

enum record_pyrowave_session_error
{
   RECORD_PYROWAVE_VIDEO_ERROR = 256,
   RECORD_PYROWAVE_PCM_ERROR
};

typedef struct record_pyrowave_session_status
{
   rpyrowave_stream_config_t config;
   uint64_t generation;
   uint64_t video_sent, audio_sent, video_received, audio_received;
   uint64_t video_dropped, audio_dropped;
   unsigned state, reason;
} record_pyrowave_session_status_t;

/* Cold construction. Pools, backend and socket are borrowed, never destroyed
 * here. Each instance and all its methods belong to one I/O owner. Publish
 * frontend status through a separate handoff, not concurrent getters.
 * Producer and renderer threads own their backend GPU/PCM methods as usual. */
record_pyrowave_session_t *record_pyrowave_session_sender_new(
      const rpyrowave_stream_config_t *config, uint64_t generation,
      const rpyrowave_stream_io_t *io, const record_pyrowave_video_t *video,
      record_pyrowave_pcm_t *pcm);
record_pyrowave_session_t *record_pyrowave_session_receiver_new(const rpyrowave_stream_io_t *io);
/* CONFIGURING exposes the proposed config for cold GPU/audio setup. Attach
 * compatible pools before accept. No GPU allocation happens in pump. A new
 * connection always needs a new instance, pools and nonzero generation. */
bool record_pyrowave_session_accept(record_pyrowave_session_t *session,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm);
bool record_pyrowave_session_reject(record_pyrowave_session_t *session, unsigned reason);
void record_pyrowave_session_status(const record_pyrowave_session_t *session,
      record_pyrowave_session_status_t *status);
/* Bounded nonblocking transport and loan progress; no allocation/clock/wait.
 * Poll the GPU adapter on its render owner separately. A terminal session
 * still needs pump while a taken GPU packet is BUSY, before free can succeed. */
int record_pyrowave_session_pump(record_pyrowave_session_t *session);
bool record_pyrowave_session_wants_write(const record_pyrowave_session_t *session);
/* Sender: after requesting producer shutdown on its respective owners.
 * Drains video and PCM through queue END, then sends transport END. */
bool record_pyrowave_session_finish(record_pyrowave_session_t *session);
void record_pyrowave_session_abort(record_pyrowave_session_t *session);
/* Cold recovery only: terminal transport, both owners stopped, GPU work
 * lost or quiesced, backend subsequently abandoned rather than reused.
 * Forgets its unoffered video loan so the backend can retire it on abandon. */
bool record_pyrowave_session_abandon_video(record_pyrowave_session_t *session);
/* Refuses pending packet/PCM loans. Does not drain or destroy backend pools;
 * callers retire GPU/display work and remaining queue entries separately. */
bool record_pyrowave_session_free(record_pyrowave_session_t *session);
RETRO_END_DECLS
#endif
