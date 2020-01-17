#ifndef __LIBRETRO_SDK_VIDEOBUFFER_H__
#define __LIBRETRO_SDK_VIDEOBUFFER_H__

#include <retro_common_api.h>

#include <boolean.h>
#include <stdint.h>

#ifdef HAVE_SSA
#include <ass/ass.h>
#endif

#include <libavutil/frame.h>
#include <libswscale/swscale.h>

#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

#ifndef PIX_FMT_RGB32
#define PIX_FMT_RGB32 AV_PIX_FMT_RGB32
#endif

/**
 * video_decoder_context
 * 
 * Context object for the sws worker threads.
 * 
 */
struct video_decoder_context
{
   int index;
   int64_t pts;
   struct SwsContext *sws;
   AVFrame *source;
#if LIBAVUTIL_VERSION_MAJOR > 55
   AVFrame *hw_source;
#endif
   AVFrame *target;
#ifdef HAVE_SSA
   ASS_Track *ass_track_active;
#endif
   uint8_t *frame_buf;
};
typedef struct video_decoder_context video_decoder_context_t;

/**
 * video_buffer
 * 
 * The video buffer is a ring buffer, that can be used as a 
 * buffer for many workers while keeping the order.
 * 
 * It is thread safe in a sensem that it is designed to work
 * with one work coordinator, that allocates work slots for
 * workers threads to work on and later collect the work
 * product in the same order, as the slots were allocated.
 * 
 */
struct video_buffer;
typedef struct video_buffer video_buffer_t;

/**
 * video_buffer_create:
 * @capacity      : Size of the buffer.
 * @frame_size    : Size of the target frame.
 * @width         : Width of the target frame.
 * @height        : Height of the target frame.
 *
 * Create a video buffer.
 * 
 * Returns: A video buffer.
 */
video_buffer_t *video_buffer_create(size_t capacity, int frame_size, int width, int height);

/** 
 * video_buffer_destroy:
 * @video_buffer      : video buffer.
 * 
 * Destroys a video buffer.
 * 
 * Does also free the buffer allocated with video_buffer_create().
 * User has to shut down any external worker threads that may have
 * a reference to this video buffer.
 * 
 **/
void video_buffer_destroy(video_buffer_t *video_buffer);

/** 
 * video_buffer_clear:
 * @video_buffer      : video buffer.
 * 
 * Clears a video buffer.
 * 
 **/
void video_buffer_clear(video_buffer_t *video_buffer);

/** 
 * video_buffer_get_open_slot:
 * @video_buffer     : video buffer.
 * @context          : sws context.
 * 
 * Returns the next open context inside the ring buffer
 * and it's index. The status of the slot will be marked as
 * 'in progress' until slot is marked as finished with
 * video_buffer_finish_slot();
 *
 **/
void video_buffer_get_open_slot(video_buffer_t *video_buffer, video_decoder_context_t **context);

/** 
 * video_buffer_return_open_slot:
 * @video_buffer     : video buffer.
 * @context          : sws context.
 * 
 * Marks the given sws context that is "in progress" as "open" again.
 *
 **/
void video_buffer_return_open_slot(video_buffer_t *video_buffer, video_decoder_context_t *context);

/** 
 * video_buffer_open_slot:
 * @video_buffer     : video buffer.
 * @context          : sws context.
 * 
 * Sets the status of the given context from "finished" to "open".
 * The slot is then available for producers to claim again with video_buffer_get_open_slot().
 **/
void video_buffer_open_slot(video_buffer_t *video_buffer, video_decoder_context_t *context);

/**
 * video_buffer_get_finished_slot:
 * @video_buffer     : video buffer.
 * @context          : sws context.
 * 
 * Returns a reference for the next context inside
 * the ring buffer. User needs to use video_buffer_open_slot()
 * to open the slot in the ringbuffer for the next
 * work assignment. User is free to re-allocate or
 * re-use the context.
 */
void video_buffer_get_finished_slot(video_buffer_t *video_buffer, video_decoder_context_t **context);

/**
 * video_buffer_finish_slot:
 * @video_buffer     : video buffer.
 * @context          : sws context.
 * 
 * Sets the status of the given context from "in progress" to "finished".
 * This is normally done by a producer. User can then retrieve the finished work
 * context by calling video_buffer_get_finished_slot().
 */
void video_buffer_finish_slot(video_buffer_t *video_buffer, video_decoder_context_t *context);

/**
 * video_buffer_wait_for_open_slot:
 * @video_buffer      : video buffer.
 * 
 * Blocks until open slot is available.
 * 
 * Returns true if the buffer has a open slot available.
 */
bool video_buffer_wait_for_open_slot(video_buffer_t *video_buffer);

/**
 * video_buffer_wait_for_finished_slot:
 * @video_buffer      : video buffer.
 *
 * Blocks until finished slot is available.
 * 
 * Returns true if the buffers next slot is finished and a
 * context available.
 */
bool video_buffer_wait_for_finished_slot(video_buffer_t *video_buffer);

/**
 * bool video_buffer_has_open_slot(video_buffer_t *video_buffer)
:
 * @video_buffer      : video buffer.
 * 
 * Returns true if the buffer has a open slot available.
 */
bool video_buffer_has_open_slot(video_buffer_t *video_buffer);

/**
 * video_buffer_has_finished_slot:
 * @video_buffer      : video buffer.
 *
 * Returns true if the buffers next slot is finished and a
 * context available.
 */
bool video_buffer_has_finished_slot(video_buffer_t *video_buffer);

RETRO_END_DECLS

#endif
