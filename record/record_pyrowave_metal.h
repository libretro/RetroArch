/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_METAL_H
#define RECORD_PYROWAVE_METAL_H
#include <boolean.h>
#include <formats/rpyrowave_gpu.h>
#include "record_gpu_queue.h"
#include "record_pyrowave.h"
#include "record_pyrowave_session.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_metal record_pyrowave_metal_t;
void record_pyrowave_metal_session_video(record_pyrowave_metal_t *codec, record_pyrowave_video_t *video);

typedef struct record_pyrowave_metal_config
{
   void *device;
   void *queue;
   const char *shaders[3]; /* Independent MSL: color, DWT, packet. */
   rpyrowave_sequence_t sequence;
   uint32_t maximum_bytes;
   unsigned slots;
   unsigned bit_depth;
   unsigned texture_mode;
   unsigned texture_width, texture_height;
   unsigned x, y;
   bool flip_y;
   bool decode;
} record_pyrowave_metal_config_t;

typedef struct record_pyrowave_metal_image
{
   record_gpu_frame_t frame;
   void *texture;
} record_pyrowave_metal_image_t;

/* Cold setup. Device and queue are retained; shader text is needed only
 * during this call. All buffers, textures and constants are preallocated.
 * texture_mode selects code values or linear BT.709 scRGB (1 = 80 nits).
 * Gamut/transfer conversion is fused into the color kernel. Texture views
 * must not apply another automatic sRGB conversion.
 * Metal 2.1 and tracked resource hazards are required. No Objective-C source
 * or Apple SDK types enter the C89 host/codec interface. */
record_pyrowave_metal_t *record_pyrowave_metal_new(const record_pyrowave_metal_config_t *config);

/* Encoder producer, renderer thread. Commands must belong to the configured
 * queue, retain resources and have no active encoder. Append commands here,
 * then commit exactly once, in push order. The caller owns submission.
 * BUSY consumes nothing. A changed ticket on ERROR still borrows commands
 * and source contents until completion/discard. Source readiness, including
 * untracked resources or cross-queue production, must already be encoded on
 * commands. No CPU waits, pixels or allocations. */
int record_pyrowave_metal_push(record_pyrowave_metal_t *codec, void *commands,
      void *source, uint64_t timestamp, uint64_t duration, uint64_t *ticket);
/* Encoder consumer, one I/O thread. Packet storage is lent until release. */
int record_pyrowave_metal_take(record_pyrowave_metal_t *codec, record_gpu_frame_t *frame);
int record_pyrowave_metal_packet(record_pyrowave_metal_t *codec, unsigned slot,
      record_pyrowave_packet_t *packet);
bool record_pyrowave_metal_release(record_pyrowave_metal_t *codec, unsigned slot);
uint64_t record_pyrowave_metal_source_retired(const record_pyrowave_metal_t *codec);

/* Decoder producer, one I/O thread. Receive directly into the returned
 * compressed span. A reservation remains the producer's after invalid input;
 * cancel or overwrite it. Submit indexes compressed headers in place. */
int record_pyrowave_metal_receive_acquire(record_pyrowave_metal_t *codec,
      unsigned *slot, void **data, size_t *capacity);
int record_pyrowave_metal_receive_submit(record_pyrowave_metal_t *codec,
      size_t bytes, uint64_t timestamp, uint64_t duration);
bool record_pyrowave_metal_receive_cancel(record_pyrowave_metal_t *codec);
/* Renderer consumer can drop a queued, not-yet-decoded packet for pacing
 * or shutdown. This never touches a packet already owned by GPU work. */
int record_pyrowave_metal_receive_drop(record_pyrowave_metal_t *codec, record_gpu_frame_t *frame);
/* Decoder consumer, renderer thread. Same commands/submission rules as push.
 * Decode takes one queued packet and records GPU unpack, inverse DWT and RGB
 * reconstruction. poll later makes a completed image available. */
int record_pyrowave_metal_decode(record_pyrowave_metal_t *codec, void *commands);
int record_pyrowave_metal_image(record_pyrowave_metal_t *codec,
      record_pyrowave_metal_image_t *image);
/* Retire the lent texture after its display commands complete; NULL drops
 * it without display. The caller must commit display commands exactly once.
 * The input/output slot stays owned until this final use retires. */
bool record_pyrowave_metal_present(record_pyrowave_metal_t *codec, unsigned slot, void *commands);

/* Renderer thread, bounded progress without waits. Source retirement is read
 * only on that thread. Errors publish failed packets or drop failed images. */
bool record_pyrowave_metal_poll(record_pyrowave_metal_t *codec);
/* Producer closes its queue. EOS does not retire outstanding GPU/image lends. */
bool record_pyrowave_metal_finish(record_pyrowave_metal_t *codec);
/* Renderer/controller: caller discarded this never-committed buffer and
 * guarantees it can never be committed. Retires its source/image references. */
bool record_pyrowave_metal_discard(record_pyrowave_metal_t *codec, void *commands);
/* Cold, both sides quiesced. Refuses outstanding GPU work or lends. */
bool record_pyrowave_metal_free(record_pyrowave_metal_t *codec);
RETRO_END_DECLS
#endif
