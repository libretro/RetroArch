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

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "middleware/khronos/egl/egl_server.h"
#include "middleware/imageconv/imageconv.h"
#include "vcinclude/vc_image_types.h"


typedef struct EGL_IMAGE_T {
   uint64_t pid;

   /*
    * Handle to a KHRN_IMAGE_T, whose format is required to be something
    * suitable for texturing directly from. If NULL, then use external.convert
    * below to make one (in glBindTexture_impl probably).
    */
   MEM_HANDLE_T mh_image;

   bool flip_y;

   /*
    * Any kind of "external" image-- i.e. that can't be used directly for
    * texturing.
    */
   struct
   {
      /*
       * Handle to an object that convert knows how to convert into a
       * KHRN_IMAGE_T suitable for texturing from, e.g. a multimedia image.
       */
      MEM_HANDLE_T src;
      const IMAGE_CONVERT_CLASS_T *convert;
      KHRN_IMAGE_FORMAT_T conv_khrn_format;
      VC_IMAGE_TYPE_T conv_vc_format;
      uint32_t src_updated;
      uint32_t src_converted;
   } external;

} EGL_IMAGE_T;

extern void egl_image_term(void *v, uint32_t size);
