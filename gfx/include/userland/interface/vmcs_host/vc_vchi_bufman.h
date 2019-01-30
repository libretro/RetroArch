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

#ifndef VC_VCHI_BUFMAN_H
#define VC_VCHI_BUFMAN_H

#include "interface/vctypes/vc_image_types.h"
#include "interface/vchi/vchi.h"
#ifdef __SYMBIAN32__
#include "interface/vmcs_host/vc_vchi_bufman_defs.h"
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;
namespace BufManX {
#else
#include "interface/vmcs_host/vc_dispmanx.h"
#include "interface/vmcs_host/vc_vchi_bufman_defs.h"
#endif

typedef void (*vc_bufman_callback_t) (void *next_cookie, void *next_cookie2, int32_t success);

VCHPRE_ void VCHPOST_ vc_vchi_bufman_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections);

typedef struct
{
   buf_frame_type_t type;
   int width, height, pitch;
   int bpp; // bits per pixel
   int size;
   void *pixels;
} BUFMANX_IMAGE_T;

#define BUFMAN_TRANSFORM_HFLIP     (1<<0)
#define BUFMAN_TRANSFORM_VFLIP     (1<<1)
#define BUFMAN_TRANSFORM_TRANSPOSE (1<<2)

typedef enum {
   BUFMAN_TRANSFORM_ROT0           = 0,
   BUFMAN_TRANSFORM_MIRROR_ROT0    = BUFMAN_TRANSFORM_HFLIP,
   BUFMAN_TRANSFORM_MIRROR_ROT180  = BUFMAN_TRANSFORM_VFLIP,
   BUFMAN_TRANSFORM_ROT180         = BUFMAN_TRANSFORM_HFLIP|BUFMAN_TRANSFORM_VFLIP,
   BUFMAN_TRANSFORM_MIRROR_ROT90   = BUFMAN_TRANSFORM_TRANSPOSE,
   BUFMAN_TRANSFORM_ROT270         = BUFMAN_TRANSFORM_TRANSPOSE|BUFMAN_TRANSFORM_HFLIP,
   BUFMAN_TRANSFORM_ROT90          = BUFMAN_TRANSFORM_TRANSPOSE|BUFMAN_TRANSFORM_VFLIP,
   BUFMAN_TRANSFORM_MIRROR_ROT270  = BUFMAN_TRANSFORM_TRANSPOSE|BUFMAN_TRANSFORM_HFLIP|BUFMAN_TRANSFORM_VFLIP,
} BUFMAN_TRANSFORM_T;


// we use an opaque type here as the internals shouldn't be used externally, but allocation of the size of the block is required by the caller.
#define BUFMANX_HANDLE_T_SIZE 1024
typedef struct {
   char opaque[BUFMANX_HANDLE_T_SIZE];
} BUFMANX_HANDLE_T;

VCHPRE_ int32_t VCHPOST_ vc_bufmanx_convert_init(void);
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_set_transform_buffer(void *pixels, int size);
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_allocate_image(BUFMANX_IMAGE_T *image);
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_release_image(BUFMANX_IMAGE_T *image);
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_get_default_pitch( BUFMANX_IMAGE_T *src );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_get_default_size(BUFMANX_IMAGE_T *src);
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_push ( BUFMANX_HANDLE_T *h, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_pull ( BUFMANX_HANDLE_T *h, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_pull_blocking ( BUFMANX_HANDLE_T *h, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_push_blocking ( BUFMANX_HANDLE_T *h, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_pull_striped ( BUFMANX_HANDLE_T *xh, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_pull_striped_blocking ( BUFMANX_HANDLE_T *xh, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_push_striped ( BUFMANX_HANDLE_T *xh, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_push_striped_blocking ( BUFMANX_HANDLE_T *xh, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, const VC_RECT_T *src_rect, const VC_RECT_T *dest_rect, BUFMAN_TRANSFORM_T transform );

VCHPRE_ void VCHPOST_ vc_bufmanx_push_multi ( BUFMANX_HANDLE_T *h, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ void VCHPOST_ vc_bufmanx_pull_multi ( BUFMANX_HANDLE_T *h, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_push_multi_blocking ( BUFMANX_HANDLE_T *h, const BUFMANX_IMAGE_T *src, DISPMANX_RESOURCE_HANDLE_T dst, BUFMAN_TRANSFORM_T transform );
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_pull_multi_blocking ( BUFMANX_HANDLE_T *h, BUFMANX_IMAGE_T *dst, const DISPMANX_RESOURCE_HANDLE_T src, BUFMAN_TRANSFORM_T transform );

// Allocate the specified number and type of buffers on the server side, for use with streams
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_allocate_buffers
   (uint32_t stream, uint32_t num_of_buffers,
      buf_frame_type_t type, uint32_t width, uint32_t height);

// Free buffers on the server which are associated with the specified stream
#define VC_BUFMANX_FREE_BUFFERS_ALL    0
VCHPRE_ int32_t VCHPOST_ vc_bufmanx_free_buffers(uint32_t stream, uint32_t num_of_buffers);

// Like vc_bufmanx_push_multi(), but specifies a stream, rather than a dispmanx resource handle,
// to push the data to.
VCHPRE_ void VCHPOST_ vc_bufmanx_push_multi_stream ( BUFMANX_HANDLE_T *xh, const BUFMANX_IMAGE_T *src, uint32_t stream, BUFMAN_TRANSFORM_T transform, vc_bufman_callback_t callback, void *cookie, void *cookie2 );

VCHPRE_ VC_IMAGE_TYPE_T VCHPOST_ vc_bufmanx_get_vc_image_type(buf_frame_type_t bm_type);

#ifdef __SYMBIAN32__
} // namespace BufManX
#endif

#endif /* VC_VCHI_BUFMAN_H */
