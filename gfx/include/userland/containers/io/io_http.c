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
#include <stdio.h>
#include <ctype.h>

#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_uri.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_list.h"
#include "containers/core/containers_utils.h"
#include "containers/net/net_sockets.h"

/* Set to 1 if you want to log all HTTP requests */
#define ENABLE_HTTP_EXTRA_LOGGING 0

/******************************************************************************
Defines and constants.
******************************************************************************/

#define IO_HTTP_DEFAULT_PORT     "80"

/** Space for sending requests and receiving responses */
#define COMMS_BUFFER_SIZE              4000

/** Largest allowed HTTP URI. Must be substantially smaller than COMMS_BUFFER_SIZE
 * to allow for the headers that may be sent. */
#define HTTP_URI_LENGTH_MAX            1024

/** Initial capacity of header list */
#define HEADER_LIST_INITIAL_CAPACITY   16

/** Format of the first line of an HTTP request */
#define HTTP_REQUEST_LINE_FORMAT       "%s %s HTTP/1.1\r\nHost: %s\r\n"

/** Format of a range request */
#define HTTP_RANGE_REQUEST             "Range: bytes=%"PRId64"-%"PRId64"\r\n"

/** Format string for common headers used with all request methods.
 * Note: includes double new line to terminate headers */
#define TRAILING_HEADERS_FORMAT        "User-Agent: Broadcom/1.0\r\n\r\n"

/** \name HTTP methods, used as the first item in the request line
 * @{ */
#define GET_METHOD                     "GET"
#define HEAD_METHOD                    "HEAD"
/* @} */

/** \name Names of headers used by the code
 * @{ */
#define CONTENT_LENGTH_NAME            "Content-Length"
#define CONTENT_BASE_NAME              "Content-Base"
#define CONTENT_LOCATION_NAME          "Content-Location"
#define ACCEPT_RANGES_NAME             "Accept-Ranges"
#define CONNECTION_NAME                "Connection"
/* @} */

/** Supported HTTP major version number */
#define HTTP_MAJOR_VERSION             1
/** Supported HTTP minor version number */
#define HTTP_MINOR_VERSION             1

/** Lowest successful status code value */
#define HTTP_STATUS_OK                 200
#define HTTP_STATUS_PARTIAL_CONTENT    206

typedef struct http_header_tag {
   const char *name;
   char *value;
} HTTP_HEADER_T;

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_IO_MODULE_T
{
   VC_CONTAINER_NET_T *sock;
   VC_CONTAINERS_LIST_T *header_list;           /**< Parsed response headers, pointing into comms buffer */

   bool persistent;
   int64_t cur_offset;
   bool reconnecting;

   /* Buffer used for sending and receiving HTTP messages */
   char comms_buffer[COMMS_BUFFER_SIZE];
} VC_CONTAINER_IO_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/

static int io_http_header_comparator(const HTTP_HEADER_T *first, const HTTP_HEADER_T *second);
static VC_CONTAINER_STATUS_T io_http_send(VC_CONTAINER_IO_T *p_ctx);

VC_CONTAINER_STATUS_T vc_container_io_http_open(VC_CONTAINER_IO_T *, const char *,
   VC_CONTAINER_IO_MODE_T);

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Trim whitespace from the end and start of the string
 *
 * \param   str   String to be trimmed
 * \return        Trimmed string
 */
static char *io_http_trim(char *str)
{
   char *s = str + strlen(str);

   /* Search backwards for first non-whitespace */
   while (--s >= str &&(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
      ;     /* Everything done in the while */
   s[1] = '\0';

   /* Now move start of string forwards to first non-whitespace */
   s = str;
   while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
      s++;

   return s;
}

/**************************************************************************//**
 * Header comparison function.
 * Compare two header structures and return whether the first is less than,
 * equal to or greater than the second.
 *
 * @param first   The first structure to be compared.
 * @param second  The second structure to be compared.
 * @return  Negative if first is less than second, positive if first is greater
 *          and zero if they are equal.
 */
static int io_http_header_comparator(const HTTP_HEADER_T *first, const HTTP_HEADER_T *second)
{
   return strcasecmp(first->name, second->name);
}

/**************************************************************************//**
 * Check a response status line to see if the response is usable or not.
 * Reasons for invalidity include:
 *    - Incorrectly formatted
 *    - Unsupported version
 *    - Status code is not in the 2xx range
 *
 * @param status_line   The response status line.
 * @return  The resulting status of the function.
 */
static bool io_http_successful_response_status(const char *status_line)
{
   unsigned int major_version, minor_version, status_code;

   /* coverity[secure_coding] String is null-terminated */
   if (sscanf(status_line, "HTTP/%u.%u %u", &major_version, &minor_version, &status_code) != 3)
   {
      LOG_ERROR(NULL, "HTTP: Invalid response status line:\n%s", status_line);
      return false;
   }

   if (major_version != HTTP_MAJOR_VERSION || minor_version != HTTP_MINOR_VERSION)
   {
      LOG_ERROR(NULL, "HTTP: Unexpected response HTTP version: %u.%u", major_version, minor_version);
      return false;
   }

   if (status_code != HTTP_STATUS_OK && status_code != HTTP_STATUS_PARTIAL_CONTENT)
   {
      LOG_ERROR(NULL, "HTTP: Response status unsuccessful:\n%s", status_line);
      return false;
   }

   return true;
}

/**************************************************************************//**
 * Get the content length header from the response headers as an unsigned
 * 64-bit integer.
 * If the content length header is not found or badly formatted, zero is
 * returned.
 *
 * @param header_list   The response headers.
 * @return  The content length.
 */
static uint64_t io_http_get_content_length(VC_CONTAINERS_LIST_T *header_list)
{
   uint64_t content_length = 0;
   HTTP_HEADER_T header;

   header.name = CONTENT_LENGTH_NAME;
   if (header_list && vc_containers_list_find_entry(header_list, &header))
      /* coverity[secure_coding] String is null-terminated */
      sscanf(header.value, "%"PRIu64, &content_length);

   return content_length;
}

/**************************************************************************//**
 * Get the accept ranges header from the response headers and verify that
 * the server accepts byte ranges..
 * If the accept ranges header is not found false is returned.
 *
 * @param header_list   The response headers.
 * @return  The resulting status of the function.
 */
static bool io_http_check_accept_range(VC_CONTAINERS_LIST_T *header_list)
{
   HTTP_HEADER_T header;

   header.name = ACCEPT_RANGES_NAME;
   if (header_list && vc_containers_list_find_entry(header_list, &header))
   {
      /* coverity[secure_coding] String is null-terminated */
      if (!strcasecmp(header.value, "bytes"))
         return true;
   }

   return false;
}

/**************************************************************************//**
 * Check whether the server supports persistent connections.
 *
 * @param header_list   The response headers.
 * @return  The resulting status of the function.
 */
static bool io_http_check_persistent_connection(VC_CONTAINERS_LIST_T *header_list)
{
   HTTP_HEADER_T header;

   header.name = CONNECTION_NAME;
   if (header_list && vc_containers_list_find_entry(header_list, &header))
   {
      /* coverity[secure_coding] String is null-terminated */
      if (!strcasecmp(header.value, "close"))
         return false;
   }

   return true;
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
static VC_CONTAINER_STATUS_T io_http_open_socket(VC_CONTAINER_IO_T *ctx)
{
   VC_CONTAINER_IO_MODULE_T *module = ctx->module;
   VC_CONTAINER_STATUS_T status;
   const char *host, *port;

   /* Treat empty host or port strings as not defined */
   port = vc_uri_port(ctx->uri_parts);
   if (port && !*port)
      port = NULL;

   /* Require the port to be defined */
   if (!port)
   {
      status = VC_CONTAINER_ERROR_URI_OPEN_FAILED;
      goto error;
   }

   host = vc_uri_host(ctx->uri_parts);
   if (host && !*host)
      host = NULL;

   if (!host)
   {
      status = VC_CONTAINER_ERROR_URI_OPEN_FAILED;
      goto error;
   }

   module->sock = vc_container_net_open(host, port, VC_CONTAINER_NET_OPEN_FLAG_STREAM, NULL);
   if (!module->sock)
   {
      status = VC_CONTAINER_ERROR_URI_NOT_FOUND;
      goto error;
   }

   return VC_CONTAINER_SUCCESS;

error:
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_http_close_socket(VC_CONTAINER_IO_MODULE_T *module)
{
   if (module->sock)
   {
      vc_container_net_close(module->sock);
      module->sock = NULL;
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_http_read_from_net(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   size_t ret;
   vc_container_net_status_t net_status;

   ret           = vc_container_net_read(p_ctx->module->sock, buffer, size);
   net_status    = vc_container_net_status(p_ctx->module->sock);
   p_ctx->status = translate_net_status_to_container_status(net_status);

   return ret;
}

/**************************************************************************//**
 * Reads an HTTP response and parses it into headers and content.
 * The headers and content remain stored in the comms buffer, but referenced
 * by the module's header list. Content uses a special header name that cannot
 * occur in the real headers.
 *
 * @param p_ctx   The HTTP reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T io_http_read_response(VC_CONTAINER_IO_T *p_ctx)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   char *next_read = module->comms_buffer;
   size_t space_available = sizeof(module->comms_buffer) - 1; /* Allow for a NUL */
   char *ptr = next_read;
   bool end_response = false;
   HTTP_HEADER_T header;
   const char endstr[] = "\r\n\r\n";
   int endcount = sizeof(endstr) - 1;
   int endchk = 0;

   vc_containers_list_reset(module->header_list);

   /* Response status line doesn't need to be stored, just checked */
   header.name = NULL;
   header.value = next_read;

   /*
    * We need to read just a byte at a time to make sure that we just read the HTTP response and
    * no more. For example, if a GET operation was requested the file being fetched will also
    * be waiting to be read on the socket.
    */

   while (space_available)
   {
      if (io_http_read_from_net(p_ctx, next_read, 1) != 1)
         break;

      next_read++;
      space_available--;

      if (next_read[-1] == endstr[endchk])
      {
         if (++endchk == endcount)
            break;
      }
      else
         endchk = 0;
   }
   if (!space_available)
   {
      LOG_ERROR(NULL, "comms buffer too small for complete HTTP message (%d)",
                sizeof(module->comms_buffer));
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   *next_read = '\0';

   if (endchk == endcount)
   {
      if (ENABLE_HTTP_EXTRA_LOGGING)
         LOG_DEBUG(NULL, "READ FROM SERVER: %d bytes\n%s\n-----------------------------------------",
                   sizeof(module->comms_buffer) - 1 - space_available, module->comms_buffer);

      while (!end_response && ptr < next_read)
      {
         switch (*ptr)
         {
            case ':':
               if (header.value)
               {
                  /* Just another character in the value */
                  ptr++;
               } else {
                  /* End of name, expect value next */
                  *ptr++ = '\0';
                  header.value = ptr;
               }
               break;

            case '\n':
               if (header.value)
               {
                  /* End of line while parsing the value part of the header, add name/value pair to list */
                  *ptr++ = '\0';
                  header.value = io_http_trim(header.value);
                  if (header.name)
                  {
                     if (!vc_containers_list_insert(module->header_list, &header, false))
                     {
                        LOG_ERROR(NULL, "HTTP: Failed to add <%s> header to list", header.name);
                        return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
                     }
                  } else {
                     /* Check response status line */
                     if (!io_http_successful_response_status(header.value))
                        return VC_CONTAINER_ERROR_FORMAT_INVALID;
                  }
                  /* Ready for next header */
                  header.name  = ptr;
                  header.value = NULL;
               } else {
                  /* End of line while parsing the name of a header */
                  *ptr++ = '\0';
                  if (*header.name && *header.name != '\r')
                  {
                     /* A non-empty name is invalid, so fail */
                     LOG_ERROR(NULL, "HTTP: Invalid name in header - no colon:\n%s", header.name);
                     return VC_CONTAINER_ERROR_FORMAT_INVALID;
                  }

                  /* An empty name signifies the end of the HTTP response */
                  end_response = true;
               }
               break;

            default:
               /* Just another character in either the name or the value */
               ptr++;
         }
      }
   }

   if (!space_available && !end_response)
   {
      /* Ran out of buffer space */
      LOG_ERROR(NULL, "HTTP: Response header section too big");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   return p_ctx->status;
}

/**************************************************************************//**
 * Send a GET request to the HTTP server.
 *
 * @param p_ctx      The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T io_http_send_get_request(VC_CONTAINER_IO_T *p_ctx, size_t size)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   char *ptr = module->comms_buffer, *end = ptr + sizeof(module->comms_buffer);
   int64_t end_offset;

   ptr += snprintf(ptr, end - ptr, HTTP_REQUEST_LINE_FORMAT, GET_METHOD,
                   vc_uri_path(p_ctx->uri_parts), vc_uri_host(p_ctx->uri_parts));

   end_offset = module->cur_offset + size - 1;
   if (end_offset >= p_ctx->size)
      end_offset = p_ctx->size - 1;

   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, HTTP_RANGE_REQUEST, module->cur_offset, end_offset);

   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT);

   if (ptr >= end)
   {
      LOG_ERROR(0, "comms buffer too small (%i/%u)", (int)(end - ptr),
                sizeof(module->comms_buffer));
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   }

   if (ENABLE_HTTP_EXTRA_LOGGING)
      LOG_DEBUG(NULL, "Sending server read request:\n%s\n---------------------\n", module->comms_buffer);
   return io_http_send(p_ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_http_seek(VC_CONTAINER_IO_T *p_ctx, int64_t offset)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;

   /*
    * No seeking past the end of the file.
    */

   if (offset < 0 || offset > p_ctx->size)
   {
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return VC_CONTAINER_ERROR_EOS;
   }

   module->cur_offset = offset;
   p_ctx->status = VC_CONTAINER_SUCCESS;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_http_close(VC_CONTAINER_IO_T *p_ctx)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;

   if (!module)
      return VC_CONTAINER_ERROR_INVALID_ARGUMENT;

   io_http_close_socket(module);
   if (module->header_list)
      vc_containers_list_destroy(module->header_list);

   free(module);
   p_ctx->module = NULL;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static size_t io_http_read(VC_CONTAINER_IO_T *p_ctx, void *buffer, size_t size)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   size_t content_length;
   size_t bytes_read;
   size_t ret = 0;
   char *ptr = buffer;

   /*
    * Are we at the end of the file?
    */

   if (module->cur_offset >= p_ctx->size)
   {
      p_ctx->status = VC_CONTAINER_ERROR_EOS;
      return 0;
   }

   if (!module->persistent)
   {
      status = io_http_open_socket(p_ctx);
      if (status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(NULL, "Error opening socket for GET request");
         return status;
      }
   }

   /* Send GET request and get response */
   status = io_http_send_get_request(p_ctx, size);
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_ERROR(NULL, "Error sending GET request");
      goto error;
   }

   status = io_http_read_response(p_ctx);
   if (status == VC_CONTAINER_ERROR_EOS && !module->reconnecting)
   {
      LOG_DEBUG(NULL, "reconnecting");
      io_http_close_socket(module);
      status = io_http_open_socket(p_ctx);
      if (status == VC_CONTAINER_SUCCESS)
      {
         module->reconnecting = true;
         status = io_http_read(p_ctx, buffer, size);
         module->reconnecting = false;
         return status;
      }
   }
   if (status != VC_CONTAINER_SUCCESS)
   {
      LOG_ERROR(NULL, "Error reading GET response");
      goto error;
   }

   /*
    * How much data is the server offering us?
    */

   content_length = (size_t)io_http_get_content_length(module->header_list);
   if (content_length > size)
   {
      LOG_ERROR(NULL, "received too much data (%i/%i)",
                (int)content_length, (int)size);
      status = VC_CONTAINER_ERROR_CORRUPTED;
      goto error;
   }

   bytes_read = 0;
   while (bytes_read < content_length && p_ctx->status == VC_CONTAINER_SUCCESS)
   {
      ret = io_http_read_from_net(p_ctx, ptr, content_length - bytes_read);
      if (p_ctx->status == VC_CONTAINER_SUCCESS)
      {
         bytes_read += ret;
         ptr += ret;
      }
   }

   if (p_ctx->status == VC_CONTAINER_SUCCESS)
   {
      module->cur_offset += bytes_read;
      ret = bytes_read;
   }

   if (!module->persistent)
      io_http_close_socket(module);

   return ret;

error:
   if (!module->persistent)
      io_http_close_socket(module);

   return status;
}

/*****************************************************************************/
static size_t io_http_write(VC_CONTAINER_IO_T *p_ctx, const void *buffer, size_t size)
{
   size_t ret = vc_container_net_write(p_ctx->module->sock, buffer, size);
   vc_container_net_status_t net_status;

   net_status = vc_container_net_status(p_ctx->module->sock);
   p_ctx->status = translate_net_status_to_container_status(net_status);

   return ret;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T io_http_control(struct VC_CONTAINER_IO_T *p_ctx,
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

/**************************************************************************//**
 * Send out the data in the comms buffer.
 *
 * @param p_ctx      The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T io_http_send(VC_CONTAINER_IO_T *p_ctx)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   size_t to_write;
   size_t written;
   const char *buffer = module->comms_buffer;

   to_write = strlen(buffer);

   while (to_write)
   {
      written = io_http_write(p_ctx, buffer, to_write);
      if (p_ctx->status != VC_CONTAINER_SUCCESS)
         break;

      to_write -= written;
      buffer   += written;
   }

   return p_ctx->status;
}

/**************************************************************************//**
 * Send a HEAD request to the HTTP server.
 *
 * @param p_ctx      The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T io_http_send_head_request(VC_CONTAINER_IO_T *p_ctx)
{
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   char *ptr = module->comms_buffer, *end = ptr + sizeof(module->comms_buffer);

   ptr += snprintf(ptr, end - ptr, HTTP_REQUEST_LINE_FORMAT, HEAD_METHOD,
                   vc_uri_path(p_ctx->uri_parts), vc_uri_host(p_ctx->uri_parts));
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT);

   if (ptr >= end)
   {
      LOG_ERROR(0, "comms buffer too small (%i/%u)", (int)(end - ptr),
                sizeof(module->comms_buffer));
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
   }

   return io_http_send(p_ctx);
}

static VC_CONTAINER_STATUS_T io_http_head(VC_CONTAINER_IO_T *p_ctx)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = p_ctx->module;
   uint64_t content_length;

   /* Send HEAD request and get response */
   status = io_http_send_head_request(p_ctx);
   if (status != VC_CONTAINER_SUCCESS)
      return status;
   status = io_http_read_response(p_ctx);
   if (status != VC_CONTAINER_SUCCESS)
      return status;

   /*
    * Save the content length since that's our file size.
    */

   content_length = io_http_get_content_length(module->header_list);
   if (content_length)
   {
      p_ctx->size = content_length;
      LOG_DEBUG(NULL, "File size is %"PRId64, p_ctx->size);
   }

   /*
    * Now make sure that the server supports byte range requests.
    */

   if (!io_http_check_accept_range(module->header_list))
   {
      LOG_ERROR(NULL, "Server doesn't support byte range requests");
      return VC_CONTAINER_ERROR_FAILED;
   }

   /*
    * Does it support persistent connections?
    */

   if (io_http_check_persistent_connection(module->header_list))
   {
      module->persistent = true;
   }
   else
   {
      LOG_DEBUG(NULL, "Server does not support persistent connections");
      io_http_close_socket(module);
   }

   module->cur_offset = 0;

   return status;
}

/*****************************************************************************
Functions exported as part of the I/O Module API
 *****************************************************************************/

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_io_http_open(VC_CONTAINER_IO_T *p_ctx,
   const char *unused, VC_CONTAINER_IO_MODE_T mode)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_IO_MODULE_T *module = 0;
   VC_CONTAINER_PARAM_UNUSED(unused);

   /* Check the URI to see if we're dealing with an http stream */
   if (!vc_uri_scheme(p_ctx->uri_parts) ||
       strcasecmp(vc_uri_scheme(p_ctx->uri_parts), "http"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /*
    * Some basic error checking.
    */

   if (mode == VC_CONTAINER_IO_MODE_WRITE)
   {
      status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
      goto error;
   }

   if (strlen(p_ctx->uri) > HTTP_URI_LENGTH_MAX)
   {
      status = VC_CONTAINER_ERROR_URI_OPEN_FAILED;
      goto error;
   }

   module = calloc(1, sizeof(*module));
   if (!module)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }
   p_ctx->module = module;

   /* header_list will contain pointers into the response_buffer, so take care in re-use */
   module->header_list = vc_containers_list_create(HEADER_LIST_INITIAL_CAPACITY, sizeof(HTTP_HEADER_T),
                                           (VC_CONTAINERS_LIST_COMPARATOR_T)io_http_header_comparator);
   if (!module->header_list)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }

   /*
    * Make sure that we have a port number.
    */

   if (vc_uri_port(p_ctx->uri_parts) == NULL)
      vc_uri_set_port(p_ctx->uri_parts, IO_HTTP_DEFAULT_PORT);

   status = io_http_open_socket(p_ctx);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   /*
    * Whoo hoo! Our socket is open. Now let's send a HEAD request.
    */

   status = io_http_head(p_ctx);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   p_ctx->pf_close   = io_http_close;
   p_ctx->pf_read    = io_http_read;
   p_ctx->pf_write   = NULL;
   p_ctx->pf_control = io_http_control;
   p_ctx->pf_seek    = io_http_seek;

   p_ctx->capabilities = VC_CONTAINER_IO_CAPS_NO_CACHING;
   p_ctx->capabilities |= VC_CONTAINER_IO_CAPS_SEEK_SLOW;

   return VC_CONTAINER_SUCCESS;

error:
   io_http_close(p_ctx);
   return status;
}
