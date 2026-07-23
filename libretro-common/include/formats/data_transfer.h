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

/* Cyclic streaming window over a file, for sequential consumers that
 * loop (background music).  The whole file gets an address-space
 * reservation so the consumer sees one stable base of the full
 * length, but physical pages exist only for [0, keep) - the head,
 * kept forever because loops land there - plus a moving window
 * [lo, hi) that the feeder advances behind the consumer and extends
 * ahead of it.  rewind() decommits the old window after the consumer
 * loops; extend() re-reads from the file, so a second lap costs I/O,
 * not memory.  Compile with DT_WINDOW_STRICT to make advanced-past
 * pages fault on touch instead of reading as zeros - the oracle's
 * proof that a consumer honours the sequential contract.  Platforms
 * without reservations fall back to holding the whole file.
 *
 * SINGLE OWNER, ONE THREAD.  A windowed transfer serves exactly one
 * consumer, and every call on it - feed/advance/extend/rewind and
 * every read of window_base() - belongs to that one owner.  This is a
 * hard contract, not a convention: sharing one window between two
 * consumers (the obvious way to serve audio and video from a single
 * transfer) was measured and does not work.
 *
 * The reason is page lifetime, which no amount of refcounting fixes.
 * feed() advances the window behind the consumer, and advancing
 * decommits: MADV_DONTNEED, or PROT_NONE under DT_WINDOW_STRICT.  A
 * second consumer trailing the first - audio behind video, the normal
 * case - reads pages the feeder is releasing underneath it.  Keeping
 * the struct alive keeps the struct alive; the pages still go.  Under
 * STRICT that faults.  Without it the read silently returns zeros,
 * which is worse: measured at 8-58 MB of zero-filled bytes per run,
 * no crash, decoders consuming them as if they were file content.
 *
 * There is deliberately no accessor for the committed frontier (whi).
 * A second consumer would need it to know which bytes are readable at
 * all, and exposing it would make sharing look supportable when the
 * decommit hazard above still stands.  Reading past the frontier hits
 * reserved PROT_NONE address space and dies immediately - which is
 * the intended outcome for code that should not be there.
 *
 * Two consumers wanting the same bytes should each hold their own
 * transfer, or one should copy what the other needs.  The copy is
 * cheap; sharing is a concurrency redesign whose failure mode is
 * silent corruption. */
/* A producer-backed transfer: the bytes come from a callback instead
 * of a file - an archive entry inflating as it is pulled, or any
 * other decoder producing sequential output.  The total length must
 * be known up front (a ZIP central directory carries it), so the
 * buffer is one exact malloc: plain memory the consumer can adopt
 * and free(), with no reservation machinery - a loaded ROM lives for
 * the whole session, so there is nothing to discard.  The callback
 * writes up to the room remaining at dst and returns bytes produced
 * (n is a pacing hint - producers with chunked granularity may
 * overshoot it; dst always has room to the declared end), 0 at end
 * of stream, negative on error.  iterate()'s byte budget and the
 * completion/ownership surface behave exactly as for files, which is
 * the point: a decoding producer becomes tick-sliceable the same way
 * a raw read is. */
typedef int64_t (*data_transfer_source_read_t)(void *ud, uint8_t *dst,
      size_t n);
data_transfer_t *data_transfer_open_source(size_t len,
      data_transfer_source_read_t read_cb, void *ud);
/* Detach the filled buffer from a source-mode transfer: returns the
 * exact-size malloc'd data (caller frees) and frees the transfer.
 * NULL unless the transfer completed successfully. */
uint8_t *data_transfer_source_detach(data_transfer_t *dt, size_t *len);

data_transfer_t *data_transfer_open_window(const char *path, size_t keep);
const uint8_t *data_transfer_window_base(data_transfer_t *dt, size_t *len);
/* True when the transfer is genuinely windowed - address space was
 * reserved and only the window is committed.  False when the platform
 * has no reservation and data_transfer_open_window degraded to holding
 * the whole file, so a caller can charge admission against the window
 * in the first case and against the file in the second. */
bool data_transfer_window_is_reserved(data_transfer_t *dt);
/* True when this build can reserve address space, i.e. when
 * data_transfer_open_window will map a window rather than degrading to
 * reading the whole file.  Lets a caller decide before opening whether
 * the open is cheap (head only) or is going to slurp - the capability
 * macro itself is private to data_transfer.c, so testing it outside
 * would silently take the wrong branch. */
bool data_transfer_reserve_supported(void);
bool data_transfer_window_extend(data_transfer_t *dt, size_t hi);
void data_transfer_window_advance(data_transfer_t *dt, size_t lo);
void data_transfer_window_rewind(data_transfer_t *dt);
/* Raise the permanently-resident head.  For codecs whose loop
 * landing sits past metadata of unpredictable size (FLAC PICTURE
 * blocks can push the first frame beyond any fixed head), the
 * feeder grows the head to cover the decoder's start position once
 * it is known - the loop must land on resident pages, because the
 * jump happens on the audio thread before any feeder tick. */
bool data_transfer_window_grow_keep(data_transfer_t *dt, size_t keep);
/* Positioned read of file bytes into a caller buffer, touching no
 * window state and committing no pages - for walking container
 * metadata (FLAC block headers) to learn the layout before deciding
 * what the head must cover. */
bool data_transfer_window_peek(data_transfer_t *dt, size_t off,
      void *dst, size_t n);
/* Release an inert range inside the head - bytes the consumer will
 * never revisit (a skipped PICTURE block between the stream info
 * and the first frame).  Whole pages strictly inside (from, to)
 * are decommitted; under DT_WINDOW_STRICT they fault on touch. */
void data_transfer_window_punch(data_transfer_t *dt, size_t from,
      size_t to);
/* One-call feeder policy: keep [tell - margin, tell + lookahead)
 * resident, detecting a backwards tell as a loop.  Returns false on
 * an I/O failure (the consumer will hit the end-of-data wall). */
bool data_transfer_window_feed(data_transfer_t *dt, size_t tell,
      size_t lookahead, size_t margin);

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
