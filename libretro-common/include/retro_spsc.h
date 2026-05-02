/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_spsc.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_SPSC_H
#define __LIBRETRO_SDK_SPSC_H

/*
 * retro_spsc.h - portable single-producer / single-consumer byte queue
 *
 * Lock-free byte-stream queue with one writer thread and one reader
 * thread.  Uses release-store / acquire-load on two cursors (the
 * Lamport / Disruptor design) rather than a single shared count, so
 * neither thread issues atomic RMW operations on the hot path.
 *
 * Constraints:
 *   - Exactly ONE producer and ONE consumer.  No locking is performed;
 *     two producers or two consumers will corrupt the queue.
 *   - Capacity must be power-of-2.  Init rounds up.
 *   - Maximum capacity is SIZE_MAX/2 (so head - tail never overflows
 *     the well-defined unsigned modular range).
 *   - The buffer is byte-addressed.  Callers pushing structured records
 *     are responsible for framing.
 *
 * Memory model:
 *   - Producer writes data into the buffer (non-atomic stores), then
 *     publishes the new head via release-store.
 *   - Consumer acquire-loads head (sees data writes that preceded the
 *     release), reads data (non-atomic loads), then publishes the new
 *     tail via release-store.
 *   - Producer acquire-loads tail to know how much space is free.
 *   - This pairing gives the SPSC invariant on every backend
 *     retro_atomic.h supports lock-freely.  On the volatile fallback
 *     (no real backend), correctness reduces to single-core or x86 TSO,
 *     same as every other retro_atomic.h caller.
 *
 * Cache behaviour:
 *   - head and tail are placed on separate cache lines via explicit
 *     padding to RETRO_SPSC_CACHE_LINE.  Without this, the producer's
 *     publish would invalidate the consumer's tail line and vice versa,
 *     halving throughput on contended SMP.  The padding is a
 *     performance hint; correctness does not depend on it.
 *
 * Lifetime:
 *   - retro_spsc_init allocates an internal buffer; retro_spsc_free
 *     releases it.  The caller owns the retro_spsc_t struct itself.
 *   - The struct is NOT itself thread-safe to construct or destroy
 *     while in use.  Build it on one thread, hand the producer pointer
 *     to one thread and the consumer pointer to another, then tear
 *     down on one thread after both producer and consumer have stopped.
 *
 * Example:
 *
 *   retro_spsc_t q;
 *   retro_spsc_init(&q, 4096);
 *
 *   // Producer thread:
 *   uint8_t msg[64] = ...;
 *   if (retro_spsc_write_avail(&q) >= sizeof(msg))
 *      retro_spsc_write(&q, msg, sizeof(msg));
 *
 *   // Consumer thread:
 *   uint8_t msg[64];
 *   if (retro_spsc_read_avail(&q) >= sizeof(msg))
 *      retro_spsc_read(&q, msg, sizeof(msg));
 *
 *   retro_spsc_free(&q);
 *
 * Comparison with libretro-common's existing fifo_queue_t:
 *   - fifo_queue uses a slock_t internally; safe with multiple producers
 *     and multiple consumers, but every push/pop takes a mutex.
 *   - retro_spsc is lock-free but limited to one producer and one
 *     consumer.  Use this when (a) the producer/consumer count is fixed
 *     at one each (audio driver -> backend, video -> task system, etc.)
 *     and (b) the lock contention in fifo_queue measurably matters.
 *   - For most code paths, fifo_queue is the better default.  This
 *     primitive is for hot paths where lock-free is a measured win.
 */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>
#include <retro_atomic.h>

RETRO_BEGIN_DECLS

/* Cache-line padding size.  64 covers x86-64, AArch64, most modern
 * ARMs, and modern PowerPC.  Apple Silicon's effective coherency line
 * is 128 due to its M-series cluster topology; over-padding to 64
 * still avoids false sharing, just slightly less efficiently.  Older
 * 32-bit ARMs (ARMv6, Cortex-A8) used 32-byte lines but tolerate the
 * larger pad without correctness impact. */
#ifndef RETRO_SPSC_CACHE_LINE
#define RETRO_SPSC_CACHE_LINE 64
#endif

/* Padding helper.  C89 has no _Static_assert; the array-size trick is
 * portable.  We pad to the next multiple of cache-line size after the
 * pointer-and-size_t prefix and after each cursor. */

/* Effective cache line for padding.  We pad each cursor to a full
 * cache line.  If the pre-cursor fields exceed RETRO_SPSC_CACHE_LINE
 * on some hypothetical small-cache target, the underflow on the
 * subtraction would produce a giant array; guard with a max() so
 * the pad is always at least 1 byte and never underflows. */
#define RETRO_SPSC_PAD0_BYTES \
   ((RETRO_SPSC_CACHE_LINE > (sizeof(uint8_t*) + sizeof(size_t))) \
      ? (RETRO_SPSC_CACHE_LINE - (sizeof(uint8_t*) + sizeof(size_t))) \
      : 1)
#define RETRO_SPSC_PAD1_BYTES \
   ((RETRO_SPSC_CACHE_LINE > sizeof(retro_atomic_size_t)) \
      ? (RETRO_SPSC_CACHE_LINE - sizeof(retro_atomic_size_t)) \
      : 1)

typedef struct retro_spsc
{
   uint8_t            *buffer;
   size_t              capacity;   /* power of 2; mask = capacity - 1 */
   /* Pad so head sits on a fresh cache line, isolating it from
    * the buffer/capacity fields that init may touch. */
   uint8_t             _pad0[RETRO_SPSC_PAD0_BYTES];
   retro_atomic_size_t head;       /* producer publishes; consumer reads */
   /* Pad so tail sits on its own cache line, isolating it from head. */
   uint8_t             _pad1[RETRO_SPSC_PAD1_BYTES];
   retro_atomic_size_t tail;       /* consumer publishes; producer reads */
} retro_spsc_t;

/**
 * retro_spsc_init:
 * @q             : The queue.
 * @min_capacity  : Requested capacity in bytes.  Rounded up to the
 *                  next power of 2.  Must be > 0 and <= SIZE_MAX/2.
 *
 * Allocates the internal buffer and zero-initialises both cursors.
 * Both cursors begin at 0; the queue is empty.
 *
 * Returns: true on success, false on allocation failure or invalid
 * @min_capacity.
 *
 * After this returns true, the producer thread can call
 * retro_spsc_write / retro_spsc_write_avail and the consumer thread can
 * call retro_spsc_read / retro_spsc_read_avail.
 */
bool retro_spsc_init(retro_spsc_t *q, size_t min_capacity);

/**
 * retro_spsc_free:
 * @q : The queue.
 *
 * Releases the internal buffer.  Caller must ensure both producer
 * and consumer have stopped using @q before calling.  Safe to call
 * on a queue that retro_spsc_init failed on (no-op).
 */
void retro_spsc_free(retro_spsc_t *q);

/**
 * retro_spsc_clear:
 * @q : The queue.
 *
 * Resets head and tail to 0, discarding any unread data.  The
 * underlying buffer is preserved (no reallocation).
 *
 * SAFETY: callable only when both the producer and consumer are
 * quiesced -- e.g. before either has started, or after both have
 * stopped.  Concurrent calls with a live producer or consumer are
 * a data race.  This is the same lifetime constraint as
 * retro_spsc_init / retro_spsc_free.
 *
 * Typical use is when a stream is stopped and restarted (e.g. an
 * audio driver pausing and resuming, or switching device formats),
 * and stale buffered data should be discarded before the new
 * stream begins.
 */
void retro_spsc_clear(retro_spsc_t *q);

/**
 * retro_spsc_write_avail:
 * @q : The queue.
 *
 * Producer-side query: how many bytes can be written before the
 * queue is full.
 *
 * Returns: byte count, in [0, capacity].
 *
 * SAFETY: callable only from the producer thread.
 */
size_t retro_spsc_write_avail(const retro_spsc_t *q);

/**
 * retro_spsc_read_avail:
 * @q : The queue.
 *
 * Consumer-side query: how many bytes are available to read.
 *
 * Returns: byte count, in [0, capacity].
 *
 * SAFETY: callable only from the consumer thread.
 */
size_t retro_spsc_read_avail(const retro_spsc_t *q);

/**
 * retro_spsc_write:
 * @q     : The queue.
 * @data  : Source bytes.
 * @bytes : Number of bytes to attempt to write.
 *
 * Writes up to @bytes from @data into the queue.  If the queue has
 * less than @bytes free, writes only what fits.
 *
 * Returns: number of bytes actually written.
 *
 * SAFETY: callable only from the producer thread.  Concurrent calls
 * with another producer corrupt the queue.
 */
size_t retro_spsc_write(retro_spsc_t *q, const void *data, size_t bytes);

/**
 * retro_spsc_read:
 * @q     : The queue.
 * @data  : Destination bytes.
 * @bytes : Number of bytes to attempt to read.
 *
 * Reads up to @bytes from the queue into @data.  If the queue has
 * less than @bytes available, reads only what is present.
 *
 * Returns: number of bytes actually read.
 *
 * SAFETY: callable only from the consumer thread.  Concurrent calls
 * with another consumer corrupt the queue.
 */
size_t retro_spsc_read(retro_spsc_t *q, void *data, size_t bytes);

/**
 * retro_spsc_peek:
 * @q     : The queue.
 * @data  : Destination bytes.
 * @bytes : Number of bytes to peek.
 *
 * Like retro_spsc_read but does not advance the read cursor.  The
 * peeked bytes remain available for the next read.
 *
 * Returns: number of bytes peeked.
 *
 * SAFETY: callable only from the consumer thread.
 */
size_t retro_spsc_peek(const retro_spsc_t *q, void *data, size_t bytes);

RETRO_END_DECLS

#endif /* __LIBRETRO_SDK_SPSC_H */
