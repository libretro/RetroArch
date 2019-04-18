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

#define EGL_EGLEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"
#include "interface/khronos/egl/egl_client_config.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"

#if EGL_BRCM_global_image

static EGLint get_bytes_per_pixel(EGLint pixel_format) /* returns 0 for invalid pixel formats */
{
   switch (pixel_format & ~EGL_PIXEL_FORMAT_USAGE_MASK_BRCM) {
   case EGL_PIXEL_FORMAT_ARGB_8888_PRE_BRCM: return 4;
   case EGL_PIXEL_FORMAT_ARGB_8888_BRCM:     return 4;
   case EGL_PIXEL_FORMAT_XRGB_8888_BRCM:     return 4;
   case EGL_PIXEL_FORMAT_RGB_565_BRCM:       return 2;
   case EGL_PIXEL_FORMAT_A_8_BRCM:           return 1;
   default:                                  return 0; /* invalid */
   }
}

/*
   failure is indicated by (!id[0] && !id[1]). call eglGetError for the error code

   possible failures:
   - width/height <= 0 or > EGL_CONFIG_MAX_WIDTH/EGL_CONFIG_MAX_HEIGHT (EGL_BAD_PARAMETER)
   - pixel_format invalid (EGL_BAD_PARAMETER)
   - insufficient resources (EGL_BAD_ALLOC)

   data may be NULL (in which case the image contents are undefined)
*/

EGLAPI void EGLAPIENTRY eglCreateGlobalImageBRCM(EGLint width, EGLint height, EGLint pixel_format, const void *data, EGLint data_stride, EGLint *id)
{
   EGLint bytes_per_pixel;

   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   /*
      check params
   */

   bytes_per_pixel = get_bytes_per_pixel(pixel_format);
   if ((width <= 0) || (width > EGL_CONFIG_MAX_WIDTH) ||
      (height <= 0) || (height > EGL_CONFIG_MAX_HEIGHT) ||
      (bytes_per_pixel == 0)) {
      thread->error = EGL_BAD_PARAMETER;
      id[0] = 0; id[1] = 0;
      return;
   }

   /*
      create the image
   */

   RPC_CALL4_OUT_CTRL(eglCreateGlobalImageBRCM_impl,
                      thread,
                      EGLCREATEGLOBALIMAGEBRCM_ID,
                      RPC_INT(width),
                      RPC_INT(height),
                      RPC_INT(pixel_format),
                      id);
   if (!id[0] && !id[1]) {
      thread->error = EGL_BAD_ALLOC;
      return;
   }

   /*
      fill the image in if necessary (this can't fail)
   */

   if (data) {
      #ifdef RPC_DIRECT
         RPC_CALL7(eglFillGlobalImageBRCM_impl, thread, no_id, id[0], id[1], 0, height, data, data_stride, pixel_format);
      #else
         EGLint y = 0;

         EGLint chunk_height_max = KHDISPATCH_WORKSPACE_SIZE / (width * bytes_per_pixel);
         vcos_assert(chunk_height_max != 0);

         while (height != 0) {
            VGint chunk_height = _min(height, chunk_height_max);

            uint32_t message[] = {
               EGLFILLGLOBALIMAGEBRCM_ID,
               RPC_INT(id[0]),
               RPC_INT(id[1]),
               RPC_INT(y),
               RPC_INT(chunk_height),
               RPC_INT(width * bytes_per_pixel),
               RPC_INT(pixel_format) };

            CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

            rpc_begin(thread);
            rpc_send_ctrl_begin(thread, sizeof(message));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_end(thread);
            rpc_send_bulk_gather(thread, data, width * bytes_per_pixel, data_stride, chunk_height);
            data = (const uint8_t *)data + (chunk_height * data_stride);
            rpc_end(thread);

            height -= chunk_height;
            y += chunk_height;
         }
      #endif
   }
}

/*
   failure is indicated by (!id[0] && !id[1]). call eglGetError for the error code

   possible failures:
   - src_id invalid (EGL_BAD_PARAMETER)
   - insufficient resources (EGL_BAD_ALLOC)
*/

EGLAPI void EGLAPIENTRY eglCreateCopyGlobalImageBRCM(const EGLint *src_id, EGLint *id)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   /*
      create the image
   */

   RPC_CALL3_OUT_CTRL(eglCreateCopyGlobalImageBRCM_impl,
                      thread,
                      EGLCREATECOPYGLOBALIMAGEBRCM_ID,
                      RPC_INT(src_id[0]),
                      RPC_INT(src_id[1]),
                      id);
   if (!id[0] && !id[1]) { /* not enough memory */
      thread->error = EGL_BAD_ALLOC;
   }
   if ((id[0] == -1) && (id[1] == -1)) { /* src_id was invalid */
      thread->error = EGL_BAD_PARAMETER;
      id[0] = 0; id[1] = 0;
   }
}

/*
   failure is indicated by returning EGL_FALSE. call eglGetError for the error code

   possible failures:
   - id invalid (EGL_BAD_PARAMETER)
*/

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyGlobalImageBRCM(const EGLint *id)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   /*
      destroy the image
   */

   if (!RPC_BOOLEAN_RES(RPC_CALL2_RES(eglDestroyGlobalImageBRCM_impl,
                                      thread,
                                      EGLDESTROYGLOBALIMAGEBRCM_ID,
                                      RPC_INT(id[0]),
                                      RPC_INT(id[1])))) {
      thread->error = EGL_BAD_PARAMETER;
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

/*
   failure is indicated by returning EGL_FALSE. call eglGetError for the error code

   possible failures:
   - id invalid (EGL_BAD_PARAMETER)
*/

EGLAPI EGLBoolean EGLAPIENTRY eglQueryGlobalImageBRCM(const EGLint *id, EGLint *width_height_pixel_format)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   if (!RPC_BOOLEAN_RES(RPC_CALL3_OUT_CTRL_RES(eglQueryGlobalImageBRCM_impl,
                                               thread,
                                               EGLQUERYGLOBALIMAGEBRCM_ID,
                                               RPC_INT(id[0]),
                                               RPC_INT(id[1]),
                                               width_height_pixel_format))) {
      thread->error = EGL_BAD_PARAMETER;
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

#endif
