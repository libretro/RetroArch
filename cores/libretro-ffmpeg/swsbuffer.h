#ifndef __LIBRETRO_SDK_SWSBUFFER_H__
#define __LIBRETRO_SDK_SWSBUFFER_H__

#include <retro_common_api.h>

#include <boolean.h>

#include <libavutil/frame.h>
#include <libswscale/swscale.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

#ifndef PIX_FMT_RGB32
#define PIX_FMT_RGB32 AV_PIX_FMT_RGB32
#endif

/**
 * sws_context
 * 
 * Context object for the sws worker threads.
 * 
 */
struct sws_context
{
   int index;
   struct SwsContext *sws;
   AVFrame *source;
#if LIBAVUTIL_VERSION_MAJOR > 55
   AVFrame *hw_source;
#endif
   AVFrame *target;
   void *frame_buf;
};
typedef struct sws_context sws_context_t;

/**
 * swsbuffer
 * 
 * The swsbuffer is a ring buffer, that can be used as a 
 * buffer for many workers while keeping the order.
 * 
 * It is thread safe in a sensem that it is designed to work
 * with one work coordinator, that allocates work slots for
 * workers threads to work on and later collect the work
 * product in the same order, as the slots were allocated.
 * 
 */
struct swsbuffer;
typedef struct swsbuffer swsbuffer_t;

/**
 * swsbuffer_create:
 * @num           : Size of the buffer.
 * @frame_size    : Size of the target frame.
 * @width         : Width of the target frame.
 * @height        : Height of the target frame.
 *
 * Create a swsbuffer.
 * 
 * Returns: swsbuffer.
 */
swsbuffer_t *swsbuffer_create(size_t num, int frame_size, int width, int height);

/** 
 * swsbuffer_destroy:
 * @swsbuffer      : sswsbuffer.
 * 
 * Destory a swsbuffer.
 * 
 * Does also free the buffer allocated with swsbuffer_create().
 * User has to shut down any external worker threads that may have
 * a reference to this swsbuffer.
 * 
 **/
void swsbuffer_destroy(swsbuffer_t *swsbuffer);

/** 
 * swsbuffer_clear:
 * @swsbuffer      : sswsbuffer.
 * 
 * Clears a swsbuffer.
 * 
 **/
void swsbuffer_clear(swsbuffer_t *swsbuffer);

/** 
 * swsbuffer_get_open_slot:
 * @swsbuffer     : sswsbuffer.
 * @contex        : sws context.
 * 
 * Returns the next open context inside the ring buffer
 * and it's index. The status of the slot will be marked as
 * 'in progress' until slot is marked as finished with
 * swsbuffer_finish_slot();
 *
 **/
void swsbuffer_get_open_slot(swsbuffer_t *swsbuffer, sws_context_t **context);

/** 
 * swsbuffer_return_open_slot:
 * @swsbuffer     : sswsbuffer.
 * @contex        : sws context.
 * 
 * Marks the given sws context that is "in progress" as "open" again.
 *
 **/
void swsbuffer_return_open_slot(swsbuffer_t *swsbuffer, sws_context_t *context);

/** 
 * swsbuffer_open_slot:
 * @swsbuffer     : sswsbuffer.
 * @context       : sws context.
 * 
 * Sets the status of the given context from "finished" to "open".
 * The slot is then available for producers to claim again with swsbuffer_get_open_slot().
 **/
void swsbuffer_open_slot(swsbuffer_t *swsbuffer, sws_context_t *context);

/**
 * swsbuffer_get_finished_slot:
 * @swsbuffer     : sswsbuffer.
 * @context       : sws context.
 * 
 * Returns a reference for the next context inside
 * the ring buffer. User needs to use swsbuffer_open_slot()
 * to open the slot in the ringbuffer for the next
 * work assignment. User is free to re-allocate or
 * re-use the context.
 *
 */
void swsbuffer_get_finished_slot(swsbuffer_t *swsbuffer, sws_context_t **context);

/**
 * swsbuffer_finish_slot:
 * @swsbuffer     : sswsbuffer.
 * @context       : sws context.
 * 
 * Sets the status of the given context from "in progress" to "finished".
 * This is normally done by a producer. User can then retrieve the finished work
 * context by calling swsbuffer_get_finished_slot().
 */
void swsbuffer_finish_slot(swsbuffer_t *swsbuffer, sws_context_t *context);

/**
 * swsbuffer_has_open_slot:
 * @swsbuffer      : sswsbuffer.
 * 
 * Returns true if the buffer has a open slot available.
 */
bool swsbuffer_has_open_slot(swsbuffer_t *swsbuffer);

/**
 * swsbuffer_has_finished_slot:
 * @swsbuffer      : sswsbuffer.
 * 
 * Returns true if the buffers next slot is finished and a
 * context available.
 */
bool swsbuffer_has_finished_slot(swsbuffer_t *swsbuffer);

RETRO_END_DECLS

#endif
