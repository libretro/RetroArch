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

#define VCOS_VERIFY_BKPTS 1 // TODO remove
#define VCOS_LOG_CATEGORY (&log_cat)

#include "interface/khronos/common/khrn_client.h"
#include "interface/vcos/vcos.h"
#ifdef KHRN_FRUIT_DIRECT
#include "middleware/khronos/egl/egl_server.h"
#include "middleware/khronos/ext/egl_khr_image.h"
#include "middleware/khronos/common/khrn_umem.h"
#endif

#include "interface/khronos/wf/wfc_client_stream.h"
#include "interface/khronos/wf/wfc_server_api.h"

//#define WFC_FULL_LOGGING
#ifdef WFC_FULL_LOGGING
#define WFC_LOG_LEVEL VCOS_LOG_TRACE
#else
#define WFC_LOG_LEVEL VCOS_LOG_WARN
#endif

//==============================================================================

//!@name Stream data block pool sizes
//!@{
#define WFC_STREAM_BLOCK_SIZE          (WFC_MAX_STREAMS_PER_CLIENT / 8)
#define WFC_STREAM_MAX_EXTENSIONS      7
#define WFC_STREAM_MAX_STREAMS         (WFC_STREAM_BLOCK_SIZE * (WFC_STREAM_MAX_EXTENSIONS + 1))
//!@}

//!@name Global lock to protect global data (stream data list, next stream ID)
//!@{
#define GLOBAL_LOCK()      do {vcos_once(&wfc_stream_initialise_once, wfc_stream_initialise); vcos_mutex_lock(&wfc_stream_global_lock);} while (0)
#define GLOBAL_UNLOCK()    do {vcos_mutex_unlock(&wfc_stream_global_lock);} while (0)
//!@}

//!@name Stream-specific mutex. Global lock must already be held when acquiring this lock.
//!@{
#define STREAM_LOCK(stream_ptr)        do {vcos_mutex_lock(&stream_ptr->mutex);} while (0)
#define STREAM_UNLOCK(stream_ptr)      do {vcos_mutex_unlock(&stream_ptr->mutex);} while (0)
//!@}

//! Period in milliseconds to wait for an existing stream handle to be released
//! when creating a new one.
#define WFC_STREAM_RETRY_DELAY_MS      1
//! Number of attempts allowed to create a stream with a given handle.
#define WFC_STREAM_RETRIES             50

//==============================================================================

//! Top-level stream type
typedef struct WFC_STREAM_tag
{
   //! Handle; may be assigned by window manager.
   WFCNativeStreamType handle;

   //! Number of times this stream has been registered in the process. Creation implies registration
   uint32_t registrations;

   //! Flag to indicate entry is no longer in use and imminently due for destruction.
   bool to_be_deleted;

   //! Mutex, for thread safety.
   VCOS_MUTEX_T mutex;

   //! Configuration info.
   WFC_STREAM_INFO_T info;

   //!@brief Image providers to which this stream sends data; recorded so we do
   //! not destroy this stream if it is still associated with a source or mask.
   uint32_t num_of_sources_or_masks;
   //! Record if this stream holds the output from an off-screen context.
   bool used_for_off_screen;

   //! Thread for handling server-side request to change source and/or destination rectangles
   VCOS_THREAD_T rect_req_thread_data;
   //! Flag for when thread must terminate
   bool rect_req_terminate;
   //! Callback function notifying calling module
   WFC_STREAM_REQ_RECT_CALLBACK_T req_rect_callback;
   //! Argument to callback function
   void *req_rect_cb_args;

   //! Pointer to next stream
   struct WFC_STREAM_tag *next;
   //! Pointer to previous stream
   struct WFC_STREAM_tag *prev;
} WFC_STREAM_T;

//==============================================================================

//! Blockpool containing all created streams.
static VCOS_BLOCKPOOL_T wfc_stream_blockpool;
//! Next stream handle, allocated by wfc_stream_get_next().
static WFCNativeStreamType wfc_stream_next_handle = (1 << 31);

static VCOS_LOG_CAT_T log_cat = VCOS_LOG_INIT("wfc_client_stream", WFC_LOG_LEVEL);

//! Ensure lock and blockpool are only initialised once
static VCOS_ONCE_T wfc_stream_initialise_once;
//! The global (process-wide) lock
static VCOS_MUTEX_T wfc_stream_global_lock;
//! Pointer to the first stream data block
static WFC_STREAM_T *wfc_stream_head;

//==============================================================================
//!@name Static functions
//!@{
static void wfc_stream_initialise(void);
static WFC_STREAM_T *wfc_stream_global_lock_and_find_stream_ptr(WFCNativeStreamType stream);
static WFC_STREAM_T *wfc_stream_create_stream_ptr(WFCNativeStreamType stream, bool allow_duplicate);
static WFC_STREAM_T *wfc_stream_find_stream_ptr(WFCNativeStreamType stream);
static void wfc_stream_destroy_if_ready(WFC_STREAM_T *stream_ptr);
static void *wfc_stream_rect_req_thread(void *arg);
static void wfc_client_stream_post_sem(void *cb_data);
//!@}
//==============================================================================
//!@name Public functions
//!@{

WFCNativeStreamType wfc_stream_get_next(void)
// In cases where the caller doesn't want to assign a stream number, provide
// one for it.
{
   GLOBAL_LOCK();

   WFCNativeStreamType next_stream = wfc_stream_next_handle;
   wfc_stream_next_handle++;

   GLOBAL_UNLOCK();

   return next_stream;
}

//------------------------------------------------------------------------------

uint32_t wfc_stream_create(WFCNativeStreamType stream, uint32_t flags)
// Create a stream, using the given stream handle (typically assigned by the
// window manager). Return zero if OK.
{
   WFC_STREAM_T *stream_ptr;
   uint32_t result = 0;

   vcos_log_info("%s: stream 0x%x flags 0x%x", VCOS_FUNCTION, stream, flags);

   // Create stream
   stream_ptr = wfc_stream_create_stream_ptr(stream, false);
   if(stream_ptr == NULL)
   {
      vcos_log_error("%s: unable to create data block for stream 0x%x", VCOS_FUNCTION, stream);
      return VCOS_ENOMEM;
   }

   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t) pid;
   uint32_t pid_hi = (uint32_t) (pid >> 32);
   int stream_in_use_retries = WFC_STREAM_RETRIES;
   WFC_STREAM_INFO_T info;

   memset(&info, 0, sizeof(info));
   info.size = sizeof(info);
   info.flags = flags;

   do
   {
      stream_ptr->handle = wfc_server_stream_create_info(stream, &info, pid_lo, pid_hi);
      vcos_log_trace("%s: server create returned 0x%x", VCOS_FUNCTION, stream_ptr->handle);

      // If a handle is re-used rapidly, it may still be in use in the server temporarily
      // Retry after a short delay
      if (stream_ptr->handle == WFC_INVALID_HANDLE)
         vcos_sleep(WFC_STREAM_RETRY_DELAY_MS);
   }
   while (stream_ptr->handle == WFC_INVALID_HANDLE && stream_in_use_retries-- > 0);

   if (stream_ptr->handle == WFC_INVALID_HANDLE)
   {
      // Even after the retries, stream handle was still in use. Fail.
      vcos_log_error("%s: stream 0x%x already exists in server", VCOS_FUNCTION, stream);
      result = VCOS_EEXIST;
      wfc_stream_destroy_if_ready(stream_ptr);
   }
   else
   {
      vcos_assert(stream_ptr->handle == stream);

      stream_ptr->registrations++;
      stream_ptr->info.flags = flags;
      STREAM_UNLOCK(stream_ptr);
   }

   return result;
}

//------------------------------------------------------------------------------

WFCNativeStreamType wfc_stream_create_assign_id(uint32_t flags)
// Create a stream, and automatically assign it a new stream number, which is returned
{
   WFCNativeStreamType stream = wfc_stream_get_next();
   uint32_t failure = wfc_stream_create(stream, flags);

   if (failure == VCOS_EEXIST)
   {
      // If a duplicate stream exists, give it one more go with a new ID
      stream = wfc_stream_get_next();
      failure = wfc_stream_create(stream, flags);
   }

   if(failure) {return WFC_INVALID_HANDLE;}
   else {return stream;}
}

//------------------------------------------------------------------------------

uint32_t wfc_stream_create_req_rect
   (WFCNativeStreamType stream, uint32_t flags,
      WFC_STREAM_REQ_RECT_CALLBACK_T callback, void *cb_args)
// Create a stream, using the given stream handle, which will notify the calling
// module when the server requests a change in source and/or destination rectangle,
// using the supplied callback. Return zero if OK.
{
   vcos_log_info("wfc_stream_create_req_rect: stream %X", stream);

   uint32_t failure;

   failure = wfc_stream_create(stream, flags | WFC_STREAM_FLAGS_REQ_RECT);
   if (failure)
      return failure;

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);
   // Stream just created, so ought to be found
   vcos_assert(stream_ptr);

   // There's no point creating this type of stream if you don't supply a callback
   // to update the src/dest rects via WF-C.
   vcos_assert(callback != NULL);

   stream_ptr->req_rect_callback = callback;
   stream_ptr->req_rect_cb_args = cb_args;

   // Create thread for handling server-side request to change source
   // and/or destination rectangles. One per stream (if enabled).
   VCOS_STATUS_T status = vcos_thread_create(&stream_ptr->rect_req_thread_data, "wfc_stream_rect_req_thread",
      NULL, wfc_stream_rect_req_thread, (void *) stream);
   vcos_demand(status == VCOS_SUCCESS);

   STREAM_UNLOCK(stream_ptr);

   return 0;
}

//------------------------------------------------------------------------------

bool wfc_stream_register_source_or_mask(WFCNativeStreamType stream, bool add_source_or_mask)
// Indicate that a source or mask is now associated with this stream, or should
// now be removed from such an association.
{
   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);

   if (!stream_ptr)
      return false;

   vcos_log_trace("%s: stream 0x%x %d->%d", VCOS_FUNCTION, stream,
         stream_ptr->num_of_sources_or_masks,
         add_source_or_mask ? stream_ptr->num_of_sources_or_masks + 1 : stream_ptr->num_of_sources_or_masks - 1);

   if(add_source_or_mask)
   {
      stream_ptr->num_of_sources_or_masks++;
      STREAM_UNLOCK(stream_ptr);
   }
   else
   {
      if(vcos_verify(stream_ptr->num_of_sources_or_masks > 0))
         {stream_ptr->num_of_sources_or_masks--;}

      // Stream is unlocked by destroy_if_ready
      wfc_stream_destroy_if_ready(stream_ptr);
   }

   return true;
}

//------------------------------------------------------------------------------

void wfc_stream_await_buffer(WFCNativeStreamType stream)
// Suspend until buffer is available on the server.
{
   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);
   if (!stream_ptr)
      return;

   if(vcos_verify(stream_ptr->info.flags & WFC_STREAM_FLAGS_BUF_AVAIL))
   {
      VCOS_SEMAPHORE_T image_available_sem;
      VCOS_STATUS_T status;

      // Long running operation, so keep VC alive until it completes.
      wfc_server_use_keep_alive();

      status = vcos_semaphore_create(&image_available_sem, "WFC image available", 0);
      vcos_assert(status == VCOS_SUCCESS);      // For all relevant platforms
      vcos_unused(status);

      wfc_server_stream_on_image_available(stream, wfc_client_stream_post_sem, &image_available_sem);

      vcos_log_trace("%s: pre async sem wait: stream: %X", VCOS_FUNCTION, stream);
      vcos_semaphore_wait(&image_available_sem);
      vcos_log_trace("%s: post async sem wait: stream: %X", VCOS_FUNCTION, stream);

      vcos_semaphore_delete(&image_available_sem);
      wfc_server_release_keep_alive();
   }

   STREAM_UNLOCK(stream_ptr);

}

//------------------------------------------------------------------------------

void wfc_stream_destroy(WFCNativeStreamType stream)
// Destroy a stream - unless it is still in use, in which case, mark it for
// destruction once all users have finished with it.
{
   vcos_log_info("%s: stream: %X", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);

   if (stream_ptr)
   {
      /* If stream is still in use (e.g. it's attached to at least one source/mask
       * which is associated with at least one element) then destruction is delayed
       * until it's no longer in use. */
      if (stream_ptr->registrations> 0)
      {
         stream_ptr->registrations--;
         vcos_log_trace("%s: stream: %X ready to destroy?", VCOS_FUNCTION, stream);
      }
      else
      {
         vcos_log_error("%s: stream: %X destroyed when unregistered", VCOS_FUNCTION, stream);
      }

      // Stream is unlocked by destroy_if_ready
      wfc_stream_destroy_if_ready(stream_ptr);
   }
   else
   {
      vcos_log_warn("%s: stream %X doesn't exist", VCOS_FUNCTION, stream);
   }

}

//------------------------------------------------------------------------------
//!@name
//! Off-screen composition functions
//!@{
//------------------------------------------------------------------------------

uint32_t wfc_stream_create_for_context
   (WFCNativeStreamType stream, uint32_t width, uint32_t height)
// Create a stream for an off-screen context to output to, with the default number of buffers.
{
   return wfc_stream_create_for_context_nbufs(stream, width, height, 0);
}

uint32_t wfc_stream_create_for_context_nbufs
   (WFCNativeStreamType stream, uint32_t width, uint32_t height, uint32_t nbufs)
// Create a stream for an off-screen context to output to, with a specific number of buffers.
{
   WFC_STREAM_T *stream_ptr;
   bool stream_created = false;

   if(!vcos_verify(stream != WFC_INVALID_HANDLE))
      {return 1;}

   stream_ptr = wfc_stream_find_stream_ptr(stream);
   if (stream_ptr)
   {
      uint32_t flags = stream_ptr->info.flags;

      // Stream already exists, check flags match expected
      STREAM_UNLOCK(stream_ptr);

      if (flags != WFC_STREAM_FLAGS_NONE)
      {
         vcos_log_error("%s: stream flags mismatch (expected 0x%x, got 0x%x)", VCOS_FUNCTION, WFC_STREAM_FLAGS_NONE, flags);
         return 1;
      }
   }
   else
   {
      // Create stream
      if (wfc_stream_create(stream, WFC_STREAM_FLAGS_NONE) != 0)
         return 1;
      stream_created = true;
   }

   // Allocate buffers on the server.
   if (!wfc_server_stream_allocate_images(stream, width, height, nbufs))
   {
      // Failed to allocate buffers
      vcos_log_warn("%s: failed to allocate %u buffers for stream %X size %ux%u", VCOS_FUNCTION, nbufs, stream, width, height);
      if (stream_created)
         wfc_stream_destroy(stream);
      return 1;
   }

   return 0;
}

//------------------------------------------------------------------------------

bool wfc_stream_used_for_off_screen(WFCNativeStreamType stream)
// Returns true if this stream exists, and is in use as the output of an
// off-screen context.
{
   bool used_for_off_screen;

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);
   if (!stream_ptr)
      return false;

   used_for_off_screen = stream_ptr->used_for_off_screen;

   STREAM_UNLOCK(stream_ptr);

   return used_for_off_screen;

}

//------------------------------------------------------------------------------

void wfc_stream_register_off_screen(WFCNativeStreamType stream, bool used_for_off_screen)
// Called on behalf of an off-screen context, to either set or clear the stream's
// flag indicating that it's being used as output for that context.
{
   if(stream == WFC_INVALID_HANDLE)
      {return;}

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);
   if (!stream_ptr)
      return;

   stream_ptr->used_for_off_screen = used_for_off_screen;

   if (used_for_off_screen)
      STREAM_UNLOCK(stream_ptr);
   else
   {
      // Stream is unlocked by destroy_if_ready
      wfc_stream_destroy_if_ready(stream_ptr);
   }
}

//!@} // Off-screen composition functions
//!@} // Public functions
//==============================================================================

/** Initialise logging and global mutex */
static void wfc_stream_initialise(void)
{
   VCOS_STATUS_T status;

   vcos_log_set_level(&log_cat, WFC_LOG_LEVEL);
   vcos_log_register("wfc_client_stream", &log_cat);

   vcos_log_trace("%s", VCOS_FUNCTION);

   status = vcos_mutex_create(&wfc_stream_global_lock, "WFC stream global lock");
   vcos_assert(status == VCOS_SUCCESS);

   status = vcos_blockpool_create_on_heap(&wfc_stream_blockpool,
         WFC_STREAM_BLOCK_SIZE, sizeof(WFC_STREAM_T),
         VCOS_BLOCKPOOL_ALIGN_DEFAULT, VCOS_BLOCKPOOL_FLAG_NONE,
         "wfc stream pool");
   vcos_assert(status == VCOS_SUCCESS);

   status = vcos_blockpool_extend(&wfc_stream_blockpool,
         WFC_STREAM_MAX_EXTENSIONS, WFC_STREAM_BLOCK_SIZE);
   vcos_assert(status == VCOS_SUCCESS);
}

//------------------------------------------------------------------------------

/** Take the global lock and then search for the stream data for a given handle.
 * The global lock is not released on return and the stream is not locked.
 *
 * @param stream The stream handle.
 * @return The pointer to the stream structure, or NULL if not found.
 */
static WFC_STREAM_T *wfc_stream_global_lock_and_find_stream_ptr(WFCNativeStreamType stream)
{
   WFC_STREAM_T *stream_ptr;

   GLOBAL_LOCK();

   stream_ptr = wfc_stream_head;
   while (stream_ptr && stream_ptr->handle != stream)
      stream_ptr = stream_ptr->next;

   return stream_ptr;
}

//------------------------------------------------------------------------------

/** Create a stream structure corresponding to the specified stream handle. If
 * the stream structure already exists or there is an error allocating it, the
 * function returns NULL. On success, the stream pointer is left locked.
 *
 * @param stream The stream handle.
 * @param allow_duplicate True to allow an existing entry
 * @return The pointer to the new stream structure, or NULL on error.
 */
static WFC_STREAM_T *wfc_stream_create_stream_ptr(WFCNativeStreamType stream, bool allow_duplicate)
{
   WFC_STREAM_T *stream_ptr = wfc_stream_global_lock_and_find_stream_ptr(stream);

   vcos_log_trace("%s: stream handle 0x%x", VCOS_FUNCTION, stream);

   if (stream_ptr && !stream_ptr->to_be_deleted)
   {
      if (!allow_duplicate)
      {
         vcos_log_error("%s: attempt to create duplicate of stream handle 0x%x", VCOS_FUNCTION, stream);
         // Stream already exists, return NULL
         stream_ptr = NULL;
      }
      else
      {
         vcos_log_trace("%s: duplicate of stream handle 0x%x created", VCOS_FUNCTION, stream);

         STREAM_LOCK(stream_ptr);
      }
   }
   else
   {
      if (stream_ptr)
      {
         vcos_log_trace("%s: recycling data block for stream handle 0x%x", VCOS_FUNCTION, stream);

         // Recycle existing entry
         stream_ptr->to_be_deleted = false;

         STREAM_LOCK(stream_ptr);
      }
      else
      {
         vcos_log_trace("%s: allocating block for stream handle 0x%x", VCOS_FUNCTION, stream);

         // Create new block and insert it into the list
         stream_ptr = vcos_blockpool_calloc(&wfc_stream_blockpool);

         if (stream_ptr)
         {
            VCOS_STATUS_T status;

            status = vcos_mutex_create(&stream_ptr->mutex, "WFC_STREAM_T mutex");
            if (vcos_verify(status == VCOS_SUCCESS))
            {
               STREAM_LOCK(stream_ptr);

               // First stream in this process, connect
               if (!wfc_stream_head)
                  wfc_server_connect();

               stream_ptr->handle = stream;
               stream_ptr->info.size = sizeof(stream_ptr->info);

               // Insert data into list
               stream_ptr->next = wfc_stream_head;
               if (wfc_stream_head)
                  wfc_stream_head->prev = stream_ptr;
               wfc_stream_head = stream_ptr;
            }
            else
            {
               vcos_log_error("%s: unable to create mutex for stream handle 0x%x", VCOS_FUNCTION, stream);
               vcos_blockpool_free(stream_ptr);
               stream_ptr = NULL;
            }
         }
         else
         {
            vcos_log_error("%s: unable to allocate data for stream handle 0x%x", VCOS_FUNCTION, stream);
         }
      }
   }

   GLOBAL_UNLOCK();

   return stream_ptr;
}

//------------------------------------------------------------------------------

/** Destroys a stream structure identified by stream handle. If the stream is not
 * found or the stream has not been marked for deletion, the operation has no
 * effect.
 *
 * @param stream The stream handle.
 */
static void wfc_stream_destroy_stream_ptr(WFCNativeStreamType stream)
{
   WFC_STREAM_T *stream_ptr = wfc_stream_global_lock_and_find_stream_ptr(stream);

   vcos_log_trace("%s: stream handle 0x%x", VCOS_FUNCTION, stream);

   if (stream_ptr)
   {
      if (stream_ptr->to_be_deleted)
      {
         STREAM_LOCK(stream_ptr);

         vcos_log_trace("%s: unlinking from list", VCOS_FUNCTION);

         if (stream_ptr->next)
            stream_ptr->next->prev = stream_ptr->prev;
         if (stream_ptr->prev)
            stream_ptr->prev->next = stream_ptr->next;
         else
            wfc_stream_head = stream_ptr->next;

         // No streams left in this process, disconnect
         if (wfc_stream_head == NULL)
            wfc_server_disconnect();
      }
      else
      {
         vcos_log_trace("%s: stream 0x%x recycled before destruction", VCOS_FUNCTION, stream);
         stream_ptr = NULL;
      }
   }
   else
   {
      vcos_log_error("%s: stream 0x%x not found", VCOS_FUNCTION, stream);
   }

   GLOBAL_UNLOCK();

   if (stream_ptr)
   {
      // Stream data block no longer in list, can safely destroy it
      STREAM_UNLOCK(stream_ptr);

      // Wait for rectangle request thread to complete
      if(stream_ptr->info.flags & WFC_STREAM_FLAGS_REQ_RECT)
         vcos_thread_join(&stream_ptr->rect_req_thread_data, NULL);

      // Destroy mutex
      vcos_mutex_delete(&stream_ptr->mutex);

      // Delete
      vcos_blockpool_free(stream_ptr);
   }
}

//------------------------------------------------------------------------------

/** Return a pointer to the stream structure corresponding to the specified stream
 * handle. On success, the stream pointer is locked.
 *
 * @param stream The stream handle.
 * @return The pointer to the stream structure, or NULL on error.
 */
static WFC_STREAM_T *wfc_stream_find_stream_ptr(WFCNativeStreamType stream)
{
   WFC_STREAM_T *stream_ptr = wfc_stream_global_lock_and_find_stream_ptr(stream);

   if (stream_ptr && !stream_ptr->to_be_deleted)
      STREAM_LOCK(stream_ptr);

   GLOBAL_UNLOCK();

   return stream_ptr;
}

//------------------------------------------------------------------------------

/** Destroy the stream if it is no longer in use. The stream must be locked on
 * entry and shall be unlocked (or destroyed along with the rest of the stream)
 * on exit.
 *
 * @param stream_ptr The locked stream data pointer.
 */
static void wfc_stream_destroy_if_ready(WFC_STREAM_T *stream_ptr)
{
   WFCNativeStreamType stream;
   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t)pid;
   uint32_t pid_hi = (uint32_t)(pid >> 32);

   if (stream_ptr == NULL)
   {
      vcos_log_error("%s: stream_ptr is NULL", VCOS_FUNCTION);
      return;
   }

   if(stream_ptr->num_of_sources_or_masks > 0
      || stream_ptr->used_for_off_screen
      || stream_ptr->registrations > 0)
   {
      vcos_log_trace("%s: stream: %X not ready: reg:%u srcs:%u o/s:%d", VCOS_FUNCTION,
            stream_ptr->handle, stream_ptr->registrations,
            stream_ptr->num_of_sources_or_masks, stream_ptr->used_for_off_screen);
      STREAM_UNLOCK(stream_ptr);
      return;
   }

   stream = stream_ptr->handle;

   vcos_log_info("%s: stream: %X to be destroyed", VCOS_FUNCTION, stream);

   // Prevent stream from being found, although it can be recycled.
   stream_ptr->to_be_deleted = true;

   // Delete server-side stream
   wfc_server_stream_destroy(stream, pid_lo, pid_hi);

   STREAM_UNLOCK(stream_ptr);

   wfc_stream_destroy_stream_ptr(stream);
}

//------------------------------------------------------------------------------

//! Convert from dispmanx source rectangle type (int * 2^16) to WF-C type (float).
#define WFC_DISPMANX_TO_SRC_VAL(value) (((WFCfloat) (value)) / 65536.0)

static void *wfc_stream_rect_req_thread(void *arg)
//!@brief Thread for handling server-side request to change source and/or destination
//! rectangles. One per stream (if enabled).
{
   WFCNativeStreamType stream = (WFCNativeStreamType) arg;

   WFC_STREAM_REQ_RECT_CALLBACK_T callback;
   void *cb_args;
   VCOS_SEMAPHORE_T rect_req_sem;
   VCOS_STATUS_T status;

   int32_t  vc_rects[8];
   WFCint   dest_rect[WFC_RECT_SIZE];
   WFCfloat src_rect[WFC_RECT_SIZE];

   vcos_log_info("wfc_stream_rect_req_thread: START: stream: %X", stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);
   if (!stream_ptr)
      return NULL;

   // Get local pointers to stream parameters
   callback = stream_ptr->req_rect_callback;
   cb_args = stream_ptr->req_rect_cb_args;

   STREAM_UNLOCK(stream_ptr);

   status = vcos_semaphore_create(&rect_req_sem, "WFC rect req", 0);
   vcos_assert(status == VCOS_SUCCESS);      // On all relevant platforms

   while (status == VCOS_SUCCESS)
   {
      wfc_server_stream_on_rects_change(stream, wfc_client_stream_post_sem, &rect_req_sem);

      // Await new values from server
      vcos_semaphore_wait(&rect_req_sem);

      status = wfc_server_stream_get_rects(stream, vc_rects);
      if (status == VCOS_SUCCESS)
      {
         // Convert from VC/dispmanx to WF-C types.
         vcos_static_assert(sizeof(dest_rect) == (4 * sizeof(int32_t)));
         memcpy(dest_rect, vc_rects, sizeof(dest_rect)); // Types are equivalent

         src_rect[WFC_RECT_X] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[4]);
         src_rect[WFC_RECT_Y] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[5]);
         src_rect[WFC_RECT_WIDTH] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[6]);
         src_rect[WFC_RECT_HEIGHT] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[7]);

         callback(cb_args, dest_rect, src_rect);
      }
   }

   vcos_semaphore_delete(&rect_req_sem);

   vcos_log_info("wfc_stream_rect_req_thread: END: stream: %X", stream);

   return NULL;
}

//------------------------------------------------------------------------------

static void wfc_client_stream_post_sem(void *cb_data)
{
   VCOS_SEMAPHORE_T *sem = (VCOS_SEMAPHORE_T *)cb_data;

   vcos_log_trace("%s: sem %p", VCOS_FUNCTION, sem);
   vcos_assert(sem != NULL);
   vcos_semaphore_post(sem);
}

//==============================================================================
#ifdef KHRN_FRUIT_DIRECT
static KHRN_UMEM_HANDLE_T get_ustorage(EGLImageKHR im, KHRN_DEPS_T *deps)
{
   KHRN_UMEM_HANDLE_T ret = KHRN_UMEM_HANDLE_INVALID;
   EGL_SERVER_STATE_T *state = EGL_GET_SERVER_STATE();
   EGL_IMAGE_T *eglimage_;
   KHRN_IMAGE_T *image;

   KHRN_MEM_HANDLE_T eglimage =
      khrn_map_lookup(&state->eglimages, (uint32_t) im);

   if (eglimage == KHRN_MEM_HANDLE_INVALID) {
      vcos_log_error("Bad image %p", im);
      goto end;
   }

   eglimage_ = khrn_mem_lock(eglimage);
   vcos_assert(eglimage_->external.src != KHRN_UMEM_HANDLE_INVALID);
   image = khrn_mem_lock(eglimage_->external.src);

   /* FIXME: We probably don't need this. It doesn't make any sense */
   khrn_deps_quick_write(deps, &image->interlock);

   ret = image->ustorage;

   khrn_mem_unlock(eglimage_->external.src);
   khrn_mem_unlock(eglimage);

end:
   return ret;
}

void wfc_stream_signal_eglimage_data(WFCNativeStreamType stream, EGLImageKHR im)
{
   wfc_stream_signal_eglimage_data_protected(stream, im, 0);
}

#ifdef ANDROID
void wfc_stream_signal_eglimage_data_protected(WFCNativeStreamType stream, EGLImageKHR im, uint32_t is_protected)
{
   EGL_SERVER_STATE_T *state = EGL_GET_SERVER_STATE();
   KHRN_DEPS_T deps;
   KHRN_MEM_HANDLE_T eglimage;
   EGL_IMAGE_T *eglimage_;
   KHRN_IMAGE_T *image_;

   CLIENT_LOCK();

   khrn_deps_init(&deps);

   eglimage = khrn_map_lookup(&state->eglimages, (uint32_t)im);
   eglimage_ = khrn_mem_lock(eglimage);
   image_ = khrn_mem_lock(eglimage_->external.src);

   vcos_assert(image_->ustorage != KHRN_UMEM_HANDLE_INVALID);
   khrn_umem_acquire(&deps, image_->ustorage);

   image_->flags &= ~IMAGE_FLAG_PROTECTED;
   if (is_protected)
      image_->flags |= IMAGE_FLAG_PROTECTED;

   khrn_signal_image_data((uint32_t)stream, image_->ustorage, image_->width, image_->height, image_->stride, image_->offset,
      image_->format, image_->flags, eglimage_->flip_y ? true : false);

   khrn_mem_unlock(eglimage_->external.src);
   khrn_mem_unlock(eglimage);

   CLIENT_UNLOCK();
}
#else
void wfc_stream_signal_eglimage_data_protected(WFCNativeStreamType stream, EGLImageKHR im, uint32_t is_protected)
{
   EGL_SERVER_STATE_T *state = EGL_GET_SERVER_STATE();
   KHRN_DEPS_T deps;
   KHRN_MEM_HANDLE_T eglimage;
   EGL_IMAGE_T *eglimage_;
   KHRN_IMAGE_T *image_;
   WFC_STREAM_IMAGE_T stream_image;

   CLIENT_LOCK();

   khrn_deps_init(&deps);

   eglimage = khrn_map_lookup(&state->eglimages, (uint32_t)im);
   eglimage_ = khrn_mem_lock(eglimage);
   image_ = khrn_mem_lock(eglimage_->external.src);

   vcos_assert(image_->ustorage != KHRN_UMEM_HANDLE_INVALID);
   khrn_umem_acquire(&deps, image_->ustorage);

   /* The EGL protection flag is passed through the KHRN_IMAGE_T flags field */
   image_->flags &= ~IMAGE_FLAG_PROTECTED;
   if (is_protected)
      image_->flags |= IMAGE_FLAG_PROTECTED;

   memset(&stream_image, 0, sizeof(stream_image));
   stream_image.length = sizeof(stream_image);
   stream_image.type = WFC_STREAM_IMAGE_TYPE_EGL;

   stream_image.handle = image_->ustorage;
   stream_image.width = image_->width;
   stream_image.height = image_->height;
   stream_image.format = image_->format;
   stream_image.pitch = image_->stride;
   stream_image.offset = image_->offset;
   stream_image.flags = image_->flags;
   stream_image.flip = eglimage_->flip_y ? WFC_STREAM_IMAGE_FLIP_VERT : WFC_STREAM_IMAGE_FLIP_NONE;

   khrn_mem_unlock(eglimage_->external.src);
   khrn_mem_unlock(eglimage);

   CLIENT_UNLOCK();

   wfc_server_stream_signal_image(stream, &stream_image);
}
#endif

void wfc_stream_release_eglimage_data(WFCNativeStreamType stream,
      EGLImageKHR im)
{
   KHRN_DEPS_T deps;
   KHRN_UMEM_HANDLE_T ustorage;

   CLIENT_LOCK();
   ustorage = get_ustorage(im, &deps);
   khrn_umem_release(&deps, ustorage);
   CLIENT_UNLOCK();
}
#endif

void wfc_stream_signal_mm_image_data(WFCNativeStreamType stream, uint32_t im)
{
   wfc_server_stream_signal_mm_image_data(stream, im);
}

void wfc_stream_signal_raw_pixels(WFCNativeStreamType stream, uint32_t handle,
      uint32_t format, uint32_t w, uint32_t h, uint32_t pitch, uint32_t vpitch)
{
   wfc_server_stream_signal_raw_pixels(stream, handle, format, w, h, pitch, vpitch);
}

void wfc_stream_signal_image(WFCNativeStreamType stream,
      const WFC_STREAM_IMAGE_T *image)
{
   wfc_server_stream_signal_image(stream, image);
}

void wfc_stream_register(WFCNativeStreamType stream) {
   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t)pid;
   uint32_t pid_hi = (uint32_t)(pid >> 32);

   if (wfc_server_connect() == VCOS_SUCCESS)
   {
      WFC_STREAM_INFO_T info;
      uint32_t status;

      info.size = sizeof(info);
      status = wfc_server_stream_get_info(stream, &info);

      if (status == VCOS_SUCCESS)
      {
         WFC_STREAM_T *stream_ptr = wfc_stream_create_stream_ptr(stream, true);

         if (stream_ptr)
         {
            stream_ptr->registrations++;
            memcpy(&stream_ptr->info, &info, info.size);
            STREAM_UNLOCK(stream_ptr);
         }

         wfc_server_stream_register(stream, pid_lo, pid_hi);
      }
      else
      {
         vcos_log_error("%s: get stream info failed: %u", VCOS_FUNCTION, status);
      }
   }
}

void wfc_stream_unregister(WFCNativeStreamType stream) {
   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t)pid;
   uint32_t pid_hi = (uint32_t)(pid >> 32);
   WFC_STREAM_T *stream_ptr = wfc_stream_find_stream_ptr(stream);

   if (vcos_verify(stream_ptr != NULL))
   {
      wfc_server_stream_unregister(stream, pid_lo, pid_hi);

      if (stream_ptr->registrations > 0)
      {
         stream_ptr->registrations--;
         vcos_log_trace("%s: stream %X", VCOS_FUNCTION, stream);
      }
      else
      {
         vcos_log_error("%s: stream %X already fully unregistered", VCOS_FUNCTION, stream);
      }

      wfc_stream_destroy_if_ready(stream_ptr);
   }

   wfc_server_disconnect();
}

//==============================================================================
