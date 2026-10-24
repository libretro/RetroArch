/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_player.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_VIDEO_PLAYER_H
#define __LIBRETRO_SDK_FORMAT_VIDEO_PLAYER_H

/* Plays an image_transfer animation stream (WEBM, MP4, WEBP, APNG) to
 * a sink on a clock the caller supplies.
 *
 * The player owns the pacing - each frame's container duration, the
 * loop count, catch-up when the decode falls behind the file's rate -
 * and the decode: on a shared worker thread with two jobs, so one
 * frame is decoded while the one before it waits to be shown, or on
 * the polling thread (VIDEO_PLAYER_INLINE, or a build or run without
 * the worker). Hardware setup uses the worker when available.
 * The sink owns where frames go: the buffers they are
 * decoded into, and showing them.
 *
 * video_player_poll() is called once a tick by one thread; the sink's
 * slot_begin, slot_end and present run on that thread, feed and
 * decoded on the thread that decodes. No lock is taken anywhere. */

/* MP4 and WebM movies with a declared duration use movie-relative picture
 * timestamps and the full movie period. The first picture waits through
 * a leading empty edit; the last picture is held through the video tail
 * and the next loop's leading interval. Still/reference decoding is
 * independent of this presentation schedule. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>
#include <formats/image.h>

RETRO_BEGIN_DECLS

typedef struct video_player video_player_t;

enum video_player_flags
{
   /* Frames as R,G,B,A in memory; ARGB words otherwise. */
   VIDEO_PLAYER_RGBA   = (1 << 0),
   /* Frames as linear scRGB half floats, 8 bytes a pixel. Only a frame
    * the stream decodes into the slot itself can be one: any other
    * ends the playback on the last good frame. */
   VIDEO_PLAYER_FP16   = (1 << 1),
   /* Decode frames on the polling thread; cold hardware setup may use the worker. */
   VIDEO_PLAYER_INLINE = (1 << 2)
};

/* How a frame reached its buffer. */
enum video_player_how
{
   VIDEO_PLAYER_DIRECT = 0, /* decoded into the slot                */
   VIDEO_PLAYER_COPY,       /* copied into the slot from a canvas   */
   VIDEO_PLAYER_SWIZZLE,    /* copied, red and blue exchanged       */
   VIDEO_PLAYER_CANVAS      /* left in the stream's canvas          */
};

enum video_player_present
{
   VIDEO_PLAYER_PRESENT_DONE = 0, /* shown                                  */
   VIDEO_PLAYER_PRESENT_QUEUED,   /* on its way: the slot is the sink's
                                     until video_player_slot_released     */
   VIDEO_PLAYER_PRESENT_BUSY,     /* not now: offered again next poll    */
   VIDEO_PLAYER_PRESENT_FAILED    /* the playback ends                   */
};

#define VIDEO_PLAYER_HW_REFUSED (-2)

enum video_player_state
{
   VIDEO_PLAYER_PLAYING = 0,
   VIDEO_PLAYER_ENDED,         /* loops done, or a decode error */
   VIDEO_PLAYER_SEEK_FAILED    /* picture retained; a new seek can recover */
};

typedef struct video_player_frame
{
   /* Movie timeline, including completed loops; -1 when unavailable. */
   int64_t time_us;
   /* The frame in the stream's canvas, valid until the next poll; NULL
    * when it is in the slot. */
   const uint32_t *pixels;
   unsigned slot;
   unsigned width, height;
   int duration_ms;
   enum video_player_how how;
   /* pixels are in the order the flags asked for; a canvas from a
    * stream that cannot emit it is R,G,B,A in memory */
   bool native;
   /* A frame decoded on the GPU (start_hw), NULL otherwise. DONE or
    * QUEUED makes it the sink's, to read or abandon; on BUSY a frame
    * decoded on the worker is offered again, any other is given back
    * by the player. A movie frame decoded ahead on the polling thread
    * is also retained for retry until taken, replaced by a seek or freed. */
   const video_hw_frame_t *hw;
} video_player_frame_t;

typedef struct video_player_sink
{
   void *user;
   /* Once, before the first decode: make whatever the frames need for
    * @num_slots slots of @width x @height (num_slots 0: the sink takes
    * canvases). Returns the video_player_flags to decode with, or < 0
    * to be asked again on a later poll. */
   int (*start)(void *user, unsigned width, unsigned height,
         unsigned num_slots);
   /* The buffer slot @slot's next frame is decoded into, or NULL while
    * it is not writable (a later poll tries again). NULL function: no
    * slots, frames are presented from the stream's canvas. */
   uint32_t *(*slot_begin)(void *user, unsigned slot);
   /* The player writes slot @slot no more. Optional. */
   void (*slot_end)(void *user, unsigned slot);
   enum video_player_present (*present)(void *user,
         const video_player_frame_t *frame);
   /* Optional, on the decoding thread before each decode; false ends
    * the playback. */
   bool (*feed)(void *user);
   /* Optional, on the decoding thread after each decode. */
   void (*decoded)(void *user, enum video_player_how how);
   /* Optional: the sink takes frames decoded on the GPU. Once, before
    * the first decode, instead of start when the stream's decoder has
    * a hardware backend for it (image_transfer_anim_stream_set_hw):
    * frames then come as video_player_frame_t::hw, into no slot;
    * @jobs is 2 when they are decoded on the worker, a frame ahead,
    * and 1 in line. As start returns. */
   /* VIDEO_PLAYER_HW_REFUSED permanently refuses hardware;
    * -1 retains the existing retry-later contract. */
   int (*start_hw)(void *user, unsigned width, unsigned height,
         unsigned jobs);
   /* Optional, after slot_begin: row stride in pixels for that buffer.
    * Absent means width. Snapshotted before the decode is posted. */
   unsigned (*slot_stride)(void *user, unsigned slot);
} video_player_sink_t;

/* Plays @stream (borrowed; freed by the caller after the player) from
 * where it stands. @loop_count as the container says, 0 forever. */
video_player_t *video_player_new(void *stream, enum image_type_enum type,
      int loop_count, const video_player_sink_t *sink, unsigned flags);

/* Stops playback: queued work will not run; running decode/setup is
 * waited out. Every started slot is ended. */
void video_player_free(video_player_t *p);

/* Cancels queued work without waiting. False: running decode/setup still
 * borrows the player, stream and sink; retain them and poll stop again.
 * True: free() will not wait. A stopped player cannot resume. */
bool video_player_stop(video_player_t *p);

/* Polling owner: changes loop policy at the next clean movie EOF. */
bool video_player_set_repeat(video_player_t *p, bool repeat);
/* Invalid timing or a rate change; cleared after fresh stable evidence. */
bool video_player_cadence_unstable(const video_player_t *player);
/* Measured from 32 stable movie timestamp intervals. Polling-thread only. */
bool video_player_cadence(const video_player_t *player,
      unsigned *numerator, unsigned *denominator);

/* Clean finite EOF after the final picture duration, excluding failures. */
bool video_player_completed(const video_player_t *p);

/* Advances the playback to @now_us (any monotonic microsecond clock,
 * the same one every call): starts decodes, presents the frame that is
 * due. */
enum video_player_state video_player_poll(video_player_t *p, int64_t now_us);

/* Polling-thread transport primitive. Pausing keeps the current picture
 * and remaining display duration; a decode already queued/running may
 * finish but is not presented until resumed. Does not stop or seek audio:
 * the owning media session coordinates its audio on the same clock.
 * Repeated requests are idempotent. A stopped player cannot resume. */
void video_player_set_paused(video_player_t *p, bool paused, int64_t now_us);
bool video_player_paused(const video_player_t *p);

/* Whether a picture has been accepted by the sink on the polling thread
 * (a QUEUED picture may still be in flight to the graphics driver). */
bool video_player_has_presented(const video_player_t *p);

/* Sink resources are ready. Audio may start at the movie origin before
 * a delayed first video picture is due. Polling thread only. */
bool video_player_started(const video_player_t *p);

/* Optional first-frame hardware recovery. Installed before startup. Called on
 * the exclusive decode owner after decode jobs and sink slots have settled:
 * -1 cannot recover, 0 another bounded step, 1 software stream rewound/ready.
 * The callback must not re-enable hardware or wait on renderer completion.
 * Returning ready restarts sink.start, without calling sink.start_hw again. */
typedef int (*video_player_hw_restart_t)(void *user);
bool video_player_set_hw_fallback(video_player_t *p,
      video_player_hw_restart_t restart, void *user);
/* Polling-thread renderer refusal notification (including a failed first async
 * import). Idempotent while recovery is pending; does not wait or run restart.
 * The sink must settle/release its queued slots normally before restart runs. */
bool video_player_request_software(video_player_t *p);

enum video_player_seek_result
{
   VIDEO_PLAYER_SEEK_PROGRESS = 2
};

/* One bounded seek step: -1 failed, 0 waits for a later poll,
 * 1 positioned, 2 made progress and can run another bounded step.
 * restart begins a new
 * request, including replacing an unfinished one. The callback owns
 * stream/window preparation; it must not wait. */
typedef int (*video_player_seek_cb_t)(void *user, int64_t target_us,
      bool restart, int64_t *position_us);

/* Install before the first poll. The handler/user outlive the player. */
bool video_player_set_seek_handler(video_player_t *p,
      video_player_seek_cb_t handler, void *user);
/* Polling-thread command: latest request wins, negative targets clamp
 * to zero. No more old frames are offered to the sink; an already
 * accepted frame may still be in flight to its driver. */
bool video_player_seek(video_player_t *p, int64_t target_us);
bool video_player_seeking(const video_player_t *p);
/* Last completed request. Normal seeks complete when their replacement
 * picture is accepted; a leading-gap seek can complete when the next
 * picture is decoded and known to lie in the future. False while a newer
 * request is pending. */
bool video_player_seek_position(const video_player_t *p, int64_t *position_us);

/* When the next frame is due on that clock; 0 at once, INT64_MAX while
 * paused. A paused poll returns PLAYING without presenting or feeding. */
int64_t video_player_due(const video_player_t *p);

/* A frame presented as QUEUED has been taken: its slot may be decoded
 * into again. */
void video_player_slot_released(video_player_t *p, unsigned slot);

/* Threads the decode may use for colour conversion and the decoders'
 * own tile and row parallelism; 1 is none beside the worker. Takes
 * effect between frames. */
void video_player_set_threads(unsigned threads);

/* Tears down the shared worker and its pool, which the next player
 * that decodes threaded creates again. Stop threaded players first;
 * this joins any running stopped decode. */
void video_player_worker_deinit(void);

RETRO_END_DECLS

#endif
