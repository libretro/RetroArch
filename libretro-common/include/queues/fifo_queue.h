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
#define FIFO_READ_AVAIL(buffer) (((buffer)->end + (((buffer)->end < (buffer)->first) ? (buffer)->size : 0)) - (buffer)->first)

/**
 * Returns the available space in \c buffer for writing.
 *
 * @param buffer <tt>fifo_buffer_t *</tt>. The FIFO queue to check.
 * @return The number of bytes that \c buffer can accept.
 */
#define FIFO_WRITE_AVAIL(buffer) (((buffer)->size - 1) - (((buffer)->end + (((buffer)->end < (buffer)->first) ? (buffer)->size : 0)) - (buffer)->first))

/**
 * Returns the available data in \c buffer for reading.
 *
 * @param buffer \c fifo_buffer_t. The FIFO queue to check.
 * @return The number of bytes available for reading from \c buffer.
 */
#define FIFO_READ_AVAIL_NONPTR(buffer) (((buffer).end + (((buffer).end < (buffer).first) ? (buffer).size : 0)) - (buffer).first)

/**
 * Returns the available space in \c buffer for writing.
 *
 * @param buffer \c fifo_buffer_t. The FIFO queue to check.
 * @return The number of bytes that \c buffer can accept.
 */
#define FIFO_WRITE_AVAIL_NONPTR(buffer) (((buffer).size - 1) - (((buffer).end + (((buffer).end < (buffer).first) ? (buffer).size : 0)) - (buffer).first))

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
 * Useful for communication between threads,
 * although the caller is responsible for synchronization.
 */
typedef struct fifo_buffer fifo_buffer_t;

/**
 * Creates a new FIFO queue with \c size bytes of memory.
 * Must be freed with \c fifo_free.
 *
 * @param size The size of the FIFO queue, in bytes.
 * @return The new queue if successful, \c NULL otherwise.
 * @see fifo_initialize
 */
fifo_buffer_t *fifo_new(size_t size);

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
bool fifo_initialize(fifo_buffer_t *buf, size_t size);

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
void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size);

/**
 * Reads \c size bytes from the given queue.
 *
 * @param buffer The FIFO queue to read from.
 * @param in_buf The buffer to store the read bytes in.
 * @param size The length of \c in_buf, in bytes.
 * @post Upon return, \c buffer will have up to \c size more bytes of space available for writing.
 */
void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size);

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
