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
#ifndef VC_CONTAINERS_PRIVATE_H
#define VC_CONTAINERS_PRIVATE_H

/** \file containers_private.h
 * Private interface for container readers and writers
 */

#include <stdarg.h>
#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_io.h"
#include "containers/core/containers_filters.h"
#include "containers/packetizers.h"
#include "containers/core/containers_uri.h"

#define URI_MAX_LEN 256

/** \defgroup VcContainerModuleApi Container Module API
 * Private interface for modules implementing container readers and writers */
/* @{ */

/** Track context private to the container reader / writer instance. This private context is used to
 * store data which shouldn't be exported by the public API. */
typedef struct VC_CONTAINER_TRACK_PRIVATE_T
{
   /** Pointer to the private data of the container module in use */
   struct VC_CONTAINER_TRACK_MODULE_T *module;

   /** Pointer to the allocated buffer for the track extradata */
   uint8_t *extradata;
   /** Size of the allocated buffer for the track extradata */
   uint32_t extradata_size;

   /** Pointer to the allocated buffer for the track DRM data*/
   uint8_t *drmdata;
   /** Size of the allocated buffer for the track DRM data */
   uint32_t drmdata_size;

   /** Packetizer used by this track */
   VC_PACKETIZER_T *packetizer;

} VC_CONTAINER_TRACK_PRIVATE_T;

/** Context private to the container reader / writer instance. This private context is used to
 * store data which shouldn't be exported by the public API. */
typedef struct VC_CONTAINER_PRIVATE_T
{
   /** Pointer to the container i/o instance used to read / write to the container */
   struct VC_CONTAINER_IO_T *io;
   /** Pointer to the private data of the container module in use */
   struct VC_CONTAINER_MODULE_T *module;

   /** Reads a data packet from a container reader.
    * By default, the reader will read whatever packet comes next in the container and update the
    * given \ref VC_CONTAINER_PACKET_T structure with this packet's information.
    * This behaviour can be changed using the \ref VC_CONTAINER_READ_FLAGS_T.\n
    * \ref VC_CONTAINER_READ_FLAG_INFO will instruct the reader to only return information on the
    * following packet but not its actual data. The data can be retreived later by issuing another
    * read request.
    * \ref VC_CONTAINER_READ_FLAG_FORCE_TRACK will force the reader to read the next packet for the
    * selected track (as present in the \ref VC_CONTAINER_PACKET_T structure) instead of defaulting
    * to reading the packet which comes next in the container.
    * \ref VC_CONTAINER_READ_FLAG_SKIP will instruct the reader to skip the next packet. In this case
    * it isn't necessary for the caller to pass a pointer to a \ref VC_CONTAINER_PACKET_T structure
    * unless the \ref VC_CONTAINER_READ_FLAG_INFO is also given.
    * A combination of all these flags can be used.
    *
    * \param  context   Pointer to the context of the reader to use
    * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
    *                   This needs to be partially filled before the call (buffer, buffer_size)
    * \param  flags     Flags controlling the read operation
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_read)( VC_CONTAINER_T *context,
      VC_CONTAINER_PACKET_T *packet, VC_CONTAINER_READ_FLAGS_T flags );

   /** Writes a data packet to a container writer.
    *
    * \param  context   Pointer to the context of the writer to use
    * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_write)( struct VC_CONTAINER_T *context,
      VC_CONTAINER_PACKET_T *packet );

   /** Seek into a container reader.
    *
    * \param  context   Pointer to the context of the reader to use
    * \param  offset    Offset to seek to. Used as an input as well as output value.
    * \param  mode      Seeking mode requested.
    * \param  flags     Flags affecting the seeking operation.
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_seek)( VC_CONTAINER_T *context, int64_t *offset,
         VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags);

   /** Extensible control function for container readers and writers.
    * This function takes a variable number of arguments which will depend on the specific operation.
    *
    * \param  context   Pointer to the VC_CONTAINER_T context to use
    * \param  operation The requested operation
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_control)( VC_CONTAINER_T *context, VC_CONTAINER_CONTROL_T operation, va_list args );

   /** Closes a container reader / writer module.
    *
    * \param  context   Pointer to the context of the instance to close
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_close)( struct VC_CONTAINER_T *context );

   /** Pointer to container filter instance used for DRM */
   struct VC_CONTAINER_FILTER_T *drm_filter;

   /** Pointer to the container module code and symbols*/
   void *module_handle;

   /** Maximum size of a stream that is being written.
    * This is set by the client using the control mechanism */
   int64_t max_size;

   /** Pointer to the temp i/o instance used to write temporary data */
   struct VC_CONTAINER_IO_T *tmp_io;

   /** Current status of the container (only used for writers to prevent trying to write
    * more data if one of the writes failed) */
   VC_CONTAINER_STATUS_T status;

   /** Logging verbosity */
   uint32_t verbosity;

   /** Uniform Resource Identifier */
   struct VC_URI_PARTS_T *uri;

   /** Flag specifying whether one of the tracks is being packetized */
   bool packetizing;

   /** Temporary packet structure used to feed data to the packetizer */
   VC_CONTAINER_PACKET_T packetizer_packet;

   /** Temporary buffer used by the packetizer */
   uint8_t *packetizer_buffer;

} VC_CONTAINER_PRIVATE_T;

/* Internal functions */
VC_CONTAINER_TRACK_T *vc_container_allocate_track( VC_CONTAINER_T *context, unsigned int extra_size );
void vc_container_free_track( VC_CONTAINER_T *context, VC_CONTAINER_TRACK_T *track );
VC_CONTAINER_STATUS_T vc_container_track_allocate_extradata( VC_CONTAINER_T *context,
   VC_CONTAINER_TRACK_T *p_track, unsigned int extra_size );
VC_CONTAINER_STATUS_T vc_container_track_allocate_drmdata( VC_CONTAINER_T *context,
   VC_CONTAINER_TRACK_T *p_track, unsigned int size );
   
/* @} */

#endif /* VC_CONTAINERS_PRIVATE_H */
