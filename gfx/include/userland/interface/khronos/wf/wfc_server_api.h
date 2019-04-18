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

#if !defined(WFC_SERVER_API_H)
#define WFC_SERVER_API_H

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_stdbool.h"
#include "interface/khronos/include/WF/wfc.h"
#include "interface/khronos/wf/wfc_int.h"
#include "interface/khronos/include/EGL/eglext.h"

/** Defining WFC_FULL_LOGGING will enable trace level logging across all WFC
 * components.
#define WFC_FULL_LOGGING
 */

/** General purpose callback function.
 * Note: callbacks are often made on a thread other than the original function's.
 *
 * @param cb_data Callback additional data.
 */
typedef void (*WFC_CALLBACK_T)(void *cb_data);

/** Extensible stream information block, supplied during creation and
 * retrievable using wfc_server_stream_get_info. */
typedef struct WFC_STREAM_INFO_T
{
   uint32_t size;    /**< Size of the strucuture, for versioning. */
   uint32_t flags;   /**< Stream flags. */
} WFC_STREAM_INFO_T;

/** Create a connection to the server, as necessary. If this fails, no other
 * server functions should be called. If it succeeds, there must be one call to
 * wfc_server_disconnect() when use of the server has ended.
 *
 * @return VCOS_SUCCESS or a failure code.
 */
VCOS_STATUS_T wfc_server_connect(void);

/** Destroy a connection to the server. This must be called once and only once
 * for each successful call to wfc_server_connect().
 */
void wfc_server_disconnect(void);

/** Increase the keep alive count by one. If it rises from zero, the VideoCore
 * will be prevented from being suspended.
 */
void wfc_server_use_keep_alive(void);

/** Drop the keep alive count by one. If it reaches zero, the VideoCore may be
 * suspended.
 */
void wfc_server_release_keep_alive(void);

/** Create a new on- or off-screen context for composition. The type of the context
 * is either WFC_CONTEXT_TYPE_ON_SCREEN or WFC_CONTEXT_TYPE_OFF_SCREEN.
 *
 * @param context The client context identifier.
 * @param context_type The type of the context.
 * @param screen_or_stream_num The screen number for and on-screen context, or the
 *    target client stream identifier for an off-screen context.
 * @param pid_lo The low 32-bits of the owning client process identifier.
 * @param pid_hi The high 32-bits of the owning client process identifier.
 * @return Zero on failure, or the width and height of the context in pixels,
 *    packed in the top and bottom 16 bits, respectively.
 */
uint32_t wfc_server_create_context(WFCContext context, uint32_t context_type,
   uint32_t screen_or_stream_num, uint32_t pid_lo, uint32_t pid_hi);

/** Destroy a context.
 *
 * @param context The client context identifier.
 */
void wfc_server_destroy_context(WFCContext ctx);

/** Flags to be passed to wfc_server_commit_scene. A bitwise combination of
 * flags may be passed in to control behaviour.
 */
typedef enum
{
   WFC_SERVER_COMMIT_WAIT     = 1,  /**< Wait for scene to be committed. */
   WFC_SERVER_COMMIT_COMPOSE  = 2,  /**< Trigger composition */
} WFC_SERVER_COMMIT_FLAGS_T;

/** Commit a scene in the context. If the WAIT flag is set, a callback
 * must be given and shall be called once a new scene can be given. If the
 * COMPOSE flag is set, the newly committed scene shall be composed
 * as soon as possible (subject to presence of a deferral stream).
 *
 * @param context The client context identifier.
 * @param scene The scene to be composed.
 * @param flags Combination of WFC_SERVER_COMMIT_FLAGS_T values, or 0.
 * @param scene_taken_cb Called when scene has been taken, generally on a
 *    different thread, or zero for no callback.
 * @param scene_taken_data Passed to scene_taken_cb.
 * @return VCOS_SUCCESS if successful, VCOS_EAGAIN if the scene cannot be
 *    taken immediately and the wait flag is false.
 */
uint32_t wfc_server_commit_scene(WFCContext ctx, const WFC_SCENE_T *scene,
      uint32_t flags, WFC_CALLBACK_T scene_taken_cb, void *scene_taken_data);

/** Activate a context. While a context is active, it will be automatically
 * composed when any of the streams in its current scene are updated.
 *
 * @param context The client context identifier.
 */
void wfc_server_activate(WFCContext ctx);

/** Deactivate a context.
 *
 * @param context The client context identifier.
 */
void wfc_server_deactivate(WFCContext ctx);

/** Set the deferral stream for a context. When a deferral stream is set on a
 * context and a new scene is given for that context, composition is deferred
 * until the stream is updated.
 *
 * @param context The client context identifier.
 * @param stream The deferral stream, or WFC_INVALID_HANDLE for none.
 */
void wfc_server_set_deferral_stream(WFCContext ctx, WFCNativeStreamType stream);

/** Create a stream and its buffers, for supplying pixel data to one or more
 * elements, and optionally for storing the output of off-screen composition.
 *
 * @param stream The client stream identifier.
 * @param flags Stream flags.
 * @param pid_lo The low 32-bits of the owning client process identifier.
 * @param pid_hi The high 32-bits of the owning client process identifier.
 * @return The client stream identifier on success, or WFC_INVALID_HANDLE on error.
 */
WFCNativeStreamType wfc_server_stream_create(
   WFCNativeStreamType stream, uint32_t flags, uint32_t pid_lo, uint32_t pid_hi);

/** Create a stream and its buffers, for supplying pixel data to one or more
 * elements, and optionally for storing the output of off-screen composition.
 *
 * @param stream The client stream identifier.
 * @param info Stream configuration.
 * @param pid_lo The low 32-bits of the owning client process identifier.
 * @param pid_hi The high 32-bits of the owning client process identifier.
 * @return The client stream identifier on success, or WFC_INVALID_HANDLE on error.
 */
WFCNativeStreamType wfc_server_stream_create_info(
   WFCNativeStreamType stream, const WFC_STREAM_INFO_T *info, uint32_t pid_lo, uint32_t pid_hi);

/** Destroy a stream.
 *
 * @param stream The client stream identifier.
 * @param pid_lo The low 32-bits of the owning client process identifier.
 * @param pid_hi The high 32-bits of the owning client process identifier.
 */
void wfc_server_stream_destroy(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi);

/** Set callback for when src/dest rectangles are updated.
 *
 * @param stream The client stream identifier.
 * @param rects_change_cb Called when either src or dest rectangles have been
 *    updated.
 * @param rects_change_data Passed to the callback.
 * @return VCOS_SUCCESS if successful, or an error code if not.
 */
void wfc_server_stream_on_rects_change(WFCNativeStreamType stream,
      WFC_CALLBACK_T rects_change_cb, void *rects_change_data);

/** Get destination and source rectangles, following request from server
 *
 * @param stream The client stream identifier.
 * @param rects The rectangle data is written here.
 * @return VCOS_SUCCESS if successful, or an error code if not.
 */
#define WFC_SERVER_STREAM_RECTS_SIZE   8
uint32_t wfc_server_stream_get_rects(WFCNativeStreamType stream, int32_t rects[WFC_SERVER_STREAM_RECTS_SIZE]);

/** Returns true if given stream number is currently in use.
 *
 * @param stream The client stream identifier.
 * @return True if the client stream identifier is already in use, false if not.
 */
bool wfc_server_stream_is_in_use(WFCNativeStreamType stream);

/** Allocate images in the stream into which an off-screen context can write
 * composited output.
 *
 * @param stream The client stream identifier.
 * @param width The width of the images, in pixels.
 * @param height The height of the images, in pixels.
 * @param nbufs The number of images to allocate in the stream.
 * @return True on success, false on failure.
 */
bool wfc_server_stream_allocate_images(WFCNativeStreamType stream, uint32_t width, uint32_t height, uint32_t nbufs);

/** Signal that the given EGLImageKHR is the new front image for the stream.
 * @param stream     The client stream identifier.
 * @param ustorage   The innards of KHRN_IMAGE_T
 * @param width      etc..
 * @param height     etc...
 * @param stride
 * @param offset
 * @param format
 * @param flags
 * @param flip
 */
void wfc_server_stream_signal_eglimage_data(WFCNativeStreamType stream,
      uint32_t ustorage, uint32_t width, uint32_t height, uint32_t stride,
      uint32_t offset, uint32_t format, uint32_t flags, bool flip);

/** Signal that the given multimedia image handle is the new front image for
 * the stream.
 *
 * @param stream The client stream identifier.
 * @param image_handle The new front image for the stream.
 */
void wfc_server_stream_signal_mm_image_data(WFCNativeStreamType stream, uint32_t image_handle);

/** Signal that a raw image memhandle is the new front image for the stream.
 *
 * @param stream The client stream identifier.
 * @param handle The new front image memhandle for the stream.
 * @param format The image format. See WFC_PIXEL_FORMAT_T for supported formats.
 * @param width  The image width, in pixels.
 * @param height The image height, in pixels.
 * @param pitch  The image pitch, in bytes.
 * @param vpitch The image vertical pitch, in pixels.
 */
void wfc_server_stream_signal_raw_pixels(WFCNativeStreamType stream,
      uint32_t handle, uint32_t format, uint32_t width, uint32_t height,
      uint32_t pitch, uint32_t vpitch);

/** Signal that the given image is the new front image for the stream.
 *
 * The type of each image the stream must be the same e.g. it is not permitted
 * to mix opaque and raw pixel multimedia images within the same stream.
 *
 * @param stream  The client stream identifier.
 * @param image  Structure describing the image.
 */
void wfc_server_stream_signal_image(WFCNativeStreamType stream,
      const WFC_STREAM_IMAGE_T *image);

/** Register a stream as in use by a given process.
 *
 * @param stream The client stream identifier.
 * @param pid_lo The low 32-bits of the process ID.
 * @param pid_hi The high 32-bits of the process ID.
 */
void wfc_server_stream_register(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi);

/** Unregister a stream as in use by a given process.
 *
 * @param stream The client stream identifier.
 * @param pid_lo The low 32-bits of the process ID.
 * @param pid_hi The high 32-bits of the process ID.
 */
void wfc_server_stream_unregister(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi);

/** Retrieve the stream configuration information, if available.
 *
 * @param stream The client stream identifier.
 * @param info Address of block to receive the information on success.
 * @return VCOS_SUCCESS if successful, or an error code if not.
 */
uint32_t wfc_server_stream_get_info(WFCNativeStreamType stream, WFC_STREAM_INFO_T *info);

/** Set callback for when there is an image available. The stream must have
 * been created with either the WFC_STREAM_FLAGS_EGL or WFC_STREAM_FLAGS_BUF_AVAIL
 * flags set.
 *
 * @param stream The client stream identifier.
 * @param image_available_cb Called when there is an image available.
 * @param image_available_data Passed to the callback.
 */
void wfc_server_stream_on_image_available(WFCNativeStreamType stream,
      WFC_CALLBACK_T image_available_cb, void *image_available_data);

#endif   /* WFC_SERVER_API_H */
