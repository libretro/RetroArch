/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_COMMON_RPYROWAVE_STREAM_H
#define LIBRETRO_COMMON_RPYROWAVE_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include <formats/rpyrowave.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS
#define RPYROWAVE_STREAM_HEADER_BYTES 64
#define RPYROWAVE_STREAM_CONFIG_BYTES 64
#define RPYROWAVE_STREAM_MAX_PAYLOAD 16384
#define RPYROWAVE_STREAM_FRAGMENT_BYTES 4096
#define RPYROWAVE_STREAM_MAX_FRAME_BYTES 16777216

enum rpyrowave_stream_kind
{
   RPYROWAVE_STREAM_CONFIG = 1,
   RPYROWAVE_STREAM_READY = 2,
   RPYROWAVE_STREAM_VIDEO = 3,
   RPYROWAVE_STREAM_AUDIO = 4,
   RPYROWAVE_STREAM_END = 5,
   RPYROWAVE_STREAM_REJECT = 6
};

enum rpyrowave_stream_event
{
   RPYROWAVE_STREAM_CONFIGURE = 1,
   RPYROWAVE_STREAM_STARTED,
   RPYROWAVE_STREAM_ENDED,
   RPYROWAVE_STREAM_REJECTED,
   RPYROWAVE_STREAM_CANCELLED,
   RPYROWAVE_STREAM_BROKEN
};

enum rpyrowave_stream_error
{
   RPYROWAVE_STREAM_BAD_WIRE = 1,
   RPYROWAVE_STREAM_IO_ERROR,
   RPYROWAVE_STREAM_DISCONNECTED,
   RPYROWAVE_STREAM_SINK_ERROR
};

enum rpyrowave_stream_progress
{
   RPYROWAVE_STREAM_ERROR = -1,
   RPYROWAVE_STREAM_BUSY = 0,
   RPYROWAVE_STREAM_PROGRESS = 1,
   RPYROWAVE_STREAM_FINISHED = 2
};

/* io.read: positive byte count, 0 would-block, -1 error, -2 orderly EOF.
 * io.write: positive byte count, 0 would-block, -1 error. Both must be
 * nonblocking and never exceed the supplied size. Socket ownership stays
 * with the caller. No socket, DNS, threads or clocks are hidden here. */
typedef struct rpyrowave_stream_io
{
   void *user;
   int (*read)(void*, void*, size_t);
   int (*write)(void*, const void*, size_t);
} rpyrowave_stream_io_t;

typedef struct rpyrowave_stream_config
{
   rpyrowave_sequence_t sequence;
   uint32_t maximum_frame_bytes;
   uint32_t fps_numerator, fps_denominator;
   uint32_t audio_rate, audio_channels, audio_channel_mask;
   uint32_t maximum_audio_frames;
   unsigned bit_depth;
} rpyrowave_stream_config_t;

/* Timestamps/durations are microseconds relative to the session's audio
 * origin. Audio serial is the absolute first PCM sample-frame index, starting
 * at zero; video serial is an increasing, nonzero frame ID. Audio is S16LE.
 * A receiver may drop an entire record when its bounded pool is full. */
typedef struct rpyrowave_stream_record
{
   uint64_t generation;
   uint64_t serial;
   uint64_t timestamp;
   uint64_t duration;
   uint32_t bytes;
   unsigned kind;
} rpyrowave_stream_record_t;

typedef struct rpyrowave_stream_callbacks
{
   void *user;
   /* Receiver acquire: 1 lends a writable span, 0 drops this record, -1
    * fails the session; 0/-1 consume no reservation. A successful commit transfers the span downstream;
    * return 1 only on success. Otherwise cancel retires the reservation.
    * Video is acquired once for the complete frame, even with interleaved
    * audio fragments. No payload is copied into an intermediate buffer. */
   int (*acquire)(void*, const rpyrowave_stream_record_t*, void**, size_t*);
   int (*commit)(void*, const rpyrowave_stream_record_t*, void*);
   void (*cancel)(void*, const rpyrowave_stream_record_t*, void*);
   void (*dropped)(void*, const rpyrowave_stream_record_t*);
   /* Sender completion retires a previously accepted lend. sent=1 means
    * all bytes were accepted by io.write; sent=0 means cancellation/error. */
   void (*sent)(void*, const rpyrowave_stream_record_t*, const void*, void *token, int sent);
   /* CONFIGURE borrows the peer configuration; accept/reject it later,
    * after cold GPU/audio setup. Never allocate GPU resources in this pump.
    * Callbacks must not reenter or free the stream. */
   void (*event)(void*, unsigned event, const rpyrowave_stream_config_t*, uint64_t generation, unsigned reason);
} rpyrowave_stream_callbacks_t;

typedef struct rpyrowave_stream rpyrowave_stream_t;

int rpyrowave_stream_config_valid(const rpyrowave_stream_config_t *config);
/* Exact rational conversion with overflow checks; rates are 1..1000000. */
int rpyrowave_stream_time(uint64_t ticks, uint32_t rate_numerator,
      uint32_t rate_denominator, uint64_t *microseconds);

/* Cold construction. All subsequent operations belong to one I/O owner.
 * A receiver must supply acquire/commit/cancel callbacks. New connections
 * use a new nonzero generation and independent stream/decoder instances. */
rpyrowave_stream_t *rpyrowave_stream_sender_new(const rpyrowave_stream_config_t *config,
      uint64_t generation, const rpyrowave_stream_io_t *io, const rpyrowave_stream_callbacks_t *callbacks);
rpyrowave_stream_t *rpyrowave_stream_receiver_new(const rpyrowave_stream_io_t *io,
      const rpyrowave_stream_callbacks_t *callbacks);
/* Receiver after CONFIGURE: reason=0 accepts; a nonzero reason rejects. */
int rpyrowave_stream_accept(rpyrowave_stream_t *stream, unsigned reason);

/* Sender: at most one outstanding video lend and one audio lend. PROGRESS
 * transfers ownership through sent(); BUSY/error consumes nothing. Audio
 * timestamps/durations must agree exactly with its absolute sample index.
 * Forward gaps are signalled automatically. Video may skip frame IDs. */
int rpyrowave_stream_offer(rpyrowave_stream_t *stream, const rpyrowave_stream_record_t *record,
      const void *data, void *token);
/* Each pump makes at most eight read and eight write calls, never waits,
 * allocates or reads a clock. Audio takes precedence at fragment boundaries. */
int rpyrowave_stream_pump(rpyrowave_stream_t *stream);
/* I/O owner: register writable readiness only while this is nonzero. New
 * producer data and receiver accept/reject still need an external wake. */
int rpyrowave_stream_wants_write(const rpyrowave_stream_t *stream);
/* Sender: drain accepted records, then send END. Only valid after STARTED. */
int rpyrowave_stream_finish(rpyrowave_stream_t *stream);
/* Abort releases every active lend/reservation exactly once. The caller
 * closes its socket separately. Free aborts outstanding work if necessary. */
void rpyrowave_stream_abort(rpyrowave_stream_t *stream);
void rpyrowave_stream_free(rpyrowave_stream_t *stream);
RETRO_END_DECLS
#endif
