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

#ifndef MMAL_VC_OPAQUE_ALLOC_H
#define MMAL_VC_OPAQUE_ALLOC_H

#include <stdint.h>
#include "interface/mmal/mmal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t MMAL_OPAQUE_IMAGE_HANDLE_T;

/** Allocate an opaque image on VideoCore.
 *
 * @return allocated handle, or zero if allocation failed.
 */
MMAL_OPAQUE_IMAGE_HANDLE_T mmal_vc_opaque_alloc(void);

/** Allocate an opaque image on VideoCore, providing a description.
 * @return allocated handle, or zero if allocation failed.
 */
MMAL_OPAQUE_IMAGE_HANDLE_T mmal_vc_opaque_alloc_desc(const char *description);

/** Release an opaque image.
 *
 * @param handle  handle allocated earlier
 * @return MMAL_SUCCESS or error code if handle not found
 */
MMAL_STATUS_T mmal_vc_opaque_release(MMAL_OPAQUE_IMAGE_HANDLE_T h);

/** Acquire an additional reference to an opaque image.
 *
 * @param handle  handle allocated earlier
 * @return MMAL_SUCCESS or error code if handle not found
 */
MMAL_STATUS_T mmal_vc_opaque_acquire(MMAL_OPAQUE_IMAGE_HANDLE_T h);

#ifdef __cplusplus
}
#endif

#endif
