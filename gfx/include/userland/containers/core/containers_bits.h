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

#ifndef VC_CONTAINERS_BITS_H
#define VC_CONTAINERS_BITS_H

#include "containers/containers.h"

/** Bit stream structure
 * Value are read from the buffer, taking bits from MSB to LSB in sequential
 * bytes until the number of bit and the number of bytes runs out. */
typedef struct vc_container_bits_tag
{
   const uint8_t *buffer;  /**< Buffer from which to take bits */
   uint32_t bytes;         /**< Number of bytes available from buffer */
   uint32_t bits;          /**< Number of bits available at current pointer */
} VC_CONTAINER_BITS_T;

/** Initialise a bit stream object.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object to initialise.
 * \param buffer     Pointer to the start of the byte buffer.
 * \param available  Number of bytes in the bit stream.
 */
void vc_container_bits_init(VC_CONTAINER_BITS_T *bit_stream, const uint8_t *buffer, uint32_t available);

/** Invalidates the bit stream.
 * Also returns zero, because it allows callers that need to invalidate and
 * immediately return zero to do so in a single statement.
 *
 * \pre bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  Zero, always.
 */
uint32_t vc_container_bits_invalidate( VC_CONTAINER_BITS_T *bit_stream );

/** Returns true if the bit stream is currently valid.
 * The stream becomes invalid when a read or skip operation goe beyond the end
 * of the stream.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  True if the stream is valid, false if it is invalid.
 */
bool vc_container_bits_valid(VC_CONTAINER_BITS_T *bit_stream);

/** Reset a valid bit stream object to appear empty.
 * Once a stream has become invalid, reset has no effect.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 */
void vc_container_bits_reset(VC_CONTAINER_BITS_T *bit_stream);

/** Return the current byte pointer for the bit stream.
 *
 * \pre  bit_stream is not NULL.
 * \pre  The stream is on a byte boundary.
 *
 * \param bit_stream The bit stream object.
 * \return  The current byte pointer, or NULL if the stream is invalid.
 */
const uint8_t *vc_container_bits_current_pointer(const VC_CONTAINER_BITS_T *bit_stream);

/** Copy one bit stream to another.
 * If the source stream is invalid, the destination one will become so as well.
 *
 * \pre  Neither bit stream is NULL.
 *
 * \param dst  The destination bit stream object.
 * \param src  The source bit stream object.
 */
void vc_container_bits_copy_stream(VC_CONTAINER_BITS_T *dst, const VC_CONTAINER_BITS_T *src);

/** Return the number of bits left to take from the stream.
 * If the stream is invalid, zero is returned.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  The number of bits left to take.
 */
uint32_t vc_container_bits_available(const VC_CONTAINER_BITS_T *bit_stream);

/** Return the number of bytes left to take from the stream.
 * If the stream is invalid, zero is returned.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  The number of bytes left to take.
 */
uint32_t vc_container_bits_bytes_available(const VC_CONTAINER_BITS_T *bit_stream);

/** Skip past a number of bits in the stream.
 * If bits_to_skip is greater than the number of bits available in the stream,
 * the stream becomes invalid.
 * If the stream is already invalid, this has no effect.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream    The bit stream object.
 * \param bits_to_skip  The number of bits to skip.
 */
void vc_container_bits_skip(VC_CONTAINER_BITS_T *bit_stream, uint32_t bits_to_skip);

/** Skip past a number of bytes in the stream.
 * If bytes_to_skip is greater than the number of bytes available in the stream,
 * the stream becomes invalid.
 * If the stream is already invalid, this has no effect.
 *
 * \pre  bit_stream is not NULL.
 * \pre  The stream is on a byte boundary.
 *
 * \param bit_stream    The bit stream object.
 * \param bytes_to_skip The number of bytes to skip.
 */
void vc_container_bits_skip_bytes(VC_CONTAINER_BITS_T *bit_stream, uint32_t bytes_to_skip);

/** Reduce the length of the bit stream by a number of bytes.
 * This reduces the number of bits/bytes available without changing the current
 * position in the stream. If bytes_to_reduce is greater than the number of
 * bytes available in the stream, the stream becomes invalid.
 * If the stream is already invalid, this has no effect.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream       The bit stream object.
 * \param bytes_to_reduce  The number of bytes by which to reduce the stream.
 */
void vc_container_bits_reduce_bytes(VC_CONTAINER_BITS_T *bit_stream, uint32_t bytes_to_reduce);

/** Copies a number of bytes from the stream to a byte buffer.
 * If the stream is or becomes invalid, no data is copied.
 *
 * \pre  bit_stream is not NULL.
 * \pre  The stream is on a byte boundary.
 *
 * \param bit_stream    The bit stream object.
 * \param bytes_to_copy The number of bytes to copy.
 * \param dst           The byte buffer destination.
 */
void vc_container_bits_copy_bytes(VC_CONTAINER_BITS_T *bit_stream, uint32_t bytes_to_copy, uint8_t *dst);

/** Returns the next value_bits from the stream. The last bit will be the least
 * significant bit in the returned value.
 * If value_bits is greater than the number of bits available in the stream,
 * the stream becomes invalid.
 * If the stream is invalid, or becomes invalid while reading the value, zero
 * is returned.
 *
 * \pre  bit_stream is not NULL.
 * \pre  value_bits is not larger than 32.
 *
 * \param bit_stream The bit stream object.
 * \param value_bits The number of bits to retrieve.
 * \return  The value read from the stream, or zero if the stream is invalid.
 */
uint32_t vc_container_bits_read_u32(VC_CONTAINER_BITS_T *bit_stream, uint32_t value_bits);

/** Skips the next Exp-Golomb value in the stream.
 * See section 9.1 of ITU-T REC H.264 201003 for details.
 * If there are not enough bits in the stream to complete an Exp-Golomb value,
 * the stream becomes invalid.
 * If the stream is already invalid, this has no effect.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 */
void vc_container_bits_skip_exp_golomb(VC_CONTAINER_BITS_T *bit_stream);

/** Returns the next unsigned Exp-Golomb value from the stream.
 * See section 9.1 of ITU-T REC H.264 201003 for details.
 * If there are not enough bits in the stream to complete an Exp-Golomb value,
 * the stream becomes invalid.
 * If the next unsigned Exp-Golomb value in the stream is larger than 32 bits,
 * or the stream is or becomes invalid, zero is returned.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  The next unsigned value from the stream, or zero on error.
 */
uint32_t vc_container_bits_read_u32_exp_golomb(VC_CONTAINER_BITS_T *bit_stream);

/** Returns the next signed Exp-Golomb value from the stream.
 * See section 9.1.1 of ITU-T REC H.264 201003 for details.
 * If there are not enough bits in the stream to complete an Exp-Golomb value,
 * the stream becomes invalid.
 * If the next signed Exp-Golomb value in the stream is larger than 32 bits,
 * or the stream is or becomes invalid, zero is returned.
 *
 * \pre  bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  The next signed value from the stream, or zero on error.
 */
int32_t vc_container_bits_read_s32_exp_golomb(VC_CONTAINER_BITS_T *bit_stream);

/******************************************************************************
 * Macros reduce function name length and enable logging of some operations   *
 ******************************************************************************/
#define BITS_INIT(ctx, bits, buffer, available) (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_init(bits, buffer, available))
#define BITS_INVALIDATE(ctx, bits)              (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_invalidate(bits))
#define BITS_VALID(ctx, bits)                   (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_valid(bits))
#define BITS_RESET(ctx, bits)                   (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_reset(bits))
#define BITS_AVAILABLE(ctx, bits)               (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_available(bits))
#define BITS_BYTES_AVAILABLE(ctx, bits)         (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_bytes_available(bits))
#define BITS_CURRENT_POINTER(ctx, bits)         (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_current_pointer(bits))
#define BITS_COPY_STREAM(ctx, dst, src)         (VC_CONTAINER_PARAM_UNUSED(ctx), vc_container_bits_copy_stream(dst, src))

#ifdef    ENABLE_CONTAINERS_LOG_FORMAT

typedef enum {
   VC_CONTAINER_BITS_LOG_SKIP,
   VC_CONTAINER_BITS_LOG_SKIP_BYTES,
   VC_CONTAINER_BITS_LOG_U8,
   VC_CONTAINER_BITS_LOG_U16,
   VC_CONTAINER_BITS_LOG_U32,
   VC_CONTAINER_BITS_LOG_COPY_BYTES,
   VC_CONTAINER_BITS_LOG_REDUCE_BYTES,
   VC_CONTAINER_BITS_LOG_EG_SKIP,
   VC_CONTAINER_BITS_LOG_EG_U32,
   VC_CONTAINER_BITS_LOG_EG_S32,
} VC_CONTAINER_BITS_LOG_OP_T;

/** Logs an operation with void return.
 *
 * \pre  None of p_ctx, txt or bit_stream are NULL.
 *
 * \param p_ctx      Container context.
 * \param indent     Indent level.
 * \param txt        Description of what is being read.
 * \param bit_stream The bit stream object.
 * \param op         The operation just performed.
 * \param length     The length of the operation.
 */
void vc_container_bits_log(VC_CONTAINER_T *p_ctx, uint32_t indent, const char *txt, VC_CONTAINER_BITS_T *bit_stream, VC_CONTAINER_BITS_LOG_OP_T op, uint32_t length);

/** Logs an operation with unsigned 32-bit integer return.
 *
 * \pre  None of p_ctx, txt or bit_stream are NULL.
 *
 * \param p_ctx      Container context.
 * \param indent     Indent level.
 * \param txt        Description of what is being read.
 * \param bit_stream The bit stream object.
 * \param op         The operation just performed.
 * \param length     The length of the operation.
 * \param value      The value returned by the operation.
 * \return  The unsigned 32-bit integer value passed in.
 */
uint32_t vc_container_bits_log_u32(VC_CONTAINER_T *p_ctx, uint32_t indent, const char *txt, VC_CONTAINER_BITS_T *bit_stream, VC_CONTAINER_BITS_LOG_OP_T op, uint32_t length, uint32_t value);

/** Logs an operation with signed 32-bit integer return.
 *
 * \pre  None of p_ctx, txt or bit_stream are NULL.
 *
 * \param p_ctx      Container context.
 * \param indent     Indent level.
 * \param txt        Description of what is being read.
 * \param bit_stream The bit stream object.
 * \param op         The operation just performed.
 * \param length     The length of the operation.
 * \param value      The value returned by the operation.
 * \return  The signed 32-bit integer value passed in.
 */
int32_t vc_container_bits_log_s32(VC_CONTAINER_T *p_ctx, uint32_t indent, const char *txt, VC_CONTAINER_BITS_T *bit_stream, VC_CONTAINER_BITS_LOG_OP_T op, uint32_t length, int32_t value);

#ifndef BITS_LOG_INDENT
# ifndef CONTAINER_HELPER_LOG_INDENT
#  define BITS_LOG_INDENT(ctx) 0
# else
#  define BITS_LOG_INDENT(ctx) ((ctx)->priv->io->module ? CONTAINER_HELPER_LOG_INDENT(a) : 0)
# endif
#endif

#define BITS_SKIP(ctx, bits, length, txt)             (vc_container_bits_skip(bits, length), vc_container_bits_log(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_SKIP, length))
#define BITS_SKIP_BYTES(ctx, bits, length, txt)       (vc_container_bits_skip_bytes(bits, length), vc_container_bits_log(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_SKIP_BYTES, length))

#define BITS_READ_U8(ctx, bits, length, txt)          (uint8_t)vc_container_bits_log_u32(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_U8, length, vc_container_bits_read_u32(bits, length))
#define BITS_READ_U16(ctx, bits, length, txt)         (uint16_t)vc_container_bits_log_u32(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_U16, length, vc_container_bits_read_u32(bits, length))
#define BITS_READ_U32(ctx, bits, length, txt)         vc_container_bits_log_u32(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_U32, length, vc_container_bits_read_u32(bits, length))

#define BITS_COPY_BYTES(ctx, bits, length, dst, txt)  (vc_container_bits_copy_bytes(bits, length, dst), vc_container_bits_log(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_COPY_BYTES, length))

#define BITS_REDUCE_BYTES(ctx, bits, length, txt)     (vc_container_bits_reduce_bytes(bits, length), vc_container_bits_log(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_REDUCE_BYTES, length))

#define BITS_SKIP_EXP(ctx, bits, txt)                 (vc_container_bits_skip_exp_golomb(bits), vc_container_bits_log(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_EG_SKIP, 0))

#define BITS_READ_S32_EXP(ctx, bits, txt)             vc_container_bits_log_s32(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_EG_S32, 0, vc_container_bits_read_s32_exp_golomb(bits))
#define BITS_READ_U32_EXP(ctx, bits, txt)             vc_container_bits_log_u32(ctx, BITS_LOG_INDENT(ctx), txt, bits, VC_CONTAINER_BITS_LOG_EG_U32, 0, vc_container_bits_read_u32_exp_golomb(bits))

#else  /* ENABLE_CONTAINERS_LOG_FORMAT */

#define BITS_SKIP(ctx, bits, length, txt)             (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_skip(bits, length))
#define BITS_SKIP_BYTES(ctx, bits, length, txt)       (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_skip_bytes(bits, length))

#define BITS_READ_U8(ctx, bits, length, txt)          (uint8_t)(VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_read_u32(bits, length))
#define BITS_READ_U16(ctx, bits, length, txt)         (uint16_t)(VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_read_u32(bits, length))
#define BITS_READ_U32(ctx, bits, length, txt)         (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_read_u32(bits, length))

#define BITS_COPY_BYTES(ctx, bits, length, dst, txt)  (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_copy_bytes(bits, length, dst))

#define BITS_REDUCE_BYTES(ctx, bits, length, txt)     (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_reduce_bytes(bits, length))

#define BITS_SKIP_EXP(ctx, bits, txt)                 (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_skip_exp_golomb(bits))

#define BITS_READ_S32_EXP(ctx, bits, txt)             (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_read_s32_exp_golomb(bits))
#define BITS_READ_U32_EXP(ctx, bits, txt)             (VC_CONTAINER_PARAM_UNUSED(ctx), VC_CONTAINER_PARAM_UNUSED(txt), vc_container_bits_read_u32_exp_golomb(bits))

#endif /* ENABLE_CONTAINERS_LOG_FORMAT */

#endif /* VC_CONTAINERS_BITS_H */
