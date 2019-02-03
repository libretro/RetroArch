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

#ifndef _RTP_PRIV_H_
#define _RTP_PRIV_H_

#include "containers/containers.h"

#include "containers/core/containers_private.h"
#include "containers/core/containers_bits.h"
#include "containers/core/containers_list.h"

typedef VC_CONTAINER_STATUS_T (*PAYLOAD_HANDLER_T)(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track, VC_CONTAINER_PACKET_T *p_packet, uint32_t flags);

/** Parameter list entry type. */
typedef struct parameter_tag
{
   const char *name;
   const char *value;
} PARAMETER_T;

/** Prototype for MIME parameter handling.
 * Each MIME type has a certain set of parameter names it uses, so a handler is
 * needed for each type. This is that handler's prototype.
 */
typedef VC_CONTAINER_STATUS_T (*PARAMETER_HANDLER_T)(VC_CONTAINER_T *p_ctx, VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);

/** Track module flag bit numbers (up to seven) */
typedef enum
{
   TRACK_SSRC_SET = 0,
   TRACK_HAS_MARKER,
   TRACK_NEW_PACKET,
} track_module_flag_bit_t;

/** RTP track data */
typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   PAYLOAD_HANDLER_T payload_handler;  /**< Extracts the data from the payload */
   uint8_t *buffer;              /**< Buffer into which the RTP packet is read */
   VC_CONTAINER_BITS_T payload;  /**< Payload bit bit_stream */
   uint8_t flags;                /**< Combination of track module flags */
   uint8_t payload_type;         /**< The expected payload type */
   uint16_t max_seq_num;         /**< Highest seq. number seen */
   uint32_t timestamp;           /**< RTP timestamp of packet */
   uint32_t timestamp_base;      /**< RTP timestamp value that equates to time zero */
   uint32_t last_timestamp_top;  /**< Top two bits of RTP timestamp of previous packet */
   uint32_t timestamp_wraps;     /**< Count of the times that the timestamp has wrapped */
   uint32_t timestamp_clock;     /**< Clock frequency of RTP timestamp values */
   uint32_t expected_ssrc;       /**< The expected SSRC, if set */
   uint32_t base_seq;            /**< Base seq number */
   uint32_t bad_seq;             /**< Last 'bad' seq number + 1 */
   uint32_t probation;           /**< Sequential packets till source is valid */
   uint32_t received;            /**< RTP packets received */
   void *extra;                  /**< Payload specific data */
} VC_CONTAINER_TRACK_MODULE_T;

/** Determine minimum number of bytes needed to hold a number of bits */
#define BITS_TO_BYTES(X)   (((X) + 7) >> 3)

/** Collection of bit manipulation routines */
/* @{ */
#define SET_BIT(V, B)         V |= (1 << (B))
#define CLEAR_BIT(V, B)       V &= ~(1 << (B))
#define BIT_IS_SET(V, B)      (!(!((V) & (1 << (B)))))
#define BIT_IS_CLEAR(V, B)    (!((V) & (1 << (B))))
/* }@ */

/** Get a parameter's value as a decimal number.
 *
 * \param param_list The list of parameter name/value pairs.
 * \param name The paramter's name.
 * \param value Where to put the converted value.
 * \return True if successful, false if the parameter was not found or didn't convert. */
bool rtp_get_parameter_u32(const VC_CONTAINERS_LIST_T *param_list, const char *name, uint32_t *value);

/** Get a parameter's value as a hexadecimal number.
 *
 * \param param_list The list of parameter name/value pairs.
 * \param name The paramter's name.
 * \param value Where to put the converted value.
 * \return True if successful, false if the parameter was not found or didn't convert. */
bool rtp_get_parameter_x32(const VC_CONTAINERS_LIST_T *param_list, const char *name, uint32_t *value);

#endif /* _RTP_PRIV_H_ */
