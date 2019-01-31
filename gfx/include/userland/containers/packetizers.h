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
#ifndef VC_PACKETIZERS_H
#define VC_PACKETIZERS_H

/** \file packetizers.h
 * Public API for packetizing data (i.e. framing and timestamping)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "containers/containers.h"

/** \defgroup VcPacketizerApi Packetizer API
 *  API for packetizers */
/* @{ */

/** \name Packetizer flags
 * \anchor packetizerflags
 * The following flags describe properties of a packetizer */
/* @{ */
#define VC_PACKETIZER_FLAG_ES_CHANGED 0x1 /**< ES definition has changed */
/* @} */

/** Definition of the packetizer type */
typedef struct VC_PACKETIZER_T
{
   struct VC_PACKETIZER_PRIVATE_T *priv; /**< Private member used by the implementation */
   uint32_t flags;                       /**< Flags describing the properties of a packetizer.
                                           * See \ref packetizerflags "Packetizer flags". */

   VC_CONTAINER_ES_FORMAT_T *in;  /**< Format of the input elementary stream */
   VC_CONTAINER_ES_FORMAT_T *out;  /**< Format of the output elementary stream */

   uint32_t max_frame_size; /**< Maximum size of a packetized frame */

} VC_PACKETIZER_T;

/** Open a packetizer to convert the input format into the requested output format.
 * This will create an an instance of a packetizer and its associated context.
 *
 * If no packetizer is found for the requested format, this will return a null pointer as well as
 * an error code indicating why this failed.
 *
 * \param  in           Input elementary stream format
 * \param  out_variant  Requested output variant for the output elementary stream format
 * \param  status       Returns the status of the operation
 * \return              A pointer to the context of the new instance of the packetizer.
 *                      Returns NULL on failure.
 */
VC_PACKETIZER_T *vc_packetizer_open(VC_CONTAINER_ES_FORMAT_T *in, VC_CONTAINER_FOURCC_T out_variant,
   VC_CONTAINER_STATUS_T *status);

/** Closes an instance of a packetizer.
 * This will free all the resources associated with the context.
 *
 * \param  context   Pointer to the context of the instance to close
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_packetizer_close( VC_PACKETIZER_T *context );

/** \name Packetizer flags
 * The following flags can be passed during a packetize call */
/* @{ */
/** Type definition for the packetizer flags */
typedef uint32_t VC_PACKETIZER_FLAGS_T;
/** Ask the packetizer to only return information on the next packet without reading it */
#define VC_PACKETIZER_FLAG_INFO   0x1
/** Ask the packetizer to skip the next packet */
#define VC_PACKETIZER_FLAG_SKIP   0x2
/** Ask the packetizer to flush any data being processed */
#define VC_PACKETIZER_FLAG_FLUSH   0x4
/** Force the packetizer to release an input packet */
#define VC_PACKETIZER_FLAG_FORCE_RELEASE_INPUT 0x8
/* @} */

/** Push a new packet of data to the packetizer.
 * This is the mechanism used to feed data into the packetizer. Once a packet has been
 * pushed into the packetizer it is owned by the packetizer until released by a call to
 * \ref vc_packetizer_pop
 *
 * \param  context   Pointer to the context of the packetizer to use
 * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
 *                   to push into the packetizer.
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_packetizer_push( VC_PACKETIZER_T *context,
   VC_CONTAINER_PACKET_T *packet);

/** Pop a packet of data from the packetizer.
 * This allows the client to retrieve consumed data from the packetizer. Packets returned by
 * the packetizer in this manner can then be released / recycled by the client.
 * It is possible for the client to retrieve non-consumed data by passing the
 * VC_PACKETIZER_FLAG_FORCE_RELEASE_INPUT flag. This will however trigger some internal buffering
 * inside the packetizer and thus is less efficient.
 *
 * \param  context   Pointer to the context of the packetizer to use
 * \param  packet    Pointer used to return a consumed packet
 * \param  flags     Miscellaneous flags controlling the operation
 *
 * \return           VC_CONTAINER_SUCCESS if a consumed packet was retrieved,
 *                   VC_CONTAINER_ERROR_INCOMPLETE_DATA if none is available.
 */
VC_CONTAINER_STATUS_T vc_packetizer_pop( VC_PACKETIZER_T *context,
   VC_CONTAINER_PACKET_T **packet, VC_PACKETIZER_FLAGS_T flags);

/** Read packetized data out of the packetizer.
 *
 * \param  context   Pointer to the context of the packetizer to use
 * \param  packet    Pointer to the VC_CONTAINER_PACKET_T structure describing the data packet
 *                   This might need to be partially filled before the call (buffer, buffer_size)
 *                   depending on the flags used.
 * \param  flags     Miscellaneous flags controlling the operation
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_packetizer_read( VC_PACKETIZER_T *context,
   VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags);

/** Reset packetizer state.
 * This will reset the state of the packetizer as well as mark all data pushed to it as consumed.
 *
 * \param  context   Pointer to the context of the packetizer to reset
 * \return           the status of the operation
 */
VC_CONTAINER_STATUS_T vc_packetizer_reset( VC_PACKETIZER_T *context );

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* VC_PACKETIZERS_H */
