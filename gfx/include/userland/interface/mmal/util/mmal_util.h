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

#ifndef MMAL_UTIL_H
#define MMAL_UTIL_H

#include "interface/mmal/mmal.h"

/** \defgroup MmalUtilities Utility functions
 * The utility functions provide helpers for common functionality that is not part
 * of the core MMAL API.
 * @{
 */

/** Offset in bytes of FIELD in TYPE. */
#define MMAL_OFFSET(TYPE, FIELD) ((size_t)((uint8_t *)&((TYPE*)0)->FIELD - (uint8_t *)0))

#ifdef __cplusplus
extern "C" {
#endif

/** Convert a status to a statically-allocated string.
 *
 * @param status The MMAL status code.
 * @return A C string describing the status code.
 */
const char *mmal_status_to_string(MMAL_STATUS_T status);

/** Convert stride to pixel width for a given pixel encoding.
 *
 * @param encoding The pixel encoding (such as one of the \ref MmalEncodings "pre-defined encodings")
 * @param stride The stride in bytes.
 * @return The width in pixels.
 */
uint32_t mmal_encoding_stride_to_width(uint32_t encoding, uint32_t stride);

/** Convert pixel width to stride for a given pixel encoding
 *
 * @param encoding The pixel encoding (such as one of the \ref MmalEncodings "pre-defined encodings")
 * @param width The width in pixels.
 * @return The stride in bytes.
 */
uint32_t mmal_encoding_width_to_stride(uint32_t encoding, uint32_t width);

/** Return the 16 line high sliced version of a given pixel encoding
 *
 * @param encoding The pixel encoding (such as one of the \ref MmalEncodings "pre-defined encodings")
 * @return The sliced equivalent, or MMAL_ENCODING_UNKNOWN if not supported.
 */
uint32_t mmal_encoding_get_slice_variant(uint32_t encoding);

/** Convert a port type to a string.
 *
 * @param type The MMAL port type.
 * @return A NULL-terminated string describing the port type.
 */
const char* mmal_port_type_to_string(MMAL_PORT_TYPE_T type);

/** Get a parameter from a port allocating the required amount of memory
 * for the parameter (i.e. for variable length parameters like URI or arrays).
 * The size field will be set on output to the actual size of the
 * parameter allocated and retrieved.
 *
 * The pointer returned must be released by a call to \ref mmal_port_parameter_free().
 *
 * @param port port to send request to
 * @param id parameter id
 * @param size initial size hint for allocation (can be 0)
 * @param status status of the parameter get operation (can be 0)
 * @return pointer to the header of the parameter or NULL on failure.
 */
MMAL_PARAMETER_HEADER_T *mmal_port_parameter_alloc_get(MMAL_PORT_T *port,
   uint32_t id, uint32_t size, MMAL_STATUS_T *status);

/** Free a parameter structure previously allocated via
 * \ref mmal_port_parameter_alloc_get().
 *
 * @param param pointer to header of the parameter
 */
void mmal_port_parameter_free(MMAL_PARAMETER_HEADER_T *param);

/** Copy buffer header metadata from source to destination.
 *
 * @param dest The destination buffer header.
 * @param src  The source buffer header.
 */
void mmal_buffer_header_copy_header(MMAL_BUFFER_HEADER_T *dest, const MMAL_BUFFER_HEADER_T *src);

/** Create a pool of MMAL_BUFFER_HEADER_T associated with a specific port.
 * This allows a client to allocate memory for the payload buffers based on the preferences
 * of a port. This for instance will allow the port to allocate memory which can be shared
 * between the host processor and videocore.
 * After allocation, all allocated buffer headers will have been added to the queue.
 *
 * It is valid to create a pool with no buffer headers, or with zero size payload buffers.
 * The mmal_pool_resize() function can be used to increase or decrease the number of buffer
 * headers, or the size of the payload buffers, after creation of the pool.
 *
 * @param port         Port responsible for creating the pool.
 * @param headers      Number of buffers which will be allocated with the pool.
 * @param payload_size Size of the payload buffer which will be allocated in
 *                     each of the buffer headers.
 * @return Pointer to the newly created pool or NULL on failure.
 */
MMAL_POOL_T *mmal_port_pool_create(MMAL_PORT_T *port,
   unsigned int headers, uint32_t payload_size);

/** Destroy a pool of MMAL_BUFFER_HEADER_T associated with a specific port.
 * This will also deallocate all of the memory which was allocated when creating or
 * resizing the pool.
 *
 * @param port  Pointer to the port responsible for creating the pool.
 * @param pool  Pointer to the pool to be destroyed.
 */
void mmal_port_pool_destroy(MMAL_PORT_T *port, MMAL_POOL_T *pool);

/** Log the content of a \ref MMAL_PORT_T structure.
 *
 * @param port  Pointer to the port to dump.
 */
void mmal_log_dump_port(MMAL_PORT_T *port);

/** Log the content of a \ref MMAL_ES_FORMAT_T structure.
 *
 * @param format  Pointer to the format to dump.
 */
void mmal_log_dump_format(MMAL_ES_FORMAT_T *format);

/** Return the nth port.
 *
 * @param comp   component to query
 * @param index  port index
 * @param type   port type
 *
 * @return port or NULL if not found
 */
MMAL_PORT_T *mmal_util_get_port(MMAL_COMPONENT_T *comp, MMAL_PORT_TYPE_T type, unsigned index);

/** Convert a 4cc into a string.
 *
 * @param buf    Destination for result
 * @param len    Size of result buffer
 * @param fourcc 4cc to be converted
 * @return converted string (buf)
 *
 */
char *mmal_4cc_to_string(char *buf, size_t len, uint32_t fourcc);

/** On FW prior to June 2016, camera and video_splitter
 *  had BGR24 and RGB24 support reversed.
 *  This is now fixed, and this function will return whether the
 *  FW has the fix or not.
 *
 * @param port   MMAL port to check (on camera or video_splitter)
 * @return 0 if old firmware, 1 if new.
 *
 */
int mmal_util_rgb_order_fixed(MMAL_PORT_T *port);

#ifdef __cplusplus
}
#endif

/** @} */

#endif
