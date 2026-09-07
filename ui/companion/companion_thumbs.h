/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __COMPANION_THUMBS_H
#define __COMPANION_THUMBS_H

/* Shared thumbnail engine for the desktop companions (C89, no toolkit
 * types). One instance per companion window.
 *
 * What the Qt companion does, made common and faster:
 *   - a cache of decoded thumbnails keyed by (file path, edge), already
 *     scaled to the size the view draws, under a byte budget with LRU
 *     eviction: a thumbnail decoded once is drawn from the cache for as
 *     long as it fits, across playlist switches
 *   - a pool of decode threads (Qt: one) that decode and scale off the
 *     UI thread; the UI thread only ever copies finished pixels into a
 *     toolkit image
 *   - a request queue served most-recent-first (the rows still on
 *     screen), de-duplicated by key, with a low-priority lane for
 *     prefetching what is about to scroll in
 *
 * The backend's job is only to say which rows are visible (the view
 * knows; Qt asks its grid for visibleIndexes() on a 50 ms scroll
 * debounce), turn a row into a file path, and poll once per frame for
 * finished thumbnails to hand to its image control.
 *
 * Pixels are ARGB8888 (0xAARRGGBB, opaque), w x h, row-major -
 * Windows' 32-bit DIB order, and what Cocoa/Qt take with a byte swap.
 *
 * Thread-safe where stated; everything else is UI-thread only. Builds
 * without HAVE_THREADS, decoding on the UI thread inside poll() under a
 * time budget. */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct companion_thumbs companion_thumbs_t;

/* Delivered from companion_thumbs_poll() for each thumbnail that has
 * finished decoding since the last poll. @tag is what the request
 * passed (a backend row id); @bits is valid for the duration of the
 * callback only - copy it into the toolkit image there. @bits is NULL
 * when the file could not be decoded (the backend marks the row as
 * having no thumbnail). */
typedef void (*companion_thumbs_done_cb)(void *ud, const char *path,
      int w, int h, uintptr_t tag, const uint32_t *bits);

/* @budget_bytes: cache size in bytes of decoded pixels (0 = a default of
 * 64 MiB). @threads: decode threads to start, 0 = cores - 1 clamped to
 * [1, 4]; ignored without HAVE_THREADS. */
companion_thumbs_t *companion_thumbs_new(size_t budget_bytes,
      unsigned threads);
void companion_thumbs_free(companion_thumbs_t *t);

/* Cached thumbnail for (@path, @w x @h), or NULL. The pointer is valid
 * until the next companion_thumbs_* call on this instance. UI thread.
 * Touches the entry (LRU). Grid cells pass w == h; the boxart pane its
 * own size. */
const uint32_t *companion_thumbs_get(companion_thumbs_t *t,
      const char *path, int w, int h);

/* Ask for (@path, @edge) to be decoded. @urgent requests are served
 * most-recent-first ahead of every non-urgent one; non-urgent ones
 * (prefetch) are served oldest-first after them. A key already cached
 * or already queued is ignored (returns false). @bg is the ARGB colour
 * the letterbox is filled with. UI thread. */
bool companion_thumbs_request(companion_thumbs_t *t, const char *path,
      int w, int h, uintptr_t tag, bool urgent, uint32_t bg);

/* Drop every queued request (the view changed); cached thumbnails stay.
 * Decodes already in flight still land, are cached, and are delivered -
 * the backend validates the tag against its own view state, e.g. a row
 * generation it bumps on rebuild. UI thread. */
void companion_thumbs_cancel(companion_thumbs_t *t);

/* Deliver finished decodes (at most @max, 0 = all) through @cb; without
 * HAVE_THREADS this is also where decoding happens, for at most
 * @budget_us. Returns how many were delivered. UI thread. */
size_t companion_thumbs_poll(companion_thumbs_t *t,
      companion_thumbs_done_cb cb, void *ud, size_t max,
      unsigned budget_us);

/* Animation: what RetroArch's own File Browser does for the selected
 * thumbnail - an APNG, animated WEBP, WEBM or MP4 plays its frames on
 * the container's clock (no audio, as in the menu). One animation at a
 * time (the pane showing the selection). Frames arrive through
 * companion_thumbs_poll() as ordinary deliveries for (@path, @w x @h,
 * @tag), each frame already scaled and letterboxed; the backend blits
 * them into its pane exactly as it does a still. A still image, or a
 * type without an animation decoder, produces nothing (the still that
 * was requested normally stays). Decoding runs on its own thread and
 * stops on _animate_stop(), a new _animate(), or free(). UI thread. */
void companion_thumbs_animate(companion_thumbs_t *t, const char *path,
      int w, int h, uintptr_t tag, uint32_t bg);
void companion_thumbs_animate_stop(companion_thumbs_t *t);
/* True while an animation is playing (a backend may keep polling). */
bool companion_thumbs_animating(companion_thumbs_t *t);

/* Change the cache budget (bytes; 0 = the default); evicts down to it
 * at once. UI thread. */
void companion_thumbs_set_budget(companion_thumbs_t *t, size_t budget_bytes);

/* Forget every cached size of @path (the file changed - a download
 * replaced it, say) so the next request decodes it again. A decode in
 * flight for it still lands with the old pixels; the backend can call
 * this again afterwards. Returns how many entries were dropped. */
size_t companion_thumbs_forget(companion_thumbs_t *t, const char *path);

/* Diagnostics / tests. */
size_t companion_thumbs_cached_count(companion_thumbs_t *t);
size_t companion_thumbs_cached_bytes(companion_thumbs_t *t);
size_t companion_thumbs_queued(companion_thumbs_t *t);
/* Queued + decoding + finished-but-not-yet-polled: while non-zero a
 * backend must keep polling. */
size_t companion_thumbs_pending(companion_thumbs_t *t);

/* Letterbox @src (sw x sh ARGB) into a freshly allocated dw x dh ARGB
 * buffer filled with @bg. Pure; exposed for tests and for backends that
 * scale their own images (the boxart pane). Nearest-neighbour: plenty at
 * thumbnail size, and the same on every backend. */
uint32_t *companion_thumbs_scale(const uint32_t *src, unsigned sw,
      unsigned sh, int dw, int dh, uint32_t bg);
/* Same, with the source in R,G,B,A memory order (what an animation
 * stream emits when it will not emit ARGB words): the byte swap happens
 * on the pixels sampled, never over the whole canvas - at 4K that pass
 * cost 12 ms a frame against 0.2 ms here. When shrinking by two or
 * more in both directions the samplers average four taps per output
 * pixel instead of one (under 1 ms at 4K), which takes most of the
 * shimmer out of a downscaled video. */
uint32_t *companion_thumbs_scale_ex(const uint32_t *src, unsigned sw,
      unsigned sh, int dw, int dh, uint32_t bg, bool src_rgba_order);

RETRO_END_DECLS

#endif
