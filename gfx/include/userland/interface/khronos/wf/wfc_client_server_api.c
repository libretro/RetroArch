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

#include "interface/khronos/wf/wfc_server_api.h"
#include "interface/khronos/wf/wfc_client_ipc.h"
#include "interface/khronos/wf/wfc_ipc.h"
#include "interface/vcos/vcos.h"
#include "interface/khronos/include/WF/wfc.h"
#include "interface/khronos/wf/wfc_int.h"
#include "interface/khronos/include/EGL/eglext.h"

#define VCOS_LOG_CATEGORY (&wfc_client_server_api_log_category)

//#define WFC_FULL_LOGGING
#ifdef WFC_FULL_LOGGING
#define WFC_CLIENT_SERVER_API_LOGLEVEL VCOS_LOG_TRACE
#else
#define WFC_CLIENT_SERVER_API_LOGLEVEL VCOS_LOG_WARN
#endif

static VCOS_LOG_CAT_T wfc_client_server_api_log_category;

/** Implement "void foo(WFCContext context)" */
static VCOS_STATUS_T wfc_client_server_api_send_context(WFC_IPC_MSG_TYPE msg_type, WFCContext context)
{
   WFC_IPC_MSG_CONTEXT_T msg;

   msg.header.type = msg_type;
   msg.context = context;

   return wfc_client_ipc_send(&msg.header, sizeof(msg));
}

/** Implement "void foo(WFCNativeStreamType stream)" */
static VCOS_STATUS_T wfc_client_server_api_send_stream(WFC_IPC_MSG_TYPE msg_type, WFCNativeStreamType stream)
{
   WFC_IPC_MSG_STREAM_T msg;

   msg.header.type = msg_type;
   msg.stream = stream;

   return wfc_client_ipc_send(&msg.header, sizeof(msg));
}

/** Implement "foo(WFCNativeStreamType stream)" where a result is returned.
 * This may either be as a return value, or via a pointer parameter.
 */
static VCOS_STATUS_T wfc_client_server_api_sendwait_stream(WFC_IPC_MSG_TYPE msg_type, WFCNativeStreamType stream,
      void *result, size_t *result_len)
{
   WFC_IPC_MSG_STREAM_T msg;

   msg.header.type = msg_type;
   msg.stream = stream;

   return wfc_client_ipc_sendwait(&msg.header, sizeof(msg), result, result_len);
}

/* ========================================================================= */

VCOS_STATUS_T wfc_server_connect(void)
{
   VCOS_STATUS_T status;

   vcos_log_set_level(VCOS_LOG_CATEGORY, WFC_CLIENT_SERVER_API_LOGLEVEL);
   vcos_log_register("wfccsapi", VCOS_LOG_CATEGORY);

   status = wfc_client_ipc_init();

   vcos_log_trace("%s: result %d", VCOS_FUNCTION, status);

   if (status != VCOS_SUCCESS)
   {
      vcos_log_unregister(VCOS_LOG_CATEGORY);
   }

   return status;
}

/* ------------------------------------------------------------------------- */

void wfc_server_disconnect(void)
{
   vcos_log_trace("%s: called", VCOS_FUNCTION);

   if (wfc_client_ipc_deinit())
   {
      vcos_log_unregister(VCOS_LOG_CATEGORY);
   }
}

/* ------------------------------------------------------------------------- */

void wfc_server_use_keep_alive(void)
{
   wfc_client_ipc_use_keep_alive();
}

/* ------------------------------------------------------------------------- */

void wfc_server_release_keep_alive(void)
{
   wfc_client_ipc_release_keep_alive();
}

/* ------------------------------------------------------------------------- */

uint32_t wfc_server_create_context(WFCContext context, uint32_t context_type,
   uint32_t screen_or_stream_num, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_CREATE_CONTEXT_T msg;
   VCOS_STATUS_T status;
   uint32_t result = -1;
   size_t result_len = sizeof(result);

   vcos_log_trace("%s: context 0x%x type 0x%x num 0x%x pid 0x%x%08x", VCOS_FUNCTION,
         context, context_type, screen_or_stream_num, pid_hi, pid_lo);

   msg.header.type = WFC_IPC_MSG_CREATE_CONTEXT;
   msg.context = context;
   msg.context_type = context_type;
   msg.screen_or_stream_num = screen_or_stream_num;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_sendwait(&msg.header, sizeof(msg), &result, &result_len);

   vcos_log_trace("%s: status 0x%x, result 0x%x", VCOS_FUNCTION, status, result);

   if (status != VCOS_SUCCESS)
      result = -1;

   return result;
}

/* ------------------------------------------------------------------------- */

void wfc_server_destroy_context(WFCContext context)
{
   VCOS_STATUS_T status;

   vcos_log_trace("%s: context 0x%x", VCOS_FUNCTION, context);

   status = wfc_client_server_api_send_context(WFC_IPC_MSG_DESTROY_CONTEXT, context);

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

uint32_t wfc_server_commit_scene(WFCContext context, const WFC_SCENE_T *scene,
      uint32_t flags, WFC_CALLBACK_T scene_taken_cb, void *scene_taken_data)
{
   WFC_IPC_MSG_COMMIT_SCENE_T msg;
   VCOS_STATUS_T status = VCOS_SUCCESS;
   uint32_t result = VCOS_ENOSYS;
   size_t result_len = sizeof(result);
   uint32_t i;

   vcos_log_trace("%s: context 0x%x commit %u elements %u flags 0x%x",
         VCOS_FUNCTION, context, scene->commit_count, scene->element_count, flags);
   for (i = 0; i < scene->element_count; i++)
   {
      vcos_log_trace("%s: element[%u] stream 0x%x", VCOS_FUNCTION, i, scene->elements[i].source_stream);
   }

   msg.header.type = WFC_IPC_MSG_COMMIT_SCENE;
   msg.context = context;
   msg.flags = flags;
   msg.scene_taken_cb.ptr = scene_taken_cb;
   msg.scene_taken_data.ptr = scene_taken_data;
   memcpy(&msg.scene, scene, sizeof(*scene));

   if (flags & WFC_SERVER_COMMIT_WAIT)
   {
      /* Caller will wait for callback, call cannot fail */
      vcos_assert(scene_taken_cb != NULL);
      vcos_assert(scene_taken_data != NULL);
   }
   else
   {
      /* Caller will not wait for callback, so need to at least wait for result. */
      vcos_assert(scene_taken_cb == NULL);
      vcos_assert(scene_taken_data == NULL);
   }

   status = wfc_client_ipc_sendwait(&msg.header, sizeof(msg), &result, &result_len);

   /* Override the result if the status was an error */
   if (status != VCOS_SUCCESS)
      result = status;

   return result;
}

/* ------------------------------------------------------------------------- */

void wfc_server_activate(WFCContext context)
{
   VCOS_STATUS_T status;

   vcos_log_trace("%s: context 0x%x", VCOS_FUNCTION, context);

   status = wfc_client_server_api_send_context(WFC_IPC_MSG_ACTIVATE, context);

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_deactivate(WFCContext context)
{
   VCOS_STATUS_T status;

   vcos_log_trace("%s: context 0x%x", VCOS_FUNCTION, context);

   status = wfc_client_server_api_send_context(WFC_IPC_MSG_DEACTIVATE, context);

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_set_deferral_stream(WFCContext context, WFCNativeStreamType stream)
{
   WFC_IPC_MSG_SET_DEFERRAL_STREAM_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: context 0x%x stream 0x%x", VCOS_FUNCTION, context, stream);

   msg.header.type = WFC_IPC_MSG_SET_DEFERRAL_STREAM;
   msg.context = context;
   msg.stream = stream;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

WFCNativeStreamType wfc_server_stream_create(WFCNativeStreamType stream, uint32_t flags, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_SS_CREATE_INFO_T msg;
   VCOS_STATUS_T status;
   WFCNativeStreamType result = WFC_INVALID_HANDLE;
   size_t result_len = sizeof(result);

   vcos_log_trace("%s: stream 0x%x flags 0x%x pid 0x%x%08x", VCOS_FUNCTION, stream, flags, pid_hi, pid_lo);

   msg.header.type = WFC_IPC_MSG_SS_CREATE_INFO;
   msg.stream = stream;
   memset(&msg.info, 0, sizeof(msg.info));
   msg.info.size = sizeof(msg.info);
   msg.info.flags = flags;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_sendwait(&msg.header, sizeof(msg), &result, &result_len);

   vcos_log_trace("%s: status 0x%x, result 0x%x", VCOS_FUNCTION, status, result);

   if (status != VCOS_SUCCESS)
      result = WFC_INVALID_HANDLE;

   return result;
}

/* ------------------------------------------------------------------------- */

WFCNativeStreamType wfc_server_stream_create_info(WFCNativeStreamType stream, const WFC_STREAM_INFO_T *info, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_SS_CREATE_INFO_T msg;
   uint32_t copy_size;
   VCOS_STATUS_T status;
   WFCNativeStreamType result = WFC_INVALID_HANDLE;
   size_t result_len = sizeof(result);

   if (!info)
   {
      vcos_log_error("%s: NULL info pointer passed", VCOS_FUNCTION);
      return WFC_INVALID_HANDLE;
   }

   if (info->size < sizeof(uint32_t))
   {
      vcos_log_error("%s: invalid info pointer passed (size:%u)", VCOS_FUNCTION, info->size);
      return WFC_INVALID_HANDLE;
   }

   vcos_log_trace("%s: stream 0x%x flags 0x%x pid 0x%x%08x", VCOS_FUNCTION, stream, info->flags, pid_hi, pid_lo);

   msg.header.type = WFC_IPC_MSG_SS_CREATE_INFO;
   msg.stream = stream;
   copy_size = vcos_min(info->size, sizeof(msg.info));
   memcpy(&msg.info, info, copy_size);
   msg.info.size = copy_size;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_sendwait(&msg.header, sizeof(msg), &result, &result_len);

   vcos_log_trace("%s: status 0x%x, result 0x%x", VCOS_FUNCTION, status, result);

   if (status != VCOS_SUCCESS)
      result = WFC_INVALID_HANDLE;

   return result;
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_destroy(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_SS_DESTROY_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   msg.header.type = WFC_IPC_MSG_SS_DESTROY;
   msg.stream = stream;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_on_rects_change(WFCNativeStreamType stream, WFC_CALLBACK_T rects_change_cb, void *rects_change_data)
{
   WFC_IPC_MSG_SS_ON_RECTS_CHANGE_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x cb %p data %p", VCOS_FUNCTION, stream, rects_change_cb, rects_change_data);

   msg.header.type = WFC_IPC_MSG_SS_ON_RECTS_CHANGE;
   msg.stream = stream;
   msg.rects_change_cb.ptr = rects_change_cb;
   msg.rects_change_data.ptr = rects_change_data;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   if (!vcos_verify(status == VCOS_SUCCESS))
   {
      (*rects_change_cb)(rects_change_data);
   }
}

/* ------------------------------------------------------------------------- */

uint32_t wfc_server_stream_get_rects(WFCNativeStreamType stream, int32_t rects[WFC_SERVER_STREAM_RECTS_SIZE])
{
   uint32_t result;
   VCOS_STATUS_T status;
   WFC_IPC_MSG_SS_GET_RECTS_T reply;
   size_t rects_len = sizeof(reply) - sizeof(WFC_IPC_MSG_HEADER_T);

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);
   memset(&reply, 0, sizeof(reply));
   status = wfc_client_server_api_sendwait_stream(WFC_IPC_MSG_SS_GET_RECTS, stream, &reply.result, &rects_len);

   if (status == VCOS_SUCCESS)
   {
      result = reply.result;

      if (result == VCOS_SUCCESS)
      {
         memcpy(rects, reply.rects, WFC_SERVER_STREAM_RECTS_SIZE * sizeof(*rects));
         vcos_log_trace("%s: rects (%d,%d,%d,%d) (%d,%d,%d,%d)", VCOS_FUNCTION,
               rects[0], rects[1], rects[2], rects[3], rects[4], rects[5], rects[6], rects[7]);
      }
      else
      {
         vcos_log_error("%s: result %d", VCOS_FUNCTION, result);
      }
   }
   else
   {
      vcos_log_error("%s: send msg status %d", VCOS_FUNCTION, status);
      result = status;
   }

   return result;
}

/* ------------------------------------------------------------------------- */

bool wfc_server_stream_is_in_use(WFCNativeStreamType stream)
{
   VCOS_STATUS_T status;
   uint32_t result = 0;
   size_t result_len = sizeof(result);

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   status = wfc_client_server_api_sendwait_stream(WFC_IPC_MSG_SS_IS_IN_USE, stream, &result, &result_len);

   vcos_log_trace("%s: status 0x%x, result %u", VCOS_FUNCTION, status, result);

   if (status != VCOS_SUCCESS)
      result = 0;

   return result != 0;
}

/* ------------------------------------------------------------------------- */

bool wfc_server_stream_allocate_images(WFCNativeStreamType stream, uint32_t width, uint32_t height, uint32_t nbufs)
{
   WFC_IPC_MSG_SS_ALLOCATE_IMAGES_T msg;
   VCOS_STATUS_T status;
   uint32_t result = 0;
   size_t result_len = sizeof(result);

   vcos_log_trace("%s: stream 0x%x width %u height %u nbufs %u", VCOS_FUNCTION, stream, width, height, nbufs);

   msg.header.type = WFC_IPC_MSG_SS_ALLOCATE_IMAGES;
   msg.stream = stream;
   msg.width = width;
   msg.height = height;
   msg.nbufs = nbufs;

   status = wfc_client_ipc_sendwait(&msg.header, sizeof(msg), &result, &result_len);

   vcos_log_trace("%s: status 0x%x result %u", VCOS_FUNCTION, status, result);

   if (status != VCOS_SUCCESS)
      result = 0;

   return result;
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_signal_eglimage_data(WFCNativeStreamType stream,
      uint32_t ustorage, uint32_t width, uint32_t height, uint32_t stride,
      uint32_t offset, uint32_t format, uint32_t flags, bool flip)
{
   WFC_IPC_MSG_SS_SIGNAL_EGLIMAGE_DATA_T msg;
   VCOS_STATUS_T status;

   memset(&msg, 0, sizeof msg);
   msg.header.type = WFC_IPC_MSG_SS_SIGNAL_EGLIMAGE_DATA;
   msg.stream = stream;
   msg.ustorage = ustorage;
   msg.width = width;
   msg.height = height;
   msg.stride = stride;
   msg.offset = offset;
   msg.format = format;
   msg.flags = flags;
   msg.flip = flip;

   vcos_log_trace("%s: stream 0x%x image storage 0x%x",
         VCOS_FUNCTION, stream, ustorage);

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_signal_mm_image_data(WFCNativeStreamType stream, uint32_t image_handle)
{
   WFC_IPC_MSG_SS_SIGNAL_MM_IMAGE_DATA_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x image 0x%x", VCOS_FUNCTION, stream, image_handle);

   msg.header.type = WFC_IPC_MSG_SS_SIGNAL_MM_IMAGE_DATA;
   msg.stream = stream;
   msg.image_handle = image_handle;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_signal_raw_pixels(WFCNativeStreamType stream,
      uint32_t handle, uint32_t format, uint32_t width, uint32_t height,
      uint32_t pitch, uint32_t vpitch)
{
   WFC_IPC_MSG_SS_SIGNAL_RAW_PIXELS_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x image 0x%x format 0x%x width %u height %u"
         " pitch %u vpitch %u",
         VCOS_FUNCTION, stream, handle, format, width, height, pitch, vpitch);

   msg.header.type = WFC_IPC_MSG_SS_SIGNAL_RAW_PIXELS;
   msg.stream = stream;
   msg.handle = handle;
   msg.format = format;
   msg.width = width;
   msg.height = height;
   msg.pitch = pitch;
   msg.vpitch = vpitch;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));
   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */
void wfc_server_stream_signal_image(WFCNativeStreamType stream,
      const WFC_STREAM_IMAGE_T *image)
{
   WFC_IPC_MSG_SS_SIGNAL_IMAGE_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x type 0x%x handle 0x%x "
         " format 0x%x protection 0x%x width %u height %u "
         " pitch %u vpitch %u",
         VCOS_FUNCTION, stream, image->type, image->handle,
         image->format, image->protection, image->width, image->height,
         image->pitch, image->vpitch);

   msg.header.type = WFC_IPC_MSG_SS_SIGNAL_IMAGE;
   msg.stream = stream;
   if vcos_verify(image->length <= sizeof(msg.image))
   {
      msg.image = *image;
   }
   else
   {
      /* Client is newer than VC ? */
      memcpy(&msg.image, image, sizeof(msg.image));
      msg.image.length = sizeof(msg.image);
   }

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_register(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_SS_REGISTER_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x pid 0x%x%08x", VCOS_FUNCTION, stream, pid_hi, pid_lo);

   msg.header.type = WFC_IPC_MSG_SS_REGISTER;
   msg.stream = stream;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_unregister(WFCNativeStreamType stream, uint32_t pid_lo, uint32_t pid_hi)
{
   WFC_IPC_MSG_SS_UNREGISTER_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x pid 0x%x%08x", VCOS_FUNCTION, stream, pid_hi, pid_lo);

   msg.header.type = WFC_IPC_MSG_SS_UNREGISTER;
   msg.stream = stream;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   vcos_assert(status == VCOS_SUCCESS);
}

/* ------------------------------------------------------------------------- */

uint32_t wfc_server_stream_get_info(WFCNativeStreamType stream, WFC_STREAM_INFO_T *info)
{
   uint32_t result;
   VCOS_STATUS_T status;
   WFC_IPC_MSG_SS_GET_INFO_T reply;
   size_t info_len = sizeof(reply) - sizeof(WFC_IPC_MSG_HEADER_T);

   if (!info)
   {
      vcos_log_error("%s: NULL info pointer passed", VCOS_FUNCTION);
      return WFC_INVALID_HANDLE;
   }

   if (info->size < sizeof(uint32_t))
   {
      vcos_log_error("%s: invalid info pointer passed (size:%u)", VCOS_FUNCTION, info->size);
      return WFC_INVALID_HANDLE;
   }

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);
   memset(&reply, 0, sizeof(reply));
   status = wfc_client_server_api_sendwait_stream(WFC_IPC_MSG_SS_GET_INFO, stream, &reply.result, &info_len);

   if (status == VCOS_SUCCESS)
   {
      result = reply.result;

      if (result == VCOS_SUCCESS)
      {
         uint32_t copy_size = vcos_min(info->size, reply.info.size);
         memcpy(info, &reply.info, copy_size);
         info->size = copy_size;
         vcos_log_trace("%s: copied %u bytes", VCOS_FUNCTION, copy_size);
      }
      else
      {
         vcos_log_error("%s: result %d", VCOS_FUNCTION, result);
      }
   }
   else
   {
      vcos_log_error("%s: send msg status %d", VCOS_FUNCTION, status);
      result = status;
   }

   return result;
}

/* ------------------------------------------------------------------------- */

void wfc_server_stream_on_image_available(WFCNativeStreamType stream, WFC_CALLBACK_T image_available_cb, void *image_available_data)
{
   WFC_IPC_MSG_SS_ON_IMAGE_AVAILABLE_T msg;
   VCOS_STATUS_T status;

   vcos_log_trace("%s: stream 0x%x cb %p data %p", VCOS_FUNCTION, stream, image_available_cb, image_available_data);

   msg.header.type = WFC_IPC_MSG_SS_ON_IMAGE_AVAILABLE;
   msg.stream = stream;
   msg.image_available_cb.ptr = image_available_cb;
   msg.image_available_data.ptr = image_available_data;

   status = wfc_client_ipc_send(&msg.header, sizeof(msg));

   if (!vcos_verify(status == VCOS_SUCCESS))
   {
      (*image_available_cb)(image_available_data);
   }
}
