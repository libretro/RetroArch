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

#include <stdlib.h>
#include <string.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"
#include "containers/net/net_sockets.h"

/* Uncomment this macro definition to capture data read and written through this interface */
/* #define IO_NET_CAPTURE_PACKETS */

#ifdef IO_NET_CAPTURE_PACKETS
#include <stdio.h>

#ifdef ENABLE_CONTAINERS_STANDALONE
#ifdef _MSC_VER
#define IO_NET_CAPTURE_PREFIX          "C:\\"
#else /* !_MSC_VER */
#define IO_NET_CAPTURE_PREFIX          "~/"
#endif
#else /* !ENABLE_CONTAINERS_STANDALONE */
#define IO_NET_CAPTURE_PREFIX          "/mfs/sd/"
#endif

#define IO_NET_CAPTURE_READ_FILE       "capture_read_%s_%s%c.pkt"
#define IO_NET_CAPTURE_WRITE_FILE      "capture_write_%s_%s%c.pkt"
#define IO_NET_CAPTURE_READ_FORMAT     IO_NET_CAPTURE_PREFIX IO_NET_CAPTURE_READ_FILE
#define IO_NET_CAPTURE_WRITE_FORMAT    IO_NET_CAPTURE_PREFIX IO_NET_CAPTURE_WRITE_FILE

#define CAPTURE_FILENAME_BUFFER_SIZE   300

#define CAPTURE_BUFFER_SIZE            65536

/** Native byte order word */
#define NATIVE_BYTE_ORDER  0x50415753
#endif

/******************************************************************************
Defines and constants.
******************************************************************************/

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_IO_MODULE_T
{
   VC_CONTAINER_NET_T *sock;
#ifdef IO_NET_CAPTURE_PACKETS
   FILE *read_capture_file;
   FILE *write_capture_file;
#endif
} VC_CONTAINER_IO_MODULE_T;

/** List of recognised network URI schemes (TCP or UDP).
 * Note: always use lower case for the scheme name. */
static struct
{
   const char *scheme;
   bool is_udp;
} recognised_schemes[] = {
   { "rtp:", true },
   { "rtsp:", false },
};

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_net_open( VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T );

/******************************************************************************
Local Functions
******************************************************************************/

#ifdef IO_NET_CAPTURE_PACKETS
/*****************************************************************************/
static FILE *io_net_open_capture_file(const char *host_str,
      const char *port_str,
      bool is_udp,
      VC_CONTAINER_IO_MODE_T mode)
{
   char filename[CAPTURE_FILENAME_BUFFER_SIZE];
   const char *format;
   FILE *stream = NULL;
   uint32_t byte_order = NATIVE_BYTE_ORDER;

   switch (mode)
   {
   case VC_CONTAINER_IO_MODE_WRITE:
      format = IO_NET_CAPTURE_WRITE_FORMAT;
      break;
   case VC_CONTAINER_IO_MODE_READ:
      format = IO_NET_CAPTURE_READ_FORMAT;
      break;
   default:
      /* Invalid mode */
      return NULL;
   }

   if (!host_str)
      host_str = "";
   if (!port_str)
      port_str = "";

   /* Check filename will fit in buffer */
   if (strlen(format) + strlen(host_str) + strlen(port_str) - 4 > CAPTURE_FILENAME_BUFFER_SIZE)
      return NULL;

   /* Create the file */
   sprintf(filename, format, host_str, port_str, is_udp ? 'u' : 't');
   stream = fopen(filename, "wb");
   if (!stream)
      return NULL;

   /* Buffer plenty of data at a time, if possible */
   setvbuf(stream, NULL, _IOFBF, CAPTURE_BUFFER_SIZE);

   /* Start file with a byte order marker */
   if (fwrite(&byte_order, 1, sizeof(byte_order), stream) != sizeof(byte_order))
   {
      /* Failed to write even just the byte order mark - abort */
      fclose(stream);
      stream = NULL;
      remove(filename);
   }

   return stream;
}

/*****************************************************************************/
static void io_net_capture_write_packet( FILE *stream,
      const char *buffer,
      uint32_t buffer_size )
{
   if (stream && buffer && buffer_size)
   {
      fwrite(&buffer_size, 1, sizeof(buffer_size), stream);
      fwrite(buffer, 1, buffer_size, stream);
   }
}
#endif

/*****************************************************************************/
static bool io_net_recognise_scheme(const char *uri, bool *is_udp)
{
   size_t ii;
   const char *scheme;

   if (!uri)
      return false;

   for (ii = 0; ii < countof(recognised_schemes); ii++)
   {
      scheme = recognised_schemes[ii].scheme;
      if (strncmp(scheme, uri, strlen(scheme)) == 0)
      {
         *is_udp = recognised_schemes[ii].is_udp;
         return true;
      }
   }

   return false;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T translate_net_status_to_container_status(vc_container_net_status_t net_status)
{
   switch (net_status)
   {
   case VC_CONTAINER_NET_SUCCESS:                  return VC_CONTAINER_SUCCESS;
   case VC_CONTAINER_NET_ERROR_INVALID_SOCKET:     return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   case VC_CONTAINER_NET_ERROR_NOT_ALLOWED:        return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   case VC_CONTAINER_NET_ERROR_INVALID_PARAMETER:  return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   case VC_CONTAINER_NET_ERROR_NO_MEMORY:          return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   case VC_CONTAINER_NET_ERROR_IN_USE:             return VC_CONTAINER_ERROR_URI_OPEN_FAILED;
   case VC_CONTAINER_NET_ERROR_NETWORK:            return VC_CONTAINER_ERROR_EOS;
   case VC_CONTAINER_NET_ERROR_CONNECTION_LOST:    return VC_CONTAINER_ERROR_EOS;
   case VC_CONTAINER_NET_ERROR_NOT_CONNECTED:      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   case VC_CONTAINER_NET_ERROR_TIMED_OUT:          return VC_CONTAINER_ERROR_ABORTED;
   case VC_CONTAINER_NET_ERROR_CONNECTION_REFUSED: return VC_CONTAINER_ERROR_NOT_FOUND;
   case VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND:     return VC_CONTAINER_ERROR_NOT_FOUND;
   case VC_CONTAINER_NET_ERROR_TRY_AGAIN:          return VC_CONTAINER_ERROR_CONTINUE;
   default:                                        return VC_CONTAINER_ERROR_FAILED;
   }
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_net_close( VC_CONTAINER_IO_T *p_ctx )
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;

   if (!module)
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   if (module->sock)
      vc_container_net_close(module->sock);
#ifdef IO_NET_CAPTURE_PACKETS
   if (module->read_capture_file)
      fclose(module->read_capture_file);
   if (module->write_capture_file)
      fclose(module->write_capture_file);
#endif
   free(module);
   p_ctx->module = NULL;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_net_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret = vc_container_net_read(p_ctx->module->sock, buffer, size);
   vc_container_net_status_t net_status;

   net_status = vc_container_net_status(p_ctx->module->sock);
   p_ctx->status = translate_net_status_to_container_status(net_status);

#ifdef IO_NET_CAPTURE_PACKETS
   if (p_ctx->status == VC_CONTAINER_SUCCESS)
      io_net_capture_write_packet(p_ctx->module->read_capture_file, (const char *)buffer, ret);
#endif

   return ret;
}

/*****************************************************************************/
static size_t io_net_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   size_t ret = vc_container_net_write(p_ctx->module->sock, buffer, size);
   vc_container_net_status_t net_status;

   net_status = vc_container_net_status(p_ctx->module->sock);
   p_ctx->status = translate_net_status_to_container_status(net_status);

#ifdef IO_NET_CAPTURE_PACKETS
   if (p_ctx->status == VC_CONTAINER_SUCCESS)
      io_net_capture_write_packet(p_ctx->module->write_capture_file, (const char *)buffer, ret);
#endif

   return ret;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_net_control(struct VC_CONTAINER_IO_T *p_ctx, 
      VC_CONTAINER_CONTROL_T operation,
      va_list args)
{
   vc_container_net_status_t net_status;
   VC_CONTAINER_STATUS_T status;

   switch (operation)
   {
   case VC_CONTAINER_CONTROL_IO_SET_READ_BUFFER_SIZE:
      net_status = vc_container_net_control(p_ctx->module->sock, VC_CONTAINER_NET_CONTROL_SET_READ_BUFFER_SIZE, args);
      break;
   case VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS:
      net_status = vc_container_net_control(p_ctx->module->sock, VC_CONTAINER_NET_CONTROL_SET_READ_TIMEOUT_MS, args);
      break;
   default:
      net_status = VC_CONTAINER_NET_ERROR_NOT_ALLOWED;
   }

   status = translate_net_status_to_container_status(net_status);
   p_ctx->status = status;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_net_open_socket(VC_CONTAINER_IO_T *ctx,
   VC_CONTAINER_IO_MODE_T mode, bool is_udp)
{
   VC_CONTAINER_IO_MODULE_T *module = ctx->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   const char *host, *port;

   /* Treat empty host or port strings as not defined */
   port = vc_uri_port(ctx->uri_parts);
   if (port && !*port)
      port = NULL;

   /* Require the port to be defined */
   if (!port) { status = VC_CONTAINER_ERROR_URI_OPEN_FAILED; goto error; }

   host = vc_uri_host(ctx->uri_parts);
   if (host && !*host)
      host = NULL;

   if (!host)
   {
      /* TCP servers cannot be handled by this interface and UDP senders need a target */
      if (!is_udp || mode == VC_CONTAINER_IO_MODE_WRITE)
      {
         status = VC_CONTAINER_ERROR_URI_OPEN_FAILED;
         goto error;
      }
   }

   module->sock = vc_container_net_open(host, port, is_udp ? 0 : VC_CONTAINER_NET_OPEN_FLAG_STREAM, NULL);
   if (!module->sock) { status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

#ifdef IO_NET_CAPTURE_PACKETS
   if (!is_udp || mode == VC_CONTAINER_IO_MODE_READ)
      module->read_capture_file = io_net_open_capture_file(host, port, is_udp, VC_CONTAINER_IO_MODE_READ);
   if (!is_udp || mode == VC_CONTAINER_IO_MODE_WRITE)
      module->write_capture_file = io_net_open_capture_file(host, port, is_udp, VC_CONTAINER_IO_MODE_WRITE);
#endif

error:
   return status;
}

/*****************************************************************************
Functions exported as part of the I/O Module API
 *****************************************************************************/

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_net_open( VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = 0;
   bool is_udp;
   VC_CONTAINER_PARAM_UNUSED(unused);

   if (!io_net_recognise_scheme(p_ctx->uri, &is_udp))
   { status = VC_CONTAINER_ERROR_URI_NOT_FOUND; goto error; }

   module = (VC_CONTAINER_IO_MODULE_T *)malloc( sizeof(*module) );
   if (!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->module = module;

   status = io_net_open_socket(p_ctx, mode, is_udp);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   p_ctx->pf_close = io_net_close;
   p_ctx->pf_read = io_net_read;
   p_ctx->pf_write = io_net_write;
   p_ctx->pf_control = io_net_control;

   /* Disable caching, as this will block waiting for enough data to fill the cache or an error */
   p_ctx->capabilities = VC_CONTAINER_IO_CAPS_CANT_SEEK;

   return VC_CONTAINER_SUCCESS;

error:
   io_net_close(p_ctx);
   return status;
}
