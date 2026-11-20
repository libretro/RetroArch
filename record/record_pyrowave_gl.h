/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_GL_H
#define RECORD_PYROWAVE_GL_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/rpyrowave_gpu.h>
#include "record_gpu_queue.h"
#include "record_pyrowave.h"
#include "record_pyrowave_session.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_gl record_pyrowave_gl_t;
void record_pyrowave_gl_session_video(record_pyrowave_gl_t *codec, record_pyrowave_video_t *video);
typedef void (*record_pyrowave_gl_proc_t)(void);

typedef struct record_pyrowave_gl_config
{
   record_pyrowave_gl_proc_t (*get_proc)(const char *name);
   rpyrowave_sequence_t sequence;
   uint32_t maximum_bytes;
   unsigned programs[3]; /* Color, DWT, packet; borrowed for this session. */
   unsigned slots;
   unsigned bit_depth;
   unsigned texture_mode;
   unsigned texture_width, texture_height;
   unsigned x, y;
   bool flip_y;
   bool decode;
   bool scale_source;
} record_pyrowave_gl_config_t;

typedef struct record_pyrowave_gl_image
{
   record_gpu_frame_t frame;
   void *ready_sync;
   unsigned texture;
} record_pyrowave_gl_image_t;

/* Decode mode: I/O producer writes compressed bytes directly into this span.
 * Submit indexes headers in place; failure retains the reservation. No GL
 * calls on this side. finish is also called by this producer in decode mode. */
int record_pyrowave_gl_receive_acquire(record_pyrowave_gl_t *codec,
      void **data, size_t *capacity);
int record_pyrowave_gl_receive_submit(record_pyrowave_gl_t *codec,
      size_t bytes, uint64_t timestamp, uint64_t duration);
bool record_pyrowave_gl_receive_cancel(record_pyrowave_gl_t *codec);
/* Context owner: append one decode, or drop one queued compressed frame.
 * END only closes input; GPU/image/display borrows must still retire. */
int record_pyrowave_gl_decode(record_pyrowave_gl_t *codec);
int record_pyrowave_gl_receive_drop(record_pyrowave_gl_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_gl_receive_peek(record_pyrowave_gl_t *codec, record_gpu_frame_t *frame);
/* poll publishes completed, validated RGBA32F textures in texture_mode.
 * Views must not perform an additional automatic sRGB conversion.
 * Each image stays borrowed until drop or presentation retires. ready_sync is
 * borrowed until drop/present; a shared-context display queues a server wait
 * on it before sampling and rebinds the texture in that context. */
int record_pyrowave_gl_image(record_pyrowave_gl_t *codec, record_pyrowave_gl_image_t *image);
/* Drop only without outstanding display use; otherwise present after the
 * final draw. A drop immediately returns the texture to the producer pool. */
bool record_pyrowave_gl_image_drop(record_pyrowave_gl_t *codec, unsigned slot);
/* After same-context display commands, pass NULL. A cross-context display
 * supplies its flushed, borrowed GLsync; the server waits, never the CPU.
 * poll returns the slot only after this final display use completes. */
bool record_pyrowave_gl_present(record_pyrowave_gl_t *codec, unsigned slot, void *display_sync);

/* Cold setup on the owning GL context. Requires compute plus persistent
 * coherent read mapping (desktop ARB_buffer_storage or GLES EXT equivalent).
 * Programs use the binding layout of the independent codec shaders.
 * Hot methods run on this context's thread except encode take/packet/release
 * and decode receive_acquire/submit/cancel/finish, owned by the I/O side.
 * GL bindings are clobbered; the renderer must rebind before drawing.
 * GPU pixels stay resident; only compressed capacity/control are staged. */
record_pyrowave_gl_t *record_pyrowave_gl_new(const record_pyrowave_gl_config_t *config);

/* Commands reading texture are placed on this context's queue. Keep a
 * cross-context producer from rewriting it until source_retired >= ticket.
 * Same-context later writes are ordered naturally. ready_sync, if supplied,
 * must be flushed by its producer; it is borrowed while queuing a server-side
 * wait. No CPU waits or allocation.
 * BUSY consumes nothing. On OK ticket is also frame.ready_value. An ERROR
 * with a changed ticket may have submitted work; retire that source through
 * poll or context destruction before its producer can reuse it. */
int record_pyrowave_gl_push(record_pyrowave_gl_t *capture, unsigned texture,
      void *ready_sync, uint64_t timestamp, uint64_t duration, uint64_t *ticket);
/* Polls each outstanding fence once, without waiting. Publishes immutable
 * mapped spans to the consumer; source retirement is producer-thread only. */
bool record_pyrowave_gl_poll(record_pyrowave_gl_t *capture);
uint64_t record_pyrowave_gl_source_retired(const record_pyrowave_gl_t *capture);
/* Context owner: all GPU/display fence loans retired; packets are separate. */
bool record_pyrowave_gl_idle(const record_pyrowave_gl_t *capture);
bool record_pyrowave_gl_finish(record_pyrowave_gl_t *capture);

/* Single consumer. A taken frame stays owned through BUSY and until packet
 * transmission finishes. packet lends mapped compressed storage until release.
 * ERROR packets must also be released. No GL calls on this side. */
int record_pyrowave_gl_take(record_pyrowave_gl_t *capture, record_gpu_frame_t *frame);
int record_pyrowave_gl_packet(record_pyrowave_gl_t *capture, unsigned slot,
      record_pyrowave_packet_t *packet);
bool record_pyrowave_gl_release(record_pyrowave_gl_t *capture, unsigned slot);

/* Cold controller after the consumer quiesces. Refuses unfinished GPU work
 * and queued/borrowed packets; retry after poll/drain. NULL succeeds. */
bool record_pyrowave_gl_free(record_pyrowave_gl_t *capture);
/* Only after the GL context was destroyed/lost and the consumer quiesced.
 * Discards pending GPU references and packets without making any GL calls.
 * The quiesced consumer must no longer access any lent packet pointer. */
bool record_pyrowave_gl_abandon(record_pyrowave_gl_t *capture);
/* Cold healthy-context recovery after both owners quiesce. Inserts and polls
 * an ordered GPU marker without waiting, then discards all loans and frees.
 * Retry false on the same context. No borrowed packet may still be accessed. */
bool record_pyrowave_gl_retire(record_pyrowave_gl_t *capture);
/* Cold first phase: stop renderer calls, then prove all native work retired.
 * Packet loans remain intact until the consumer quiesces and retire frees. */
bool record_pyrowave_gl_quiesce(record_pyrowave_gl_t *capture);
RETRO_END_DECLS
#endif
