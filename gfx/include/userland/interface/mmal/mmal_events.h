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

#ifndef MMAL_EVENTS_H
#define MMAL_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mmal_common.h"
#include "mmal_parameters.h"
#include "mmal_port.h"

/** \defgroup MmalEvents List of pre-defined event types
 * This defines a list of standard event types. Components can still define proprietary
 * event types by using their own FourCC and defining their own event structures. */
/* @{ */

/** \name Pre-defined event FourCCs */
/* @{ */

/** Error event. Data contains a \ref MMAL_STATUS_T */
#define MMAL_EVENT_ERROR                     MMAL_FOURCC('E','R','R','O')

/** End-of-stream event. Data contains a \ref MMAL_EVENT_END_OF_STREAM_T */
#define MMAL_EVENT_EOS                       MMAL_FOURCC('E','E','O','S')

/** Format changed event. Data contains a \ref MMAL_EVENT_FORMAT_CHANGED_T */
#define MMAL_EVENT_FORMAT_CHANGED            MMAL_FOURCC('E','F','C','H')

/** Parameter changed event. Data contains the new parameter value, see
 * \ref MMAL_EVENT_PARAMETER_CHANGED_T
 */
#define MMAL_EVENT_PARAMETER_CHANGED         MMAL_FOURCC('E','P','C','H')

/* @} */

/** End-of-stream event. */
typedef struct MMAL_EVENT_END_OF_STREAM_T
{
   MMAL_PORT_TYPE_T port_type;   /**< Type of port that received the end of stream */
   uint32_t port_index;          /**< Index of port that received the end of stream */
} MMAL_EVENT_END_OF_STREAM_T;

/** Format changed event data. */
typedef struct MMAL_EVENT_FORMAT_CHANGED_T
{
   uint32_t buffer_size_min;         /**< Minimum size of buffers the port requires */
   uint32_t buffer_num_min;          /**< Minimum number of buffers the port requires */
   uint32_t buffer_size_recommended; /**< Size of buffers the port recommends for optimal performance.
                                          A value of zero means no special recommendation. */
   uint32_t buffer_num_recommended;  /**< Number of buffers the port recommends for optimal
                                          performance. A value of zero means no special recommendation. */

   MMAL_ES_FORMAT_T *format;         /**< New elementary stream format */
} MMAL_EVENT_FORMAT_CHANGED_T;

/** Parameter changed event data.
 * This is a variable sized event. The full parameter is included in the event
 * data, not just the header. Use the \ref MMAL_PARAMETER_HEADER_T::id field to determine how to
 * cast the structure. The \ref MMAL_PARAMETER_HEADER_T::size field can be used to check validity.
 */
typedef struct MMAL_EVENT_PARAMETER_CHANGED_T
{
   MMAL_PARAMETER_HEADER_T hdr;
} MMAL_EVENT_PARAMETER_CHANGED_T;

/** Get a pointer to the \ref MMAL_EVENT_FORMAT_CHANGED_T structure contained in the buffer header.
 * Note that the pointer will point inside the data contained in the buffer header
 * so doesn't need to be freed explicitly.
 *
 * @param buffer buffer header containing the MMAL_EVENT_FORMAT_CHANGED event.
 * @return pointer to a MMAL_EVENT_FORMAT_CHANGED_T structure.
 */
MMAL_EVENT_FORMAT_CHANGED_T *mmal_event_format_changed_get(MMAL_BUFFER_HEADER_T *buffer);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_EVENTS_H */
