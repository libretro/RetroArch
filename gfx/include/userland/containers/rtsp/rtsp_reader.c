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
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define CONTAINER_IS_BIG_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_list.h"
#include "containers/core/containers_uri.h"

/******************************************************************************
Configurable defines and constants.
******************************************************************************/

/** Maximum number of tracks allowed in an RTSP reader */
#define RTSP_TRACKS_MAX                4

/** Space for sending requests and receiving responses */
#define COMMS_BUFFER_SIZE              2048

/** Largest allowed RTSP URI. Must be substantially smaller than COMMS_BUFFER_SIZE
 * to allow for the headers that may be sent. */
#define RTSP_URI_LENGTH_MAX            1024

/** Maximum allowed length for the Session: header recevied in a SETUP response,
 * This is to ensure comms buffer is not overflowed. */
#define SESSION_HEADER_LENGTH_MAX      100

/** Number of milliseconds to block trying to read from the RTSP stream when no
 * data is available from any of the tracks */
#define DATA_UNAVAILABLE_READ_TIMEOUT_MS  1

/** Size of buffer for each track to use when receiving packets */
#define UDP_READ_BUFFER_SIZE           520000

/* Arbitrary number of different dynamic ports to try */
#define DYNAMIC_PORT_ATTEMPTS_MAX      16

/******************************************************************************
Defines and constants.
******************************************************************************/

#define RTSP_SCHEME                    "rtsp:"
#define RTP_SCHEME                     "rtp"

/** The RTSP PKT scheme is used with test pkt files */
#define RTSP_PKT_SCHEME                "rtsppkt:"

#define RTSP_NETWORK_URI_START         "rtsp://"
#define RTSP_NETWORK_URI_START_LENGTH  (sizeof(RTSP_NETWORK_URI_START)-1)

/** Initial capacity of header list */
#define HEADER_LIST_INITIAL_CAPACITY   16

/** Format of the first line of an RTSP request */
#define RTSP_REQUEST_LINE_FORMAT       "%s %s RTSP/1.0\r\n"

/** Format string for common headers used with all request methods.
 * Note: includes double new line to terminate headers */
#define TRAILING_HEADERS_FORMAT        "CSeq: %u\r\nConnection: Keep-Alive\r\nUser-Agent: Broadcom/1.0\r\n\r\n"

/** Format for the Transport: header */
#define TRANSPORT_HEADER_FORMAT        "Transport: RTP/AVP;unicast;client_port=%hu-%hu;mode=play\r\n"

/** Format for including Session: header. */
#define SESSION_HEADER_FORMAT          "Session: %s\r\n"

/** \name RTSP methods, used as the first item in the request line
 * @{ */
#define DESCRIBE_METHOD                "DESCRIBE"
#define SETUP_METHOD                   "SETUP"
#define PLAY_METHOD                    "PLAY"
#define TEARDOWN_METHOD                "TEARDOWN"
/* @} */

/** \name Names of headers used by the code
 * @{ */
#define CONTENT_PSEUDOHEADER_NAME      ":"
#define CONTENT_LENGTH_NAME            "Content-Length"
#define CONTENT_BASE_NAME              "Content-Base"
#define CONTENT_LOCATION_NAME          "Content-Location"
#define RTP_INFO_NAME                  "RTP-Info"
#define SESSION_NAME                   "Session"
/* @} */

/** Supported RTSP major version number */
#define RTSP_MAJOR_VERSION             1
/** Supported RTSP minor version number */
#define RTSP_MINOR_VERSION             0

/** Lowest successful status code value */
#define RTSP_STATUS_OK                 200
/** Next failure status code after the set of successful ones */
#define RTSP_STATUS_MULTIPLE_CHOICES   300

/** Maximum size of a decimal string representation of a uint16_t, plus NUL */
#define PORT_BUFFER_SIZE               6
/** Start of private / dynamic port region */
#define FIRST_DYNAMIC_PORT             0xC000
/** End of private / dynamic port region */
#define LAST_DYNAMIC_PORT              0xFFF0

/** Format of RTP track file extension */
#define RTP_PATH_EXTENSION_FORMAT      ".t%u.pkt"
/** Extra space need for creating an RTP track file name from an RTSP URI path */
#define RTP_PATH_EXTRA                 17

/** \name RTP URI parameter names
 * @{ */
#define PAYLOAD_TYPE_NAME              "rtppt"
#define MIME_TYPE_NAME                 "mime-type"
#define SAMPLE_RATE_NAME               "rate"
#define CHANNELS_NAME                  "channels"
/* @} */

/** Largest signed 64-bit integer */
#define MAXIMUM_INT64                  (int64_t)((1ULL << 63) - 1)

/******************************************************************************
Type definitions
******************************************************************************/

typedef int (*PARSE_IS_DELIMITER_FN_T)(int char_to_test);

typedef struct rtsp_header_tag
{
   const char *name;
   char *value;
} RTSP_HEADER_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   VC_CONTAINER_T *reader;          /**< RTP reader for track */
   VC_URI_PARTS_T *reader_uri;      /**< URI built up from SDP and used to open reader */
   char *control_uri;               /**< URI used to control track playback */
   char *session_header;            /**< Session header to be used when sending control requests */
   char *payload_type;              /**< RTP payload type for track */
   char *media_type;                /**< MIME type for track */
   VC_CONTAINER_PACKET_T info;      /**< Latest track packet info block */
   unsigned short rtp_port;       /**< UDP listener port being used in RTP reader */
} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *tracks[RTSP_TRACKS_MAX];
   char *comms_buffer;                          /**< Buffer used for sending and receiving RTSP messages */
   VC_CONTAINERS_LIST_T *header_list;           /**< Parsed response headers, pointing into comms buffer */
   uint32_t cseq_value;                         /**< CSeq header value for next request */
   uint16_t next_rtp_port;                      /**< Next RTP port to use when opening track reader */
   uint16_t media_item;                         /**< Current media item number during initialization */
   bool uri_has_network_info;                   /**< True if the RTSP URI contains network info */
   int64_t ts_base;                             /**< Base value for dts and pts */
   VC_CONTAINER_TRACK_MODULE_T *current_track;  /**< Next track to be read, to keep info/data on same track */
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
static int rtsp_header_comparator(const RTSP_HEADER_T *first, const RTSP_HEADER_T *second);

VC_CONTAINER_STATUS_T rtsp_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Trim whitespace from the end and start of the string
 *
 * \param   str   String to be trimmed
 * \return        Trimmed string
 */
static char *rtsp_trim( char *str )
{
   char *trim = str + strlen(str);

   /* Search backwards for first non-whitespace */
   while (--trim >= str && isspace((int)*trim))
      ;     /* Everything done in the while */
   trim[1] = '\0';

   /* Now move start of string forwards to first non-whitespace */
   trim = str;
   while (isspace((int)*trim))
      trim++;

   return trim;
}

/**************************************************************************//**
 * Send out the data in the comms buffer.
 *
 * @param p_ctx      The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_send( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   uint32_t to_write;
   uint32_t written;
   const char *buffer = module->comms_buffer;

   /* When reading from a captured file, do not attempt to send data */
   if (!module->uri_has_network_info)
      return VC_CONTAINER_SUCCESS;

   to_write = strlen(buffer);

   while (to_write)
   {
      written = vc_container_io_write(p_ctx->priv->io, buffer, to_write);
      if (!written)
         break;
      to_write -= written;
      buffer += written;
   }

   return p_ctx->priv->io->status;
}

/**************************************************************************//**
 * Send a DESCRIBE request to the RTSP server.
 *
 * @param p_ctx      The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_send_describe_request( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   char *ptr = module->comms_buffer, *end = ptr + COMMS_BUFFER_SIZE;
   char *uri = p_ctx->priv->io->uri;

   if (strlen(uri) > RTSP_URI_LENGTH_MAX)
   {
      LOG_ERROR(p_ctx, "RTSP: URI is too long (%d>%d)", strlen(uri), RTSP_URI_LENGTH_MAX);
      return VC_CONTAINER_ERROR_URI_OPEN_FAILED;
   }

   ptr += snprintf(ptr, end - ptr, RTSP_REQUEST_LINE_FORMAT, DESCRIBE_METHOD, uri);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT, module->cseq_value++);
   vc_container_assert(ptr < end);

   return rtsp_send(p_ctx);
}

/**************************************************************************//**
 * Send a SETUP request to the RTSP server.
 *
 * @param p_ctx      The reader context.
 * @param t_module   The track module relating to the SETUP.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_send_setup_request( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   char *ptr = module->comms_buffer, *end = ptr + COMMS_BUFFER_SIZE;
   char *uri = t_module->control_uri;

   if (strlen(uri) > RTSP_URI_LENGTH_MAX)
   {
      LOG_ERROR(p_ctx, "RTSP: Control URI is too long (%d>%d)", strlen(uri), RTSP_URI_LENGTH_MAX);
      return VC_CONTAINER_ERROR_URI_OPEN_FAILED;
   }

   ptr += snprintf(ptr, end - ptr, RTSP_REQUEST_LINE_FORMAT, SETUP_METHOD, uri);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRANSPORT_HEADER_FORMAT, t_module->rtp_port, t_module->rtp_port + 1);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT, module->cseq_value++);
   vc_container_assert(ptr < end);

   return rtsp_send(p_ctx);
}

/**************************************************************************//**
 * Send a PLAY request to the RTSP server.
 *
 * @param p_ctx      The reader context.
 * @param t_module   The track module relating to the PLAY.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_send_play_request( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   char *ptr = module->comms_buffer, *end = ptr + COMMS_BUFFER_SIZE;
   char *uri = t_module->control_uri;

   if (strlen(uri) > RTSP_URI_LENGTH_MAX)
   {
      LOG_ERROR(p_ctx, "RTSP: Control URI is too long (%d>%d)", strlen(uri), RTSP_URI_LENGTH_MAX);
      return VC_CONTAINER_ERROR_URI_OPEN_FAILED;
   }

   ptr += snprintf(ptr, end - ptr, RTSP_REQUEST_LINE_FORMAT, PLAY_METHOD, uri);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, SESSION_HEADER_FORMAT, t_module->session_header);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT, module->cseq_value++);
   vc_container_assert(ptr < end);

   return rtsp_send(p_ctx);
}

/**************************************************************************//**
 * Send a TEARDOWN request to the RTSP server.
 *
 * @param p_ctx      The reader context.
 * @param t_module   The track module relating to the SETUP.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_send_teardown_request( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   char *ptr = module->comms_buffer, *end = ptr + COMMS_BUFFER_SIZE;
   char *uri = t_module->control_uri;

   if (strlen(uri) > RTSP_URI_LENGTH_MAX)
   {
      LOG_ERROR(p_ctx, "RTSP: Control URI is too long (%d>%d)", strlen(uri), RTSP_URI_LENGTH_MAX);
      return VC_CONTAINER_ERROR_URI_OPEN_FAILED;
   }

   ptr += snprintf(ptr, end - ptr, RTSP_REQUEST_LINE_FORMAT, TEARDOWN_METHOD, uri);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, SESSION_HEADER_FORMAT, t_module->session_header);
   if (ptr < end)
      ptr += snprintf(ptr, end - ptr, TRAILING_HEADERS_FORMAT, module->cseq_value++);
   vc_container_assert(ptr < end);

   return rtsp_send(p_ctx);
}

/**************************************************************************//**
 * Check a response status line to see if the response is usable or not.
 * Reasons for invalidity include:
 *    - Incorrectly formatted
 *    - Unsupported version
 *    - Status code is not in the 2xx range
 *
 * @param p_ctx         The reader context.
 * @param status_line   The response status line.
 * @return  The resulting status of the function.
 */
static bool rtsp_successful_response_status( VC_CONTAINER_T *p_ctx,
      const char *status_line)
{
   unsigned int major_version, minor_version, status_code;

   /* coverity[secure_coding] String is null-terminated */
   if (sscanf(status_line, "RTSP/%u.%u %u", &major_version, &minor_version, &status_code) != 3)
   {
      LOG_ERROR(p_ctx, "RTSP: Invalid response status line:\n%s", status_line);
      return false;
   }

   if (major_version != RTSP_MAJOR_VERSION || minor_version != RTSP_MINOR_VERSION)
   {
      LOG_ERROR(p_ctx, "RTSP: Unexpected response RTSP version: %u.%u", major_version, minor_version);
      return false;
   }

   if (status_code < RTSP_STATUS_OK || status_code >= RTSP_STATUS_MULTIPLE_CHOICES)
   {
      LOG_ERROR(p_ctx, "RTSP: Response status unsuccessful:\n%s", status_line);
      return false;
   }

   return true;
}

/**************************************************************************//**
 * Get the content length header from the response headers as an unsigned
 * 32-bit integer.
 * If the content length header is not found or badly formatted, zero is
 * returned.
 *
 * @param header_list   The response headers.
 * @return  The content length.
 */
static uint32_t rtsp_get_content_length( VC_CONTAINERS_LIST_T *header_list )
{
   unsigned int content_length = 0;
   RTSP_HEADER_T header;

   header.name = CONTENT_LENGTH_NAME;
   if (header_list && vc_containers_list_find_entry(header_list, &header))
      /* coverity[secure_coding] String is null-terminated */
      sscanf(header.value, "%u", &content_length);

   return content_length;
}

/**************************************************************************//**
 * Get the session header from the response headers.
 * If the session header is not found, the empty string is returned.
 *
 * @param header_list   The response headers.
 * @return  The session header.
 */
static const char *rtsp_get_session_header(VC_CONTAINERS_LIST_T *header_list)
{
   RTSP_HEADER_T header;

   header.name = SESSION_NAME;
   if (header_list && vc_containers_list_find_entry(header_list, &header))
      return header.value;

   return "";
}

/**************************************************************************//**
 * Returns pointer to the string with any leading whitespace trimmed and
 * terminated by a delimiter, as determined by is_delimiter_fn. The delimiter
 * character replaced by NUL (to terminate the string) is returned in the
 * variable pointed at by p_delimiter_replaced, if it is not NULL.
 *
 * The parse_str pointer is moved on to the character after the delimiter, or
 * the end of the string if it is reached first.
 *
 * @param parse_str              Pointer to the string pointer to parse.
 * @param is_delimiter_fn        Function to test if a character is a delimiter or not.
 * @param p_delimiter_replaced   Pointer to variable to receive delimiter character, or NULL.
 * @return  Pointer to extracted string.
 */
static char *rtsp_parse_extract(char **parse_str,
      PARSE_IS_DELIMITER_FN_T is_delimiter_fn,
      char *p_delimiter_replaced)
{
   char *ptr;
   char *result;

   vc_container_assert(parse_str);
   vc_container_assert(*parse_str);
   vc_container_assert(is_delimiter_fn);

   ptr = *parse_str;

   while (isspace((int)*ptr))
      ptr++;

   result = ptr;

   while (*ptr && !(*is_delimiter_fn)(*ptr))
      ptr++;
   if (p_delimiter_replaced)
      *p_delimiter_replaced = *ptr;
   if (*ptr)
      *ptr++ = '\0';

   *parse_str = ptr;
   return result;
}

/**************************************************************************//**
 * Specialised form of rtsp_parse_extract() where the delimiter is whitespace.
 * Returns pointer to the string with any leading whitespace trimmed and
 * terminated by further whitespace.
 *
 * The parse_str pointer is moved on to the character after the delimiter, or
 * the end of the string if it is reached first.
 *
 * @param parse_str  Pointer to the string pointer to parse.
 * @return  Pointer to extracted string.
 */
static char *rtsp_parse_extract_ws(char **parse_str)
{
   char *ptr;
   char *result;

   vc_container_assert(parse_str);
   vc_container_assert(*parse_str);

   ptr = *parse_str;

   while (isspace((int)*ptr))
      ptr++;

   result = ptr;

   while (*ptr && !isspace((int)*ptr))
      ptr++;
   if (*ptr)
      *ptr++ = '\0';

   *parse_str = ptr;
   return result;
}

/**************************************************************************//**
 * Returns whether the given character is a parameter name delimiter or not.
 *
 * @param char_to_test  The character under test.
 * @return  True if the character is a name delimiter, false if not.
 */
static int name_delimiter_fn(int char_to_test)
{
   switch (char_to_test)
   {
   case ' ':
   case '\t':
   case '=':
   case ';':
      return true;
   default:
      return false;
   }
}

/**************************************************************************//**
 * Returns whether the given character is a parameter value delimiter or not.
 *
 * @param char_to_test  The character under test.
 * @return  True if the character is a value delimiter, false if not.
 */
static int value_delimiter_fn(int char_to_test)
{
   switch (char_to_test)
   {
   case ' ':
   case '\t':
   case ';':
      return true;
   default:
      return false;
   }
}

/**************************************************************************//**
 * Extract a name/value pair from a given string.
 * Each pair consists of a name, optionally followed by '=' and a value, with
 * optional whitespace around either or both name and value. The parameter is
 * terminated by a semi-colon, ';'.
 *
 * The parse_str pointer is moved on to the next parameter, or the end of the
 * string if that is reached first.
 *
 * Name can be empty if there are two consecutive semi-colons, or a trailing
 * semi-colon.
 *
 * @param parse_str  Pointer to the string pointer to be parsed.
 * @param p_name     Pointer to where name string pointer shall be written.
 * @param p_value    Pointer to where value string pointer shall be written.
 * @return  True if the name is not empty.
 */
static bool rtsp_parse_extract_parameter(char **parse_str, char **p_name, char **p_value)
{
   char delimiter;

   vc_container_assert(parse_str);
   vc_container_assert(*parse_str);
   vc_container_assert(p_name);
   vc_container_assert(p_value);

   /* General form of each parameter:
      *    <name>[=<value>]
      * but allow for spaces before and after name and value */
   *p_name = rtsp_parse_extract(parse_str, name_delimiter_fn, &delimiter);
   if (isspace((int)delimiter))
   {
      /* Skip further spaces after parameter name */
      do {
         delimiter = **parse_str;
         if (delimiter)
            (*parse_str)++;
      } while (isspace((int)delimiter));
   }

   if (delimiter == '=')
   {
      /* Parameter value present (although may be empty) */
      *p_value = rtsp_parse_extract(parse_str, value_delimiter_fn, &delimiter);
      if (isspace((int)delimiter))
      {
         /* Skip spaces after parameter value */
         do {
            delimiter = **parse_str;
            if (delimiter)
               (*parse_str)++;
         } while (isspace((int)delimiter));
      }
   } else {
      *p_value = NULL;
   }

   return (**p_name != '\0');
}

/**************************************************************************//**
 * Parses RTP-Info header and stores relevant parts.
 *
 * @param header_list   The response header list.
 * @param t_module      The track module relating to the response headers.
 */
static void rtsp_store_rtp_info(VC_CONTAINERS_LIST_T *header_list,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   RTSP_HEADER_T header;
   char *ptr;

   header.name = RTP_INFO_NAME;
   if (!vc_containers_list_find_entry(header_list, &header))
      return;

   ptr = header.value;
   while (ptr && *ptr)
   {
      char *name;
      char *value;

      if (!rtsp_parse_extract_parameter(&ptr, &name, &value))
         continue;

      if (strcasecmp(name, "rtptime") == 0)
      {
         unsigned int timestamp_base = 0;

         /* coverity[secure_coding] String is null-terminated */
         if (sscanf(value, "%u", &timestamp_base) == 1)
            (void)vc_container_control(t_module->reader, VC_CONTAINER_CONTROL_SET_TIMESTAMP_BASE, timestamp_base);
      }
      else if (strcasecmp(name, "seq") == 0)
      {
         unsigned short int sequence_number = 0;

         /* coverity[secure_coding] String is null-terminated */
         if (sscanf(value, "%hu", &sequence_number) == 1)
            (void)vc_container_control(t_module->reader, VC_CONTAINER_CONTROL_SET_NEXT_SEQUENCE_NUMBER, (uint32_t)sequence_number);
      }
   }
}

/**************************************************************************//**
 * Reads an RTSP response and parses it into headers and content.
 * The headers and content remain stored in the comms buffer, but referenced
 * by the module's header list. Content uses a special header name that cannot
 * occur in the real headers.
 *
 * @param p_ctx   The RTSP reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_read_response( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_IO_T *p_ctx_io = p_ctx->priv->io;
   char *next_read = module->comms_buffer;
   uint32_t space_available = COMMS_BUFFER_SIZE - 1;     /* Allow for a NUL */
   uint32_t received;
   char *ptr = next_read;
   bool found_content = false;
   RTSP_HEADER_T header;

   vc_containers_list_reset(module->header_list);

   /* Response status line doesn't need to be stored, just checked */
   header.name = NULL;
   header.value = next_read;

   while (space_available)
   {
      received = vc_container_io_read(p_ctx_io, next_read, space_available);
      if (p_ctx_io->status != VC_CONTAINER_SUCCESS)
         break;

      next_read += received;
      space_available -= received;

      while (!found_content && ptr < next_read)
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
               header.value = rtsp_trim(header.value);
               if (header.name)
               {
                  if (!vc_containers_list_insert(module->header_list, &header, false))
                  {
                     LOG_ERROR(p_ctx, "RTSP: Failed to add <%s> header to list", header.name);
                     return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
                  }
               } else {
                  /* Check response status line */
                  if (!rtsp_successful_response_status(p_ctx, header.value))
                     return VC_CONTAINER_ERROR_FORMAT_INVALID;
               }
               /* Ready for next header */
               header.name = ptr;
               header.value = NULL;
            } else {
               uint32_t content_length;

               /* End of line while parsing the name of a header */
               *ptr++ = '\0';
               if (*header.name && *header.name != '\r')
               {
                  /* A non-empty name is invalid, so fail */
                  LOG_ERROR(p_ctx, "RTSP: Invalid name in header - no colon:\n%s", header.name);
                  return VC_CONTAINER_ERROR_FORMAT_INVALID;
               }

               /* An empty name signifies the start of the content has been found */
               found_content = true;

               /* Make a pseudo-header for the content and add it to the list */
               header.name = CONTENT_PSEUDOHEADER_NAME;
               header.value = ptr;
               if (!vc_containers_list_insert(module->header_list, &header, false))
               {
                  LOG_ERROR(p_ctx, "RTSP: Failed to add content pseudoheader to list");
                  return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
               }

               /* Calculate how much content there is left to read, based on Content-Length header */
               content_length = rtsp_get_content_length(module->header_list);
               if (ptr + content_length < next_read)
               {
                  /* Final content byte already present, with extra data after it */
                  space_available = 0;
               } else {
                  uint32_t content_to_read = content_length - (next_read - ptr);

                  if (content_to_read >= space_available)
                  {
                     LOG_ERROR(p_ctx, "RTSP: Not enough room to read content");
                     return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
                  }

                  /* Restrict further reading to the number of content bytes left */
                  space_available = content_to_read;
               }
            }
            break;

         default:
            /* Just another character in either the name or the value */
            ptr++;
         }
      }
   }

   if (!space_available)
   {
      if (found_content)
      {
         /* Terminate content region */
         *next_read = '\0';
      } else {
         /* Ran out of buffer space and never found the content */
         LOG_ERROR(p_ctx, "RTSP: Response header section too big / content missing");
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
   }

   return p_ctx_io->status;
}

/**************************************************************************//**
 * Creates a new track from an SDP media field.
 * Limitation: only the first payload type of the field is used.
 *
 * @param p_ctx   The RTSP reader context.
 * @param media   The media field.
 * @param p_track Pointer to the variable to receive the new track pointer.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_create_track_for_media_field(VC_CONTAINER_T *p_ctx,
      char *media,
      VC_CONTAINER_TRACK_T **p_track )
{
   VC_CONTAINER_TRACK_T *track = NULL;
   VC_CONTAINER_TRACK_MODULE_T *t_module = NULL;
   char *ptr = media;
   char *media_type;
   char *rtp_port;
   char *transport_type;
   char *payload_type;

   *p_track = NULL;
   if (p_ctx->tracks_num == RTSP_TRACKS_MAX)
   {
      LOG_DEBUG(p_ctx, "RTSP: Too many media items in SDP data, only %d are supported.", RTSP_TRACKS_MAX);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   /* Format of media item:
    *    m=<media type> <port> <transport> <payload type(s)>
    * Only RTP/AVP transport and the first payload type are supported */
   media_type = rtsp_parse_extract_ws(&ptr);
   rtp_port = rtsp_parse_extract_ws(&ptr);
   transport_type = rtsp_parse_extract_ws(&ptr);
   payload_type = rtsp_parse_extract_ws(&ptr);
   if (!*media_type || !*rtp_port || strcmp(transport_type, "RTP/AVP") || !*payload_type)
   {
      LOG_ERROR(p_ctx, "RTSP: Failure to parse media field");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   track = vc_container_allocate_track(p_ctx, sizeof(VC_CONTAINER_TRACK_MODULE_T));
   if (!track) goto out_of_memory_error;
   t_module = track->priv->module;

   /* If the port specifier is invalid, treat it as if it were zero */
   /* coverity[secure_coding] String is null-terminated */
   sscanf(rtp_port, "%hu", &t_module->rtp_port);
   t_module->payload_type = payload_type;
   t_module->media_type = media_type;

   t_module->reader_uri = vc_uri_create();
   if (!t_module->reader_uri) goto out_of_memory_error;
   if (!vc_uri_set_scheme(t_module->reader_uri, RTP_SCHEME)) goto out_of_memory_error;
   if (!vc_uri_add_query(t_module->reader_uri, PAYLOAD_TYPE_NAME, payload_type)) goto out_of_memory_error;

   p_ctx->tracks[p_ctx->tracks_num++] = track;
   *p_track = track;
   return VC_CONTAINER_SUCCESS;

out_of_memory_error:
   if (track)
   {
      if (t_module->reader_uri)
         vc_uri_release(t_module->reader_uri);
      vc_container_free_track(p_ctx, track);
   }
   LOG_ERROR(p_ctx, "RTSP: Memory allocation failure creating track");
   return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
}

/**************************************************************************//**
 * Returns whether the given character is a slash or not.
 *
 * @param char_to_test  The character under test.
 * @return  True if the character is a slash, false if not.
 */
static int slash_delimiter_fn(int char_to_test)
{
   return char_to_test == '/';
}

/**************************************************************************//**
 * Parse an rtpmap attribute and store values in the related track.
 *
 * @param p_ctx      The RTSP reader context.
 * @param track      The track relating to the rtpmap.
 * @param attribute  The rtpmap attribute value.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_parse_rtpmap_attribute( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      char *attribute )
{
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   char *ptr = attribute;
   char *payload_type;
   char *mime_sub_type;
   char *sample_rate;
   char *full_mime_type;
   char *channels;

   /* rtpmap attribute format:
    *    <payload type> <MIME type>/<sample rate>[/<channels>]
    * Payload type must match the one used in the media field */
   payload_type = rtsp_parse_extract_ws(&ptr);
   if (strcmp(payload_type, t_module->payload_type))
   {
      /* Ignore any unsupported secondary payload type attributes */
      LOG_DEBUG(p_ctx, "RTSP: Secondary payload type attribute - not supported");
      return VC_CONTAINER_SUCCESS;
   }

   mime_sub_type = rtsp_parse_extract(&ptr, slash_delimiter_fn, NULL);
   if (!*mime_sub_type)
   {
      LOG_ERROR(p_ctx, "RTSP: rtpmap: MIME type missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   sample_rate = rtsp_parse_extract(&ptr, slash_delimiter_fn, NULL);
   if (!*sample_rate)
   {
      LOG_ERROR(p_ctx, "RTSP: rtpmap: sample rate missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   full_mime_type = (char *)malloc(strlen(t_module->media_type) + strlen(mime_sub_type) + 2);
   if (!full_mime_type)
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to allocate space for full MIME type");
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   }
   /* coverity[secure_coding] String has been allocated of the right size */
   sprintf(full_mime_type, "%s/%s", t_module->media_type, mime_sub_type);
   if (!vc_uri_add_query(t_module->reader_uri, MIME_TYPE_NAME, full_mime_type))
   {
      free(full_mime_type);
      LOG_ERROR(p_ctx, "RTSP: Failed to add MIME type to URI");
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   }
   free(full_mime_type);

   if (!vc_uri_add_query(t_module->reader_uri, SAMPLE_RATE_NAME, sample_rate))
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to add sample rate to URI");
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   }

   /* Optional channels specifier */
   channels = rtsp_parse_extract_ws(&ptr);
   if (*channels)
   {
      if (!vc_uri_add_query(t_module->reader_uri, CHANNELS_NAME, channels))
      {
         LOG_ERROR(p_ctx, "RTSP: Failed to add channels to URI");
         return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      }
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Parse an fmtp attribute and store values in the related track.
 *
 * @param p_ctx      The RTSP reader context.
 * @param track      The track relating to the fmtp.
 * @param attribute  The fmtp attribute value.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_parse_fmtp_attribute( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      char *attribute )
{
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   char *ptr = attribute;
   char *payload_type;

   /* fmtp attribute format:
    *    <payload type> <parameters>
    * The payload type must match the first one in the media field, parameters
    * are semi-colon separated and may have additional whitespace around them. */

   payload_type = rtsp_parse_extract_ws(&ptr);
   if (strcmp(payload_type, t_module->payload_type))
   {
      /* Ignore any unsupported secondary payload type attributes */
      LOG_DEBUG(p_ctx, "RTSP: Secondary payload type attribute - not supported");
      return VC_CONTAINER_SUCCESS;
   }

   while (*ptr)
   {
      char *name;
      char *value;

      /* Only add the parameter if the name was not empty. This avoids problems with
       * strings like ";;", ";" or ";=value;" */
      if (rtsp_parse_extract_parameter(&ptr, &name, &value))
      {
         if (!vc_uri_add_query(t_module->reader_uri, name, value))
         {
            if (value)
               LOG_ERROR(p_ctx, "RTSP: Failed to add <%s>=<%s> query to URI", name, value);
            else
               LOG_ERROR(p_ctx, "RTSP: Failed to add <%s> query to URI", name);
            return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
         }
      }
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Merge base URI and relative URI strings into a merged URI string.
 * Always creates a new string, even if the relative URI is actually absolute.
 *
 * @param p_ctx            The RTSP reader context.
 * @param base_uri_str     The base URI string.
 * @param relative_uri_str The relative URI string.
 * @param p_merged_uri_str Pointer to where to put the merged string pointer.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_merge_uris( VC_CONTAINER_T *p_ctx,
      const char *base_uri_str,
      const char *relative_uri_str,
      char **p_merged_uri_str)
{
   VC_URI_PARTS_T *base_uri = NULL;
   VC_URI_PARTS_T *relative_uri = NULL;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   uint32_t merged_size;

   *p_merged_uri_str = NULL;
   relative_uri = vc_uri_create();
   if (!relative_uri) goto tidy_up;
   if (!vc_uri_parse(relative_uri, relative_uri_str))
   {
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto tidy_up;
   }

   if (vc_uri_scheme(relative_uri) != NULL)
   {
      /* URI is absolute, not relative, so return it as the merged URI */
      size_t len = strlen(relative_uri_str);

      *p_merged_uri_str = (char *)malloc(len + 1);
      if (!*p_merged_uri_str) goto tidy_up;

      strncpy(*p_merged_uri_str, relative_uri_str, len);
      status = VC_CONTAINER_SUCCESS;
      goto tidy_up;
   }

   base_uri = vc_uri_create();
   if (!base_uri) goto tidy_up;
   if (!vc_uri_parse(base_uri, base_uri_str))
   {
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto tidy_up;
   }

   /* Build up merged URI in relative_uri, using base_uri as necessary */
   if (!vc_uri_merge(base_uri, relative_uri)) goto tidy_up;

   merged_size = vc_uri_build(relative_uri, NULL, 0) + 1;
   *p_merged_uri_str = (char *)malloc(merged_size);
   if (!*p_merged_uri_str) goto tidy_up;

   vc_uri_build(relative_uri, *p_merged_uri_str, merged_size);

   status = VC_CONTAINER_SUCCESS;

tidy_up:
   if (base_uri) vc_uri_release(base_uri);
   if (relative_uri) vc_uri_release(relative_uri);
   if (status != VC_CONTAINER_SUCCESS)
      LOG_ERROR(p_ctx, "RTSP: Error merging URIs: %d", (int)status);
   return status;
}

/**************************************************************************//**
 * Parse a control attribute and store it as an absolute URI.
 *
 * @param p_ctx               The RTSP reader context.
 * @param attribute           The control attribute value.
 * @param base_uri_str        The base URI string.
 * @param p_control_uri_str   Pointer to where to put the control string pointer.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_parse_control_attribute( VC_CONTAINER_T *p_ctx,
      const char *attribute,
      const char *base_uri_str,
      char **p_control_uri_str)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   /* control attribute format:
    *    <control URI>
    * The control URI is either absolute or relative to the base URI. If the
    * control URI is just an asterisk, the control URI matches the base URI. */

   if (!*attribute || strcmp(attribute, "*") == 0)
   {
      size_t len = strlen(base_uri_str);

      *p_control_uri_str = (char *)malloc(len + 1);
      if (!*p_control_uri_str)
      {
         LOG_ERROR(p_ctx, "RTSP: Failed to allocate control URI");
         return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      }
      strncpy(*p_control_uri_str, base_uri_str, len);
   } else {
      status = rtsp_merge_uris(p_ctx, base_uri_str, attribute, p_control_uri_str);
   }

   return status;
}

/**************************************************************************//**
 * Open a reader for the track using the URI that has been generated.
 *
 * @param p_ctx      The RTSP reader context.
 * @param t_module   The track module for which a reader is needed.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_open_track_reader( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t uri_buffer_size;
   char *uri_buffer;

   uri_buffer_size = vc_uri_build(t_module->reader_uri, NULL, 0) + 1;
   uri_buffer = (char *)malloc(uri_buffer_size);
   if (!uri_buffer)
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to build RTP URI");
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   }
   vc_uri_build(t_module->reader_uri, uri_buffer, uri_buffer_size);

   t_module->reader = vc_container_open_reader(uri_buffer, &status, NULL, NULL);
   free(uri_buffer);

   return status;
}

/**************************************************************************//**
 * Open a reader for the track using the network URI that has been generated.
 *
 * @param p_ctx      The RTSP reader context.
 * @param t_module   The track module for which a reader is needed.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_open_network_reader( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   char port[PORT_BUFFER_SIZE] = {0};

   if (!t_module->rtp_port)
   {
      VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

      t_module->rtp_port = module->next_rtp_port;
      if (t_module->rtp_port > LAST_DYNAMIC_PORT)
      {
         LOG_ERROR(p_ctx, "RTSP: Out of dynamic ports");
         return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;
      }

      module->next_rtp_port += 2;
   }

   snprintf(port, sizeof(port)-1, "%hu", t_module->rtp_port);
   if (!vc_uri_set_port(t_module->reader_uri, port))
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to set track reader URI port");
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   }

   return rtsp_open_track_reader(p_ctx, t_module);
}

/**************************************************************************//**
 * Open a reader for the track using the file URI that has been generated.
 *
 * @param p_ctx      The RTSP reader context.
 * @param t_module   The track module for which a reader is needed.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_open_file_reader( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_STATUS_T status;
   VC_URI_PARTS_T *rtsp_uri = NULL;
   const char *rtsp_path;
   int len;
   char *new_path = NULL;
   char *extension;

   /* Use the RTSP URI's path, with the extension changed to ".t<track>.pkt"
    * where <track> is the zero-based media item number. */

   rtsp_uri = vc_uri_create();
   if (!rtsp_uri)
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to create RTSP URI");
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto tidy_up;
   }

   if (!vc_uri_parse(rtsp_uri, p_ctx->priv->io->uri))
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to parse RTSP URI <%s>", p_ctx->priv->io->uri);
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto tidy_up;
   }

   rtsp_path = vc_uri_path(rtsp_uri);
   if (!rtsp_path || !*rtsp_path)
   {
      LOG_ERROR(p_ctx, "RTSP: RTSP URI path missing <%s>", p_ctx->priv->io->uri);
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      goto tidy_up;
   }

   len = strlen(rtsp_path);
   new_path = (char *)calloc(1, len + RTP_PATH_EXTRA + 1);
   if (!rtsp_uri)
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to create buffer for RTP path");
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto tidy_up;
   }

   strncpy(new_path, rtsp_path, len);
   extension = strrchr(new_path, '.');          /* Find extension, to replace it */
   if (!extension)
      extension = new_path + strlen(new_path);  /* No extension, so append instead */

   snprintf(extension, len + RTP_PATH_EXTRA - (extension - new_path),
            RTP_PATH_EXTENSION_FORMAT, p_ctx->priv->module->media_item);
   if (!vc_uri_set_path(t_module->reader_uri, new_path))
   {
      LOG_ERROR(p_ctx, "RTSP: Failed to store RTP path <%s>", new_path);
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto tidy_up;
   }

   free(new_path);
   vc_uri_release(rtsp_uri);

   return rtsp_open_track_reader(p_ctx, t_module);

tidy_up:
   if (new_path) free(new_path);
   if (rtsp_uri) vc_uri_release(rtsp_uri);
   return status;
}

/**************************************************************************//**
 * Copy track information from the encapsulated track reader's track to the
 * RTSP track.
 *
 * @param p_ctx   The RTSP reader context.
 * @param track   The RTSP track requiring its information to be filled in.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_copy_track_data_from_reader( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track )
{
   VC_CONTAINER_T *reader = track->priv->module->reader;
   VC_CONTAINER_ES_FORMAT_T *src_format, *dst_format;

   if (reader->tracks_num != 1)
   {
      LOG_ERROR(p_ctx, "RTSP: Expected track reader to have one track, has %d", reader->tracks_num);
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   if (reader->tracks[0]->meta_num)
   {
      LOG_DEBUG(p_ctx, "RTSP: Track reader has meta data - not supported");
   }

   src_format = reader->tracks[0]->format;
   dst_format = track->format;

   /* Copy fields individually to avoid problems with pointers (type and extradata). */
   dst_format->es_type        = src_format->es_type;
   dst_format->codec          = src_format->codec;
   dst_format->codec_variant  = src_format->codec_variant;
   *dst_format->type          = *src_format->type;
   dst_format->bitrate        = src_format->bitrate;
   memcpy(dst_format->language, src_format->language, sizeof(dst_format->language));
   dst_format->group_id       = src_format->group_id;
   dst_format->flags          = src_format->flags;

   if (src_format->extradata)
   {
      VC_CONTAINER_STATUS_T status;
      uint32_t extradata_size = src_format->extradata_size;

      status = vc_container_track_allocate_extradata(p_ctx, track, extradata_size);
      if (status != VC_CONTAINER_SUCCESS)
         return status;

      memcpy(dst_format->extradata, src_format->extradata, extradata_size);
      dst_format->extradata_size = extradata_size;
   }

   track->is_enabled = reader->tracks[0]->is_enabled;

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Finalise the creation of the RTSP track by opening its reader and filling in
 * the track information.
 *
 * @param p_ctx   The RTSP reader context.
 * @param track   The RTSP track being finalised.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_complete_track( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if (!t_module->control_uri)
   {
      LOG_ERROR(p_ctx, "RTSP: Track control URI is missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   if (module->uri_has_network_info)
   {
      int ii;

      if (!vc_uri_set_host(t_module->reader_uri, ""))
      {
         LOG_ERROR(p_ctx, "RTSP: Failed to set track reader URI host");
         return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      }

      status = rtsp_open_network_reader(p_ctx, t_module);

      for (ii = 0; status == VC_CONTAINER_ERROR_URI_OPEN_FAILED && ii < DYNAMIC_PORT_ATTEMPTS_MAX; ii++)
      {
         /* Reset port to pick up next dynamic port */
         t_module->rtp_port = 0;
         status = rtsp_open_network_reader(p_ctx, t_module);
      }

      /* Change I/O to non-blocking, so that tracks can be polled */
      if (status == VC_CONTAINER_SUCCESS)
         status = vc_container_control(t_module->reader, VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS, 0);
      /* Set a large read buffer, to avoid dropping bursts of large packets (e.g. hi-def video) */
      if (status == VC_CONTAINER_SUCCESS)
         status = vc_container_control(t_module->reader, VC_CONTAINER_CONTROL_IO_SET_READ_BUFFER_SIZE, UDP_READ_BUFFER_SIZE);
   } else {
      status = rtsp_open_file_reader(p_ctx, t_module);
   }

   vc_uri_release(t_module->reader_uri);
   t_module->reader_uri = NULL;

   if (status == VC_CONTAINER_SUCCESS)
      status = rtsp_copy_track_data_from_reader(p_ctx, track);

   return status;
}

/**************************************************************************//**
 * Returns whether the given character is an attribute name delimiter or not.
 *
 * @param char_to_test  The character under test.
 * @return  True if the character is an attribute name delimiter, false if not.
 */
static int attribute_name_delimiter_fn(int char_to_test)
{
   return (char_to_test == ':');
}

/**************************************************************************//**
 * Create RTSP tracks from media fields in SDP formatted data.
 *
 * @param p_ctx      The RTSP reader context.
 * @param sdp_buffer The SDP data.
 * @param base_uri   The RTSP base URI.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_create_tracks_from_sdp( VC_CONTAINER_T *p_ctx,
      char *sdp_buffer,
      char *base_uri )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_T *track = NULL;
   char *session_base_uri = base_uri;
   char *ptr;
   char *next_line_ptr;
   char *attribute;

   for (ptr = sdp_buffer; status == VC_CONTAINER_SUCCESS && *ptr; ptr = next_line_ptr)
   {
      /* Find end of line */
      char field = *ptr;

      next_line_ptr = ptr;
      while (*next_line_ptr && *next_line_ptr != '\n')
         next_line_ptr++;

      /* Terminate line */
      if (*next_line_ptr)
         *next_line_ptr++ = '\0';

      /* The format of the line has to be "<field>=<value>" where <field> is a single
       * character. Ignore anything else. */
      if (ptr[1] != '=')
         continue;
      ptr = rtsp_trim(ptr + 2);

      switch (field)
      {
      case 'm':
         /* Start of media item */
         if (track)
         {
            /* Finish previous track */
            status = rtsp_complete_track(p_ctx, track);
            track = NULL;
            p_ctx->priv->module->media_item++;
            if (status != VC_CONTAINER_SUCCESS)
               break;
         }

         status = rtsp_create_track_for_media_field(p_ctx, ptr, &track);
         break;
      case 'a':   /* Attribute (either session or media level) */
         /* Attributes of the form "a=<name>:<value>" */
         attribute = rtsp_parse_extract(&ptr, attribute_name_delimiter_fn, NULL);

         if (track)
         {
            /* Media level attribute */

            /* Look for known attributes */
            if (strcmp(attribute, "rtpmap") == 0)
               status = rtsp_parse_rtpmap_attribute(p_ctx, track, ptr);
            else if (strcmp(attribute, "fmtp") == 0)
               status = rtsp_parse_fmtp_attribute(p_ctx, track, ptr);
            else if (strcmp(attribute, "control") == 0)
            {
               char **track_control_uri = &track->priv->module->control_uri;

               if (*track_control_uri)
               {
                  free(*track_control_uri);
                  *track_control_uri = NULL;
               }
               status = rtsp_parse_control_attribute(p_ctx, ptr, session_base_uri, track_control_uri);
            }
            /* Any other attributes are ignored */
         } else {
            /* Session level attribute */
            if (strcmp(attribute, "control") == 0)
            {
               /* Only need to change the session_base_uri if it differs from the base URI */
               ptr = rtsp_trim(ptr);
               if (session_base_uri != base_uri)
               {
                  free(session_base_uri);
                  session_base_uri = base_uri;
               }
               if (strcmp(ptr, base_uri) != 0)
                  status = rtsp_parse_control_attribute(p_ctx, ptr, base_uri, &session_base_uri);
            }
         }
         break;
      default:    /* Ignore any other field names */
         ;
      }
   }

   if (session_base_uri && session_base_uri != base_uri)
      free(session_base_uri);

   /* Having no media fields is an error, since there will be nothing to play back */
   if (status == VC_CONTAINER_SUCCESS)
   {
      if (!p_ctx->tracks_num)
         status = VC_CONTAINER_ERROR_FORMAT_INVALID;
      else if (track)
      {
         /* Finish final track */
         status = rtsp_complete_track(p_ctx, track);
         p_ctx->priv->module->media_item++;
      }
   }

   return status;
}

/**************************************************************************//**
 * Create RTSP tracks from the response to a DESCRIBE request.
 * The response must have already been filled into the comms buffer and header
 * list.
 *
 * @param p_ctx   The RTSP reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_create_tracks_from_response( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINERS_LIST_T *header_list = p_ctx->priv->module->header_list;
   RTSP_HEADER_T header;
   char *base_uri;
   char *content;

   header.name = CONTENT_PSEUDOHEADER_NAME;
   if (!vc_containers_list_find_entry(header_list, &header))
   {
      LOG_ERROR(p_ctx, "RTSP: Content missing");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }
   content = header.value;

   /* The control URI may be relative to a base URI which is the first of these
    * that is available:
    *    1. Content-Base header
    *    2. Content-Location header
    *    3. Request URI
    */
   header.name = CONTENT_BASE_NAME;
   if (vc_containers_list_find_entry(header_list, &header))
      base_uri = header.value;
   else {
      header.name = CONTENT_LOCATION_NAME;
      if (vc_containers_list_find_entry(header_list, &header))
         base_uri = header.value;
      else
         base_uri = p_ctx->priv->io->uri;
   }

   return rtsp_create_tracks_from_sdp(p_ctx, content, base_uri);
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
static int rtsp_header_comparator(const RTSP_HEADER_T *first, const RTSP_HEADER_T *second)
{
   return strcasecmp(first->name, second->name);
}

/**************************************************************************//**
 * Make a DESCRIBE request to the server and create tracks from the response.
 *
 * @param p_ctx   The RTSP reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_describe( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   /* Send DESCRIBE request and get response */
   status = rtsp_send_describe_request(p_ctx);
   if (status != VC_CONTAINER_SUCCESS) return status;
   status = rtsp_read_response(p_ctx);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Create tracks from SDP content */
   status = rtsp_create_tracks_from_response(p_ctx);

   return status;
}

/**************************************************************************//**
 * Make a SETUP request to the server and get session from response.
 *
 * @param p_ctx      The RTSP reader context.
 * @param t_module   The track module to be set up.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_setup( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   const char *session_header;
   size_t session_header_len;

   status = rtsp_send_setup_request(p_ctx, t_module);
   if (status != VC_CONTAINER_SUCCESS) return status;
   status = rtsp_read_response(p_ctx);
   if (status != VC_CONTAINER_SUCCESS) return status;

   session_header = rtsp_get_session_header(module->header_list);
   session_header_len = strlen(session_header);
   if (session_header_len > SESSION_HEADER_LENGTH_MAX) return VC_CONTAINER_ERROR_FORMAT_INVALID;

   t_module->session_header = (char *)malloc(session_header_len + 1);
   if (!t_module->session_header) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   strncpy(t_module->session_header, session_header, session_header_len);

   return status;
}

/**************************************************************************//**
 * Make a SETUP request to the server and get session from response.
 *
 * @param p_ctx      The RTSP reader context.
 * @param t_module   The track module to be set up.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_play( VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   status = rtsp_send_play_request(p_ctx, t_module);
   if (status != VC_CONTAINER_SUCCESS) return status;
   status = rtsp_read_response(p_ctx);
   if (status != VC_CONTAINER_SUCCESS) return status;

   rtsp_store_rtp_info(module->header_list, t_module);

   return status;
}

/**************************************************************************//**
 * Blocking read/skip data from a container.
 * Can also be used to query information about the next block of data.
 *
 * @pre The container is set to non-blocking.
 * @post The container is set to non-blocking.
 *
 * @param p_ctx      The reader context.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_blocking_track_read(VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *p_packet,
                                               uint32_t flags)
{
   VC_CONTAINER_STATUS_T status;

   status = vc_container_read(p_ctx, p_packet, flags);

   /* The ..._ABORTED status corresponds to a timeout waiting for data */
   if (status == VC_CONTAINER_ERROR_ABORTED)
   {
      /* So switch to blocking temporarily to wait for some */
      (void)vc_container_control(p_ctx, VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS, VC_CONTAINER_READ_TIMEOUT_BLOCK);
      status = vc_container_read(p_ctx, p_packet, flags);
      (void)vc_container_control(p_ctx, VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS, 0);
   }

   return status;
}

/**************************************************************************//**
 * Update the cached packet info blocks for all tracks.
 * If one or more of the tracks has data, set the current track to the one with
 * the earliest decode timestamp.
 *
 * @pre The track readers must not block when data is requested from them.
 *
 * @param p_ctx   The RTSP reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_update_track_info( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t tracks_num = p_ctx->tracks_num;
   uint32_t track_idx;
   int64_t earliest_dts = MAXIMUM_INT64;
   VC_CONTAINER_TRACK_MODULE_T *earliest_track = NULL;

   /* Reset current track to unknown */
   p_ctx->priv->module->current_track = NULL;

   /* Collect each track's info and return the one with earliest timestamp. */
   for (track_idx = 0; track_idx < tracks_num; track_idx++)
   {
      VC_CONTAINER_TRACK_MODULE_T *t_module = p_ctx->tracks[track_idx]->priv->module;
      VC_CONTAINER_PACKET_T *info = &t_module->info;

      /* If this track has no data available, request more */
      if (!info->size)
      {
         /* This is a non-blocking read, so status will be ..._ABORTED if nothing available */
         status = vc_container_read(t_module->reader, info, VC_CONTAINER_READ_FLAG_INFO);
         /* Adjust track index to be the RTSP index instead of the RTP one */
         info->track = track_idx;
      }

      if (status == VC_CONTAINER_SUCCESS)
      {
         if (info->dts < earliest_dts)
         {
            earliest_dts = info->dts;
            earliest_track = t_module;
         }
      }
      else if (status != VC_CONTAINER_ERROR_ABORTED)
      {
         /* Not a time-out failure, so abort */
         return status;
      }
   }

   p_ctx->priv->module->current_track = earliest_track;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

/**************************************************************************//**
 * Read/skip data from the container.
 * Can also be used to query information about the next block of data.
 *
 * @param p_ctx      The reader context.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_reader_read( VC_CONTAINER_T *p_ctx,
                                               VC_CONTAINER_PACKET_T *p_packet,
                                               uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_TRACK_MODULE_T *current_track = module->current_track;
   VC_CONTAINER_PACKET_T *info;

   if (flags & VC_CONTAINER_READ_FLAG_FORCE_TRACK)
   {
      vc_container_assert(p_packet);
      vc_container_assert(p_packet->track < p_ctx->tracks_num);
      current_track = p_ctx->tracks[p_packet->track]->priv->module;
      module->current_track = current_track;

      if (!current_track->info.size)
      {
         status = rtsp_blocking_track_read(current_track->reader, &current_track->info, VC_CONTAINER_READ_FLAG_INFO);
         if (status != VC_CONTAINER_SUCCESS)
            goto error;
      }
   }
   else if (!current_track || !current_track->info.size)
   {
      status = rtsp_update_track_info(p_ctx);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;

      while (!module->current_track)
      {
         /* Check RTSP stream to see if it has closed */
         status = rtsp_read_response(p_ctx);
         if (status == VC_CONTAINER_SUCCESS || status == VC_CONTAINER_ERROR_ABORTED)
         {
            /* No data from any track yet, so keep checking */
            status = rtsp_update_track_info(p_ctx);
         }
         if (status != VC_CONTAINER_SUCCESS)
            goto error;
      }

      current_track = module->current_track;
   }

   info = &current_track->info;
   vc_container_assert(info->size);

   if (flags & VC_CONTAINER_READ_FLAG_INFO)
   {
      vc_container_assert(p_packet);
      memcpy(p_packet, info, sizeof(*info));
   } else {
      status = rtsp_blocking_track_read(current_track->reader, p_packet, flags);
      if (status != VC_CONTAINER_SUCCESS)
         goto error;

      if (p_packet)
      {
         p_packet->track = info->track;

         if (flags & VC_CONTAINER_READ_FLAG_SKIP)
         {
            info->size = 0;
         } else {
            vc_container_assert(info->size >= p_packet->size);
            info->size -= p_packet->size;
         }
      } else {
         info->size = 0;
      }
   }

   if (p_packet)
   {
      /* Adjust timestamps to be relative to zero */
      if (!module->ts_base)
         module->ts_base = p_packet->dts;
      p_packet->dts -= module->ts_base;
      p_packet->pts -= module->ts_base;
   }

error:
   STREAM_STATUS(p_ctx) = status;
   return status;
}

/**************************************************************************//**
 * Seek over data in the container.
 *
 * @param p_ctx      The reader context.
 * @param p_offset   The seek offset.
 * @param mode       The seek mode.
 * @param flags      The seek flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_reader_seek( VC_CONTAINER_T *p_ctx,
                                               int64_t *p_offset,
                                               VC_CONTAINER_SEEK_MODE_T mode,
                                               VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(p_offset);
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);

   /* RTSP is a non-seekable container */
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/**************************************************************************//**
 * Close the container.
 *
 * @param p_ctx   The reader context.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T rtsp_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   unsigned int i;

   for(i = 0; i < p_ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_MODULE_T *t_module = p_ctx->tracks[i]->priv->module;

      if (t_module->control_uri && t_module->session_header)
      {
         /* Send the teardown message and wait for a response, although it
          * isn't important whether it was successful or not. */
         if (rtsp_send_teardown_request(p_ctx, t_module) == VC_CONTAINER_SUCCESS)
            (void)rtsp_read_response(p_ctx);
      }

      if (t_module->reader)
         vc_container_close(t_module->reader);
      if (t_module->reader_uri)
         vc_uri_release(t_module->reader_uri);
      if (t_module->control_uri)
         free(t_module->control_uri);
      if (t_module->session_header)
         free(t_module->session_header);
      vc_container_free_track(p_ctx, p_ctx->tracks[i]);  /* Also need to close track's reader */
   }
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   if (module)
   {
      if (module->comms_buffer)
         free(module->comms_buffer);
      if (module->header_list)
         vc_containers_list_destroy(module->header_list);
      free(module);
   }
   p_ctx->priv->module = 0;
   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Open the container.
 * Uses the I/O URI and/or data to configure the container.
 *
 * @param p_ctx   The reader context.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T rtsp_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint32_t ii;

   /* Check the URI scheme looks valid */
   if (!vc_uri_scheme(p_ctx->priv->uri) ||
       (strcasecmp(vc_uri_scheme(p_ctx->priv->uri), RTSP_SCHEME) &&
        strcasecmp(vc_uri_scheme(p_ctx->priv->uri), RTSP_PKT_SCHEME)))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   if ((module = (VC_CONTAINER_MODULE_T *)malloc(sizeof(VC_CONTAINER_MODULE_T))) == NULL)
   {
      status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      goto error;
   }

   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks = module->tracks;
   module->next_rtp_port = FIRST_DYNAMIC_PORT;
   module->cseq_value = 0;
   module->uri_has_network_info =
         (strncasecmp(p_ctx->priv->io->uri, RTSP_NETWORK_URI_START, RTSP_NETWORK_URI_START_LENGTH) == 0);
   module->comms_buffer = (char *)calloc(1, COMMS_BUFFER_SIZE+1);
   if (!module->comms_buffer) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   /* header_list will contain pointers into the response_buffer, so take care in re-use */
   module->header_list = vc_containers_list_create(HEADER_LIST_INITIAL_CAPACITY, sizeof(RTSP_HEADER_T),
         (VC_CONTAINERS_LIST_COMPARATOR_T)rtsp_header_comparator);
   if (!module->header_list) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }

   status = rtsp_describe(p_ctx);
   for (ii = 0; status == VC_CONTAINER_SUCCESS && ii < p_ctx->tracks_num; ii++)
      status = rtsp_setup(p_ctx, p_ctx->tracks[ii]->priv->module);
   for (ii = 0; status == VC_CONTAINER_SUCCESS && ii < p_ctx->tracks_num; ii++)
      status = rtsp_play(p_ctx, p_ctx->tracks[ii]->priv->module);
   if (status != VC_CONTAINER_SUCCESS)
      goto error;

   /* Set the RTSP stream to block briefly, to allow polling for closure as well as to avoid spinning CPU */
   vc_container_control(p_ctx, VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS, DATA_UNAVAILABLE_READ_TIMEOUT_MS);

   p_ctx->priv->pf_close = rtsp_reader_close;
   p_ctx->priv->pf_read = rtsp_reader_read;
   p_ctx->priv->pf_seek = rtsp_reader_seek;

   if(STREAM_STATUS(p_ctx) != VC_CONTAINER_SUCCESS) goto error;
   return VC_CONTAINER_SUCCESS;

error:
   if(status == VC_CONTAINER_SUCCESS || status == VC_CONTAINER_ERROR_EOS)
      status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   LOG_DEBUG(p_ctx, "error opening RTSP stream (%i)", status);
   rtsp_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open rtsp_reader_open
#endif
