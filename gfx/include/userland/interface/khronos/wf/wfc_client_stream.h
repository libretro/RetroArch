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

#ifndef WFC_CLIENT_STREAM_H
#define WFC_CLIENT_STREAM_H

#include "interface/vctypes/vc_image_types.h"
#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/include/WF/wfc.h"
#include "interface/khronos/wf/wfc_int.h"
#include "interface/khronos/include/EGL/eglext.h"

//==============================================================================
//!@name Flags
//!@{

#define WFC_STREAM_FLAGS_NONE       0
//!@brief Allow the server to indicate to the host when a buffer is available; for use
//! with wfc_stream_await_buffer().
#define WFC_STREAM_FLAGS_BUF_AVAIL  (1 << 0)
//! GNL - Hack up a new semaphore thing to make Android rendering wait
#define WFC_STREAM_FLAGS_ANDROID_GL_STREAM (1 << 2)
//! So the reference to the vc image in the pool can be released
#define WFC_STREAM_FLAGS_ANDROID_MM_STREAM (1 << 3)

//! Flags needed to work with EGL
#define WFC_STREAM_FLAGS_EGL  (WFC_STREAM_FLAGS_BUF_AVAIL)

//!@}
//!@name Internal flags; do not use directly.
//!@{

//!@brief Allow the server to indicate to the host that a change to the source and/or
//! destination rectangles has been requested by the server.
#define WFC_STREAM_FLAGS_REQ_RECT   (1 << 31)

//!@}
//==============================================================================

//! Type for callback from wfc_stream_create_req_rect().
typedef void (*WFC_STREAM_REQ_RECT_CALLBACK_T)
   (void *args, const WFCint dest_rect[WFC_RECT_SIZE], const WFCfloat src_rect[WFC_RECT_SIZE]);

//==============================================================================

//! In cases where the caller doesn't want to assign a stream number, provide one for it.
WFCNativeStreamType wfc_stream_get_next(void);

//!@brief Create a stream, using the given stream handle (typically assigned by the
//! window manager). Return zero if OK.
uint32_t wfc_stream_create(WFCNativeStreamType stream, uint32_t flags);

//! Create a stream, and automatically assign it a new stream number, which is returned
WFCNativeStreamType wfc_stream_create_assign_id(uint32_t flags);

//!@brief Create a stream, using the given stream handle, which will notify the calling
//! module when the server requests a change in source and/or destination rectangle,
//! using the supplied callback. Return zero if OK.
uint32_t wfc_stream_create_req_rect
   (WFCNativeStreamType stream, uint32_t flags,
      WFC_STREAM_REQ_RECT_CALLBACK_T callback, void *cb_args);

//!@brief Indicate that a source or mask is now associated with this stream, or should
//! now be removed from such an association.
//!
//!@return True if successful, false if not (invalid handle).
bool wfc_stream_register_source_or_mask(WFCNativeStreamType stream, bool add_source_or_mask);

//!@brief Suspend until buffer is available on the server (requires
//! WFC_STREAM_FLAGS_ASYNC_SEM to have been specified on creation).
void wfc_stream_await_buffer(WFCNativeStreamType stream);

//! Destroy a stream.
void wfc_stream_destroy(WFCNativeStreamType stream);

//------------------------------------------------------------------------------
//!@name Off-screen composition functions
//!@{

//! Create a stream for an off-screen context to output to, with the default number of buffers.
uint32_t wfc_stream_create_for_context
   (WFCNativeStreamType stream, uint32_t width, uint32_t height);

//! Create a stream for an off-screen context to output to, with a specific number of buffers.
uint32_t wfc_stream_create_for_context_nbufs
   (WFCNativeStreamType stream, uint32_t width, uint32_t height, uint32_t nbufs);

//! Returns true if this stream exists, and is in use as the output of an off-screen context.
bool wfc_stream_used_for_off_screen(WFCNativeStreamType stream);

//!@brief Called on behalf of an off-screen context, to either set or clear the stream's
//! flag indicating that it's being used as output for that context.
void wfc_stream_register_off_screen(WFCNativeStreamType stream, bool used_for_off_screen);

//!@}
//------------------------------------------------------------------------------

void wfc_stream_signal_eglimage_data(WFCNativeStreamType stream, EGLImageKHR im);
void wfc_stream_signal_eglimage_data_protected(WFCNativeStreamType stream, EGLImageKHR im, uint32_t is_protected);
void wfc_stream_release_eglimage_data(WFCNativeStreamType stream, EGLImageKHR im);
void wfc_stream_signal_mm_image_data(WFCNativeStreamType stream, uint32_t im);

void wfc_stream_signal_raw_pixels(WFCNativeStreamType stream, uint32_t handle,
      uint32_t format, uint32_t w, uint32_t h, uint32_t pitch, uint32_t vpitch);
void wfc_stream_signal_image(WFCNativeStreamType stream,
      const WFC_STREAM_IMAGE_T *image);
void wfc_stream_register(WFCNativeStreamType stream);
void wfc_stream_unregister(WFCNativeStreamType stream);

//==============================================================================
#endif /* WF_INT_STREAM_H_ */
