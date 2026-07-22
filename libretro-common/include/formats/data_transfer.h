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


/* A growable buffer on the same reservation machinery as the prefix
 * reader: init reserves 'ceiling' bytes of address space (physical
 * pages only as ensure() commits them), so growth never copies, the
 * base never moves, and the buffer's cost is what it holds - no
 * doubling spikes, no over-allocation tail.  Where the platform
 * cannot reserve (or ceiling is 0), it degrades to realloc doubling,
 * and base may move across ensure(); callers must re-read base after
 * every ensure either way.  release() is idempotent. */
typedef struct data_transfer_arena
{
   uint8_t *base;
   size_t   committed;  /* bytes usable at base                     */
   size_t   cap;        /* reservation ceiling / current allocation */
   uint8_t  reserved;   /* 1: address-space reservation, stable base */
} data_transfer_arena_t;

bool data_transfer_arena_init(data_transfer_arena_t *a, size_t ceiling);
bool data_transfer_arena_ensure(data_transfer_arena_t *a, size_t need);
void data_transfer_arena_release(data_transfer_arena_t *a);

/* Open a file of any size for prefix reading: address space for the
 * whole file is reserved up front, but physical memory is committed
 * only as far as the fill actually reaches - the caller's iterate
 * budget, not the file's size, decides the footprint.  A consumer
 * that needs 2% of a 4 GB file to decode a still costs 2%, and the
 * uncommitted tail is hardware-protected, so a read past avail()
 * faults immediately instead of returning garbage.
 *
 * commit_cap (0 = none) is a hard ceiling on committed bytes: a
 * transfer that reaches it stops there and reports capped() - a
 * distinct terminal from complete (the whole file arrived) and
 * failed (the read ended short inside the committed region).
 * complete() keeps its whole-file meaning for every consumer.
 *
 * On platforms without address-space reservation the buffer degrades
 * to a plain allocation of min(len, commit_cap) (or a built-in
 * window when commit_cap is 0), so callers there should treat the
 * cap as advisory sizing. */
data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap);

/* The fill stopped at commit_cap with the file continuing beyond it.
 * Terminal: iterate() will not advance further. */
bool data_transfer_capped(data_transfer_t *dt);

/* Bounded-memory streaming over a prefix transfer: release the
 * physical backing of the leading bytes below 'up_to' (rounded down
 * to a page).  avail() is unchanged, but the released bytes are GONE
 * - a caller that might read them again must data_transfer_refill()
 * first, and the discard contract is a promise not to touch them
 * meanwhile.  A consumer that discards behind its read position as
 * it goes plays a file of any size in a constant residency window.
 * Best-effort: a no-op on the nbio strategy and on the no-reservation
 * fallback (whose bytes simply stay). */
void data_transfer_discard(data_transfer_t *dt, size_t up_to);

/* Make [from, avail) readable again after discards, re-reading any
 * released span from the file (the transfer keeps it open).  The cost
 * of a backward seek, paid honestly: false only if the re-read fails
 * (the file shrank or errored), in which case the transfer settles
 * failed. */
bool data_transfer_refill(data_transfer_t *dt, size_t from);


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
