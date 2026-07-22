/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_DATA_TRANSFER_H__
#define __LIBRETRO_SDK_FORMAT_DATA_TRANSFER_H__

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A growing, stable-pointer view of a file being read: the source-side
 * companion to the image_transfer/audio_transfer decode facades, for
 * consumers that decode against a partially-read buffer (the video
 * still decoders via image_transfer_set_avail, the partial-read
 * demuxers via rwebm_set_avail/rmp4_set_avail, or any other
 * incremental parser).
 *
 * A thin wrapper over nbio that packages its sharp edges into an
 * unmissable shape rather than a documented one: the buffer pointer is
 * valid and stable from open (nbio sizes or maps it up front); filling
 * happens in caller-budgeted steps; a read that ends short of the file
 * (I/O error, the file shrank) is reported as failure with an honest
 * byte count, never as completion; and free cancels any in-flight
 * operation first, which on the stdio backend would otherwise abort.
 *
 * Each of those edges cost a real bug in the thumbnail partial-read
 * work before nbio was fixed to expose them safely; this interface
 * exists so the next consumer cannot re-hit them. */

typedef struct data_transfer data_transfer_t;

/* Open a file for incremental reading.  Returns NULL if it cannot be
 * opened or its buffer cannot be sized.  No bytes are read yet. */
data_transfer_t *data_transfer_open(const char *path);

/* Read up to roughly max_bytes more of the file (rounded up to the
 * backend's chunk; 0 means no budget - fill to the end).  Returns the
 * bytes now valid at the front of the buffer.  Cheap once the fill is
 * over. */
size_t data_transfer_iterate(data_transfer_t *dt, size_t max_bytes);

/* The buffer.  Valid and stable from open; its leading
 * data_transfer_avail() bytes are file content, the rest not yet.
 * *len receives the full file length. */
const uint8_t *data_transfer_ptr(data_transfer_t *dt, size_t *len);

/* Bytes valid at the front of the buffer.  Monotonic. */
size_t data_transfer_avail(data_transfer_t *dt);

/* The whole file arrived. */
bool data_transfer_complete(data_transfer_t *dt);

/* The read ended short of the file (I/O error, the file shrank
 * underneath the fill).  avail() is frozen at the bytes actually
 * delivered; complete() never becomes true. */
bool data_transfer_failed(data_transfer_t *dt);

/* Close, cancelling any in-flight read.  NULL-safe. */
void data_transfer_free(data_transfer_t *dt);

RETRO_END_DECLS

#endif
