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

#ifndef __GFX_ANIM_PREVIEW_H
#define __GFX_ANIM_PREVIEW_H

/* The animated-thumbnail preview machinery of RetroArch's File Browser
 * (gfx_thumbnail), factored out so every consumer plays a file the same
 * way: the menu's thumbnail, and the desktop companions' preview panes.
 *
 *  - open:  the sliding-window progressive open over the file
 *           (data_transfer_open_window + image_transfer_anim_stream_new_
 *           avail): playback starts from the first resident bytes, a
 *           tail-moov MP4 opens from a few MiB, memory admission scales
 *           with the heap and never slurps a multi-GB file
 *  - feed:  per tick, keep the committed window straddling the decoder's
 *           byte frontier under a per-tick I/O budget, and hand the
 *           demuxer what is actually resident
 *  - audio: the container's audio track (WEBM / MP4) through the audio
 *           mixer, looped, over its own window, with its own feeder
 *  - close: everything down, in the right order
 *
 * Frames themselves come from image_transfer_anim_stream_next() on the
 * session's stream; the caller owns the clock (each frame's duration).
 *
 * Threading: open / feed / next / rewind may run on any one thread at a
 * time (gfx_thumbnail runs them on its worker; a companion on its
 * animation thread). audio_* run on the main thread (the mixer). */

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct data_transfer;

/* Window sizing shared by every consumer (see gfx_thumbnail.c history
 * for how these were arrived at). */
#define GFX_ANIM_PREVIEW_WINDOW_KEEP  (4 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_WINDOW_AHEAD (8 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_WINDOW_BACK  (8 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_FEED_BUDGET  (512 * 1024)
#define GFX_ANIM_PREVIEW_ABS_MAX_FILE (1024 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_MAX_FILE     (256 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_MAX_PIXELS   (3840 * 2160)
#define GFX_ANIM_PREVIEW_DUR_DEFAULT  100
#define GFX_ANIM_PREVIEW_DUR_MIN      16

typedef struct gfx_anim_preview
{
   void *stream;                 /* image_transfer anim stream */
   struct data_transfer *dt;     /* owns the mapping the stream borrows */
   const uint8_t *base;          /* mapping base (dt) */
   size_t len;                   /* full logical file length */
   enum image_type_enum type;
   unsigned width, height;
   int num_frames, loop_count;
   bool windowed;                /* dt is a sliding window (reserved) */
   /* Whether the stream emits ARGB words (asked once at open; a stream
    * that honours it keeps that order for the whole animation, and
    * some - APNG - report "no" to a repeat request after the first
    * frame even though the order stands). */
   bool native_argb;
   char *path;

   /* preview audio (WEBM / MP4), main thread */
   struct data_transfer *audio_dt;
   size_t audio_hi;
   int    audio_slot;
} gfx_anim_preview_t;

/* Open @path for animated playback. @png_probe: what a still-load task
 * already learned about a PNG (< 0 unknown - probe here; 0 still; 1
 * APNG); pass -1 when there is no verdict. Returns NULL for a still, a
 * type without an animation decoder, a malformed file, or one that
 * fails admission - the caller keeps whatever still it has. */
gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe);

/* Per tick, before decoding: keep the window straddling the decoder's
 * frontier and raise the demuxer's bound to what is resident. false on
 * an I/O failure (the caller should close: the decoder would loop
 * early on an end-of-data wall). No-op for a non-windowed session. */
bool gfx_anim_preview_feed(gfx_anim_preview_t *p);

/* Next displayed frame: the stream's canvas (valid until the next call).
 * *native_argb tells whether it is ARGB words (decided once at open) or
 * R,G,B,A memory order the caller must swizzle. NULL at the end of a
 * pass - the caller handles the loop count and rewinds. @duration_ms is
 * clamped per the container rules (default 100, min 16). */
const uint32_t *gfx_anim_preview_next(gfx_anim_preview_t *p,
      int *duration_ms, bool *native_argb);
void gfx_anim_preview_rewind(gfx_anim_preview_t *p);

/* Memory admission, exposed for a caller that knows its canvas size
 * after open (gfx_thumbnail charges the upload buffers too). */
bool gfx_anim_preview_mem_ok(gfx_anim_preview_t *p, uint64_t px);
/* The same admission on raw figures: @charge is what playback pins
 * (the file length for a resident buffer, the window for a windowed
 * one), @px the canvas. @windowed selects the test. */
bool gfx_anim_preview_admit(uint64_t charge, uint64_t px);
bool gfx_anim_preview_window_admit(uint64_t px);

/* Wrap an already-open stream / mapping (gfx_thumbnail installs those
 * itself, including ones adopted from a still decode) in a session that
 * only feeds and plays audio; it does NOT own them. Release with
 * gfx_anim_preview_release(), which frees the session and stops its
 * audio but leaves stream and mapping to the owner. */
gfx_anim_preview_t *gfx_anim_preview_wrap(void *stream,
      enum image_type_enum type, struct data_transfer *dt,
      const uint8_t *base, size_t len, bool windowed, const char *path);
void gfx_anim_preview_release(gfx_anim_preview_t *p);

/* Preview audio: start the container's audio track looping through the
 * mixer (WEBM / MP4 only; honours the menu_thumbnail_preview_audio
 * setting), feed its window per tick, stop it. Main thread. */
void gfx_anim_preview_audio_begin(gfx_anim_preview_t *p);
bool gfx_anim_preview_audio_feed(gfx_anim_preview_t *p);
void gfx_anim_preview_audio_stop(gfx_anim_preview_t *p);

/* Close: audio, stream, mapping. */
void gfx_anim_preview_close(gfx_anim_preview_t *p);

RETRO_END_DECLS

#endif
