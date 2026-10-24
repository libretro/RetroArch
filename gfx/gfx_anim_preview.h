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

#include <retro_atomic.h>
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>
#include <retro_common_api.h>

#include "video_defines.h"   /* VIDEO_SCALE_PACK */

RETRO_BEGIN_DECLS

struct data_transfer;
typedef struct gfx_anim_preview_audio_prepared gfx_anim_preview_audio_prepared_t;

/* Window sizing shared by every consumer (see gfx_thumbnail.c history
 * for how these were arrived at). */
#define GFX_ANIM_PREVIEW_WINDOW_KEEP  (4 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_WINDOW_AHEAD (8 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_WINDOW_BACK  (8 * 1024 * 1024)
#define GFX_ANIM_PREVIEW_FEED_BUDGET  (512 * 1024)
#define GFX_ANIM_PREVIEW_SEEK_AHEAD   (32 * 1024)
/* Bitrate-sized feed for the video types: seconds of content kept
 * committed ahead of and behind the decoder, and the floors under
 * them. A short lookahead is refilled every animate() tick with
 * FEED_BUDGET, so two seconds is many ticks of slack at any bitrate
 * a preview plays; the floors cover a packet burst on a low-bitrate
 * file. The fixed AHEAD/BACK above stay as the ceilings.  */
#define GFX_ANIM_PREVIEW_FEED_AHEAD_SECS   2
#define GFX_ANIM_PREVIEW_FEED_BACK_MSECS   500
#define GFX_ANIM_PREVIEW_FEED_AHEAD_MIN    (1024 * 1024)
#define GFX_ANIM_PREVIEW_FEED_BACK_MIN     (256 * 1024)
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
   /* Both axes in one word, VIDEO_SCALE_PACK's layout. */
   unsigned dims;
   int num_frames, loop_count;
   int64_t timeline_start_us; /* immutable movie origin, currently zero */
   bool seekable; /* immutable decoder capability, sampled before publication */
   bool windowed;                /* dt is a sliding window (reserved) */
   /* Feed geometry for this session. The video types size these to
    * the container's bitrate at open (see gfx_anim_preview_open);
    * the frame-indexed types keep the fixed constants. */
   size_t feed_ahead;
   size_t feed_back;
   /* Where the resident window ended after the last feed: how far
    * ahead of the decoder the feed still is, before this tick's. */
   size_t feed_res_hi;
   /* bytes the window holds, as of the last feed; any thread reads it */
   retro_atomic_int_t resident_seen;
   size_t feed_tell;    /* the decoder's position at the last feed */
   /* Whether the stream emits ARGB words (asked once at open; a stream
    * that honours it keeps that order for the whole animation, and
    * some - APNG - report "no" to a repeat request after the first
    * frame even though the order stands). */
   bool native_argb;
   char *path;

   /* preview audio (WEBM / MP4), main thread */
   struct data_transfer *audio_dt;
   void *audio_owner;            /* retains the audio window independently */
   struct audio_mixer_voice *audio_voice; /* static-pool clock binding */
   size_t audio_serial;
   size_t audio_hi;
   size_t audio_tell;   /* the decoder's position at the last feed */
   int    audio_slot;
   bool   paused;               /* main-thread media transport state */
   bool   playback_started;     /* playback sink ready, main thread */
   bool   clock_initialized;
   bool   clock_audio;
   int64_t clock_us, clock_wall_us;
   size_t clock_frames;
   size_t clock_serial;
   unsigned clock_rate, clock_fraction;
   bool seeking; /* main-thread transport hold, independent of user pause */
   bool seek_failed;
   retro_atomic_int_t seek_audio_format; /* main publishes, worker snapshots */
   unsigned seek_format, seek_phase; /* decoding-thread state below */
   size_t seek_feed_hi, seek_feed_anchor;
   int64_t seek_video_us;
   gfx_anim_preview_audio_prepared_t *seek_audio;
   int64_t media_origin_us, media_clock_us, media_requested_us; /* main-thread position origin */
   bool media_clock_valid;
} gfx_anim_preview_t;

/* Open @path for animated playback. @png_probe: what a still-load task
 * already learned about a PNG (< 0 unknown - probe here; 0 still; 1
 * APNG); pass -1 when there is no verdict. Returns NULL for a still, a
 * type without an animation decoder, a malformed file, or one that
 * fails admission - the caller keeps whatever still it has. */
gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe);

/* GPU player only: may return a stream without CPU pixel output. Native
 * admission is deferred to the player; open does not construct a backend. */
gfx_anim_preview_t *gfx_anim_preview_open_hw(const char *path, int png_probe);

/* Cheap "does this file animate?" from its head alone, without opening
 * a session: 1 animated, 0 a still (or a type without an animation
 * decoder), -1 unreadable.  Reads 4 KiB of a PNG (acTL precedes the
 * first IDAT) and 32 bytes of a WebP (VP8X's animation flag); answers
 * 1 for WEBM and MP4 without reading.  Lets a still request take the
 * anim-first route - the animation's first frame IS the still - for
 * an animated file, instead of a whole-file still decode that the
 * animation then re-reads windowed. */
int gfx_anim_preview_probe(const char *path);

/* Per tick, before decoding: keep the window straddling the decoder's
 * frontier and raise the demuxer's bound to what is resident. false on
 * an I/O failure (the caller should close: the decoder would loop
 * early on an end-of-data wall). No-op for a non-windowed session. */
bool gfx_anim_preview_feed(gfx_anim_preview_t *p);

/* Bytes of the file this session currently holds resident (head plus
 * window; the whole file for a non-windowed session). For readouts
 * and harness numbers. */
size_t gfx_anim_preview_resident_bytes(const gfx_anim_preview_t *p);

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

/* Whether the session's mapping is a sliding window (a reservation)
 * rather than a whole-file buffer. gfx_thumbnail carries this into its
 * own install so admission and the feeder agree with the open. */
bool gfx_anim_preview_windowed(const gfx_anim_preview_t *p);

/* Preview audio: start the container's audio track looping through the
 * mixer (WEBM / MP4 only; honours the menu_thumbnail_preview_audio
 * setting), feed its window per tick, stop it. Main thread. */
/* Worker-owned preparation over an independent source window. Currently
 * MP4 AAC; mix_rate/s16 are the main thread's captured device format.
 * Step prepares at most one access unit with bounded source refill.
 * Transfer the object to the main thread only after step returns 1;
 * adopt consumes it on every outcome. Free cancels an unpublished seek.
 * The session must remain alive while new reads its immutable metadata. */
gfx_anim_preview_audio_prepared_t *gfx_anim_preview_audio_prepare_new(
      const gfx_anim_preview_t *p, int64_t target_us, unsigned mix_rate, bool s16);
int gfx_anim_preview_audio_prepare_step(gfx_anim_preview_audio_prepared_t *a);
void gfx_anim_preview_audio_prepare_free(gfx_anim_preview_audio_prepared_t *a);
bool gfx_anim_preview_audio_adopt(gfx_anim_preview_t *p,
      gfx_anim_preview_audio_prepared_t *a);

/* Main-thread request setup before the player is polled; rate0 disables
 * audio preparation. Step is a video_player seek callback on its decoder
 * thread. Commit only after that player completed the latest seek AND
 * accepted its first target picture, using the player's landed position. */
bool gfx_anim_preview_seek_request(gfx_anim_preview_t *p,
      unsigned mix_rate, bool s16);
int gfx_anim_preview_seek_step(void *user, int64_t target_us,
      bool restart, int64_t *position_us);
void gfx_anim_preview_seek_cancel(gfx_anim_preview_t *p);
void gfx_anim_preview_seek_commit(gfx_anim_preview_t *p, int64_t position_us);

void gfx_anim_preview_audio_begin(gfx_anim_preview_t *p);
bool gfx_anim_preview_audio_feed(gfx_anim_preview_t *p);
void gfx_anim_preview_audio_stop(gfx_anim_preview_t *p);

/* Main thread: preserve audio position, including an audio voice that
 * begins later when a pending file read completes. */
void gfx_anim_preview_set_repeat(gfx_anim_preview_t *p, bool repeat);
void gfx_anim_preview_set_paused(gfx_anim_preview_t *p, bool paused);

/* Main thread: one logical clock for video and transport. Follows actual
 * mixed PCM while audio is live; monotonic wall time otherwise. Pauses
 * freeze it. This is not the hardware audio-device play cursor. */
int64_t gfx_anim_preview_clock(gfx_anim_preview_t *p, int64_t wall_us);

/* Close: audio, stream, mapping. */
void gfx_anim_preview_close(gfx_anim_preview_t *p);

RETRO_END_DECLS

#endif
