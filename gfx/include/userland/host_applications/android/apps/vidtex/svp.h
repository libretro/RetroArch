/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SVP_H
#define SVP_H

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 * Simple video player using MMAL.
 * Uses MMAL container reader plus video decode component, and provides callback to retrieve
 * buffers of decoded video frames.
 *
 * Thread-safety: The public API functions must be called from the same thread for a given SVP_T
 *                instance. The user is notified of decoded frames and other events from a separate
 *                thread, started by svp_start().
 */

/** Flags indicating reason for processing stop */
/** Stop on to user request */
#define SVP_STOP_USER    (1 << 0)
/** Stop on end-of-stream */
#define SVP_STOP_EOS     (1 << 1)
/** Stop on error */
#define SVP_STOP_ERROR   (1 << 2)
/** Stop on timer */
#define SVP_STOP_TIMEUP  (1 << 3)

/**
 * Callback functions and context for player to communicate asynchronous data
 * and events to user.
 */
typedef struct SVP_CALLBACKS_T
{
   /** Private pointer specified by caller, passed to callback functions.
    * Not examined by svp_ functions, so may have any value, including NULL.
    */
   void *ctx;

   /** Callback for decoded video frames.
    * The buffer is released when this function returns, so the callback must either have processed
    * the buffer or acquired a reference before returning.
    * @param ctx  Caller's private context, as specified by SVP_CALLBACKS_T::ctx.
    * @param ob   MMAL opaque buffer handle.
    */
   void (*video_frame_cb)(void *ctx, void *buf);

   /** Callback for end of processing.
    * @param ctx     Caller's private context, as specified by SVP_CALLBACKS_T::ctx.
    * @param reason  Bitmask of SVP_STOP_XXX values giving reason for stopping.
    */
   void (*stop_cb)(void *ctx, uint32_t stop_reason);
} SVP_CALLBACKS_T;

/** Options to SVP playback */
typedef struct SVP_OPTS_T
{
   /** Duration of playback, in milliseconds. 0 = default, which is full duration for media and
    * a limited duration for camera.
    */
   unsigned duration_ms;
} SVP_OPTS_T;

/** Playback stats */
typedef struct SVP_STATS_T
{
   /** Total number of video frames processed since the last call to svp_start().
    * 0 if svp_start() has never been called. */
   unsigned video_frame_count;
} SVP_STATS_T;

/** Simple Video Player instance. Opaque structure. */
typedef struct SVP_T SVP_T;

/**
 * Create a simple video player instance.
 * @param uri        URI to media, or NULL to use camera preview.
 * @param callbacks  Callbacks for caller to receive notification of events,
 *                   such as decoded buffers.
 * @param opts       Player options.
 * @return Newly created simple video player instance, or NULL on error.
 */
SVP_T *svp_create(const char *uri, SVP_CALLBACKS_T *callbacks, const SVP_OPTS_T *opts);

/**
 * Destroy a simple video player instance.
 * @param svp  Simple video player instance. May be NULL, in which case this
 *             function does nothing.
 */
void svp_destroy(SVP_T *svp);

/**
 * Start a simple video player instance.
 * Decoded frames are returned to the SVP_CALLBACKS_T::video_frame_cb function
 * passed to svp_create().
 * @param svp  Simple video player instance.
 * @return 0 on success; -1 on failure.
 */
int svp_start(SVP_T *svp);

/**
 * Stop a simple video player instance.
 * If the player is not running, then this function does nothing.
 * @param svp  Simple video player instance.
 */
void svp_stop(SVP_T *svp);

/**
 * Get player stats.
 * @param svp    Simple video player instance.
 * @param stats  Buffer in which stats are returned.
 */
void svp_get_stats(SVP_T *svp, SVP_STATS_T *stats);

#ifdef __cplusplus
}
#endif

#endif /* SVP_H */
