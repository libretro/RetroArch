/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fifo_queue.h).
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

#ifndef __LIBRETRO_SDK_FIFO_BUFFER_H
#define __LIBRETRO_SDK_FIFO_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

/**
 * Returns the available data in \c buffer for reading.
 *
 * @param buffer <tt>fifo_buffer_t *</tt>. The FIFO queue to check.
 * @return The number of bytes available for reading from \c buffer.
 */
#define FIFO_READ_AVAIL(buffer) fifo_read_avail_of(buffer)

/**
 * Returns the available space in \c buffer for writing.
 *
 * @param buffer <tt>fifo_buffer_t *</tt>. The FIFO queue to check.
 * @return The number of bytes that \c buffer can accept.
 */
#define FIFO_WRITE_AVAIL(buffer) fifo_write_avail_of(buffer)

/**
 * Returns the available data in \c buffer for reading.
 *
 * @param buffer \c fifo_buffer_t. The FIFO queue to check.
 * @return The number of bytes available for reading from \c buffer.
 */
#define FIFO_READ_AVAIL_NONPTR(buffer) fifo_read_avail_of(&(buffer))

/**
 * Returns the available space in \c buffer for writing.
 *
 * @param buffer \c fifo_buffer_t. The FIFO queue to check.
 * @return The number of bytes that \c buffer can accept.
 */
#define FIFO_WRITE_AVAIL_NONPTR(buffer) fifo_write_avail_of(&(buffer))

/** @copydoc fifo_buffer_t */
struct fifo_buffer
{
   uint8_t *buffer;
   size_t size;
   size_t first;
   size_t end;
};

/**
 * A bounded FIFO byte queue implemented as a ring buffer.
 *
 * The caller synchronises it, and every use in the tree does so with
 * a lock. It is not lock-free: the producer's and the consumer's
 * indices sit together, and read with no lock they bounce a cache
 * line between threads. For one producer and one consumer with no
 * lock, retro_spsc_t is the type.
 *
 * The ring is one byte larger than the capacity asked for, and full
 * is one byte short of it - so size is never a power of two, and the
 * wrap is a compare and a subtract rather than a mask. Rounding the
 * allocation up to a power of two would change the capacity a caller
 * asked for; a mask wants a ring with a count instead of a wasted
 * slot, which changes what the availability means, and is a different
 * type.
 */
typedef struct fifo_buffer fifo_buffer_t;

/* The availability, from one load each of first, end and size: the
 * argument is evaluated once, and a reader without the caller's lock
 * sees one snapshot rather than several. Inline, so a driver's
 * write_avail(), which rate control samples every frame, pays a few
 * instructions. */
static INLINE size_t fifo_read_avail_of(const fifo_buffer_t *buffer)
{
   size_t first = buffer->first;
   size_t end   = buffer->end;
   return (end < first) ? end + buffer->size - first : end - first;
}

static INLINE size_t fifo_write_avail_of(const fifo_buffer_t *buffer)
{
   return (buffer->size - 1) - fifo_read_avail_of(buffer);
}

/**
 * Creates a new FIFO queue with \c size bytes of memory.
 * Must be freed with \c fifo_free.
 *
 * @param size The size of the FIFO queue, in bytes.
 * @return The new queue if successful, \c NULL otherwise.
 * @see fifo_initialize
 */
fifo_buffer_t *fifo_new(size_t len);

/**
 * Initializes an existing FIFO queue with \c size bytes of memory.
 *
 * Suitable for use with \c fifo_buffer_t instances
 * of static or automatic lifetime.
 * Must be freed with \c fifo_deinitialize.
 *
 * @param buf Pointer to the FIFO queue to initialize.
 * May be static or automatic.
 * @param size The size of the FIFO queue, in bytes.
 * @return \c true if \c buf was initialized with the requested memory,
 * \c false if \c buf is \c NULL or there was an error.
 */
bool fifo_initialize(fifo_buffer_t *buf, size_t len);

/**
 * Resets the bounds of \c buffer,
 * effectively clearing it.
 *
 * No memory will actually be freed,
 * but the contents of \c buffer will be overwritten
 * with the next call to \c fifo_write.
 * @param buffer The FIFO queue to clear.
 * Behavior is undefined if \c NULL.
 */
static INLINE void fifo_clear(fifo_buffer_t *buffer)
{
   buffer->first = 0;
   buffer->end   = 0;
}

/**
 * Writes \c size bytes to the given queue.
 *
 * @param buffer The FIFO queue to write to.
 * @param in_buf The buffer to read bytes from.
 * @param size The length of \c in_buf, in bytes.
 */
/* Unchecked: len must be at most FIFO_WRITE_AVAIL() for a write and
 * FIFO_READ_AVAIL() for a read, which the caller has established under
 * whatever synchronisation it owns; the ring is never locked here. A
 * length past that is a copy past the ring. The checked calls below
 * clamp and report instead. */
void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t len);

/**
 * Reads \c size bytes from the given queue.
 *
 * @param buffer The FIFO queue to read from.
 * @param in_buf The buffer to store the read bytes in.
 * @param size The length of \c in_buf, in bytes.
 * @post Upon return, \c buffer will have up to \c size more bytes of space available for writing.
 */
void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t len);

/* Checked: copy at most what is available and return how much moved.
 * A length of zero, or nothing available, moves nothing. */
size_t fifo_write_checked(fifo_buffer_t *buffer, const void *in_buf, size_t len);
size_t fifo_read_checked(fifo_buffer_t *buffer, void *in_buf, size_t len);

/**
 * Releases \c buffer and its contents.
 *
 * @param buffer The FIFO queue to free.
 * If \c NULL, this function will do nothing.
 * Behavior is undefined if \c buffer was previously freed.
 * @see fifo_deinitialize
 */
void fifo_free(fifo_buffer_t *buffer);

/**
 * Deallocates the contents of \c buffer,
 * but not \c buffer itself.
 *
 * Suitable for use with static or automatic \c fifo_buffer_t instances.
 *
 * @param buffer The buffer to deinitialize.
 * @return \c false if \c buffer is \c NULL, \c true otherwise.
 * @see fifo_free
 */
bool fifo_deinitialize(fifo_buffer_t *buffer);


RETRO_END_DECLS

#endif
