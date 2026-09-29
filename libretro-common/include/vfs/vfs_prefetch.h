/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_prefetch.h).
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

#ifndef __LIBRETRO_SDK_VFS_PREFETCH_H
#define __LIBRETRO_SDK_VFS_PREFETCH_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A background prefetcher for a file read over a network: a thread of
 * its own fetches the windows ahead of the reader on a connection of
 * its own, so a run of sequential reads is served from memory and the
 * reading thread never waits for the network. The reader's side is
 * lock-free: a read copies from a ready window or reports a miss (and
 * the caller reads the network itself, as it would without this), and
 * a seek out of the fetched range retargets the thread.
 *
 * Only built with HAVE_THREADS; without it vfs_prefetch_new() returns
 * NULL and everything else is a no-op, so callers keep one code path. */

struct vfs_prefetch;

/* Read @len octets at @off into @buf; the octets read (short at end of
 * file), -1 on failure. Runs on the prefetch thread, on that thread's
 * own connection. */
typedef int64_t (*vfs_prefetch_fetch_t)(void *user, uint64_t off, uint8_t *buf, size_t len);

/**
 * vfs_prefetch_new:
 * @window   : octets per window (a client's read-ahead size fits)
 * @slots    : windows kept ahead, 2 or more; memory is window x slots
 * @start    : the offset to begin at (0 for a fresh file)
 *
 * Starts the thread. NULL when threads are unavailable or memory is.
 **/
struct vfs_prefetch *vfs_prefetch_new(vfs_prefetch_fetch_t fetch, void *user,
      size_t window, unsigned slots, uint64_t start);

/**
 * vfs_prefetch_read:
 *
 * Copy up to @len octets at @off from the fetched windows. Returns the
 * octets copied (fewer than @len when the range runs past what is
 * ready, 0 when a ready window says the file ends there), or -1 when
 * nothing at @off is ready: the caller then reads the network itself.
 * Consumed windows are recycled as the reader moves past them.
 **/
int64_t vfs_prefetch_read(struct vfs_prefetch *p, uint64_t off, uint8_t *buf, size_t len);

/**
 * vfs_prefetch_seek:
 *
 * The reader will continue at @off: when that is outside what is
 * fetched or being fetched, the thread starts over there.
 **/
void vfs_prefetch_seek(struct vfs_prefetch *p, uint64_t off);

/**
 * vfs_prefetch_free:
 *
 * Stops the thread (waiting for a fetch in flight to end) and frees
 * everything. @user is the caller's to release afterwards.
 **/
void vfs_prefetch_free(struct vfs_prefetch *p);

RETRO_END_DECLS

#endif
