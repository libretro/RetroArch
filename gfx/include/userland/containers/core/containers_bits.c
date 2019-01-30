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

#include "containers/core/containers_bits.h"
#include "containers/core/containers_common.h"

#ifdef    ENABLE_CONTAINERS_LOG_FORMAT
#include "containers/core/containers_logging.h"
#endif

/******************************************************************************
Defines and constants.
******************************************************************************/

#ifdef    ENABLE_CONTAINERS_LOG_FORMAT
/** String used for indentation. If more spaces are needed, just add them. */
#define INDENT_SPACES_STRING  ">                         "
#define INDENT_SPACES_LENGTH  (sizeof(INDENT_SPACES_STRING) - 1)
#endif /* ENABLE_CONTAINERS_LOG_FORMAT */

/******************************************************************************
Type definitions
******************************************************************************/

/******************************************************************************
Function prototypes
******************************************************************************/

/******************************************************************************
Local Functions
******************************************************************************/

#ifdef    ENABLE_CONTAINERS_LOG_FORMAT

/**************************************************************************//**
 * Returns a string that indicates whether the bit stream is valid or not.
 *
 * \pre bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  A string indicating the validity of the stream.
 */
static const char * vc_container_bits_valid_str( VC_CONTAINER_BITS_T *bit_stream )
{
   return vc_container_bits_valid(bit_stream) ? "" : " - stream invalid";
}

/**************************************************************************//**
 * Returns a string of spaces the length of which is determined by the
 * parameter.
 * The length is limited to a certain size, above which a greater than symbol
 * prefixes the maximum number of spaces.
 *
 * \param length  The required length of the string.
 * \return  A string indicating the validity of the stream.
 */
static const char * vc_container_bits_indent_str(uint32_t length)
{
   uint32_t str_length = length;

   if (str_length > INDENT_SPACES_LENGTH)
      str_length = INDENT_SPACES_LENGTH;

   return INDENT_SPACES_STRING + (INDENT_SPACES_LENGTH - str_length);
}

#endif /* ENABLE_CONTAINERS_LOG_FORMAT */

/**************************************************************************//**
 * Returns the number of consecutive zero bits in the stream.
 * the zero bits are terminated either by a one bit, or the end of the stream.
 * In the former case, the zero bits and the terminating one bit are removed
 * from the stream.
 * In the latter case, the stream becomes invalid. The stream also becomes
 * invalid if there are not as many bits after the one bit as zero bits before
 * it.
 * If the stream is already or becomes invalid, zero is returned.
 *
 * \pre bit_stream is not NULL.
 *
 * \param bit_stream The bit stream object.
 * \return  The number of consecutive zero bits, or zero if the stream is
 *          invalid.
 */
static uint32_t vc_container_bits_get_leading_zero_bits( VC_CONTAINER_BITS_T *bit_stream )
{
   uint32_t leading_zero_bits;
   uint32_t bits_left = vc_container_bits_available(bit_stream);
   uint32_t bits;
   uint8_t mask, current_byte;

   if (!bits_left)
      return vc_container_bits_invalidate(bit_stream);

   /* Cache 'bits' field to avoid repeated pointer access */
   bits = bit_stream->bits;
   if (bits)
   {
      current_byte = *bit_stream->buffer;
      mask = 1 << (bits - 1);
   } else {
      /* Initialize variables to placate the compiler */
      current_byte = 0;
      mask = 0;
   }

   /* Scan for the first one bit, counting the number of zeroes. This gives the
    * number of further bits after the one that are part of the value. See
    * section 9.1 of ITU-T REC H.264 201003 for more details. */

   for (leading_zero_bits = 0; leading_zero_bits < bits_left; leading_zero_bits++)
   {
      if (!bits)
      {
         if (!bit_stream->bytes)
            return vc_container_bits_invalidate(bit_stream);
         bit_stream->bytes--;
         current_byte = *(++bit_stream->buffer);
         bits = 8;
         mask = 0x80;
      }

      bits--;
      bits_left--;
      if (current_byte & mask)
         break;      /* Found the marker bit */

      mask >>= 1;
   }

   /* Check enough bits are left in the stream for the value. */
   if (leading_zero_bits > bits_left)
      return vc_container_bits_invalidate(bit_stream);

   /* Return cached value of bits to the stream */
   bit_stream->bits = bits;
   
   return leading_zero_bits;
}

/*****************************************************************************
Functions exported as part of the bit stream API
 *****************************************************************************/

/*****************************************************************************/
void vc_container_bits_init(VC_CONTAINER_BITS_T *bit_stream,
      const uint8_t *buffer,
      uint32_t available)
{
   vc_container_assert(buffer && (buffer != (const uint8_t *)1));

   /* Start with buffer pointing at the previous byte with no bits available
    * to make the mathematics easier */
   bit_stream->buffer = buffer - 1;
   bit_stream->bytes = available;
   bit_stream->bits = 0;
}

/*****************************************************************************/
uint32_t vc_container_bits_invalidate( VC_CONTAINER_BITS_T *bit_stream )
{
   bit_stream->buffer = NULL;
   return 0;
}

/*****************************************************************************/
bool vc_container_bits_valid(VC_CONTAINER_BITS_T *bit_stream)
{
   return (bit_stream->buffer != NULL);
}

/*****************************************************************************/
void vc_container_bits_reset(VC_CONTAINER_BITS_T *bit_stream)
{
   bit_stream->bytes = 0;
   bit_stream->bits = 0;
}

/*****************************************************************************/
const uint8_t *vc_container_bits_current_pointer(const VC_CONTAINER_BITS_T *bit_stream)
{
   const uint8_t *buffer = bit_stream->buffer;

   /* Only valid on byte boundaries, where buffer pointer has not been moved yet */
   vc_container_assert(!bit_stream->bits);

   return buffer ? (buffer + 1) : NULL;
}

/*****************************************************************************/
void vc_container_bits_copy_stream(VC_CONTAINER_BITS_T *dst,
      const VC_CONTAINER_BITS_T *src)
{
   memcpy(dst, src, sizeof(VC_CONTAINER_BITS_T));
}

/*****************************************************************************/
uint32_t vc_container_bits_available(const VC_CONTAINER_BITS_T *bit_stream)
{
   if (!bit_stream->buffer)
      return 0;
   return (bit_stream->bytes << 3) + bit_stream->bits;
}

/*****************************************************************************/
uint32_t vc_container_bits_bytes_available(const VC_CONTAINER_BITS_T *bit_stream)
{
   if (!bit_stream->buffer)
      return 0;

   vc_container_assert(!bit_stream->bits);

   return vc_container_bits_available(bit_stream) >> 3;
}

/*****************************************************************************/
void vc_container_bits_skip(VC_CONTAINER_BITS_T *bit_stream,
      uint32_t bits_to_skip)
{
   uint32_t have_bits;
   uint32_t new_bytes;

   have_bits = vc_container_bits_available(bit_stream);
   if (have_bits < bits_to_skip)
   {
      vc_container_bits_invalidate(bit_stream);
      return;
   }

   have_bits -= bits_to_skip;
   new_bytes = have_bits >> 3;
   bit_stream->bits = have_bits & 7;
   bit_stream->buffer += (bit_stream->bytes - new_bytes);
   bit_stream->bytes = new_bytes;
}

/*****************************************************************************/
void vc_container_bits_skip_bytes(VC_CONTAINER_BITS_T *bit_stream,
      uint32_t bytes_to_skip)
{
   /* Only valid on byte boundaries */
   vc_container_assert(!bit_stream->bits);

   vc_container_bits_skip(bit_stream, bytes_to_skip << 3);
}

/*****************************************************************************/
void vc_container_bits_reduce_bytes(VC_CONTAINER_BITS_T *bit_stream,
      uint32_t bytes_to_reduce)
{
   if (bit_stream->bytes >= bytes_to_reduce)
      bit_stream->bytes -= bytes_to_reduce;
   else
      vc_container_bits_invalidate(bit_stream);
}

/*****************************************************************************/
void vc_container_bits_copy_bytes(VC_CONTAINER_BITS_T *bit_stream,
      uint32_t bytes_to_copy,
      uint8_t *dst)
{
   vc_container_assert(!bit_stream->bits);

   if (bit_stream->bytes < bytes_to_copy)
   {
      /* Not enough data */
      vc_container_bits_invalidate(bit_stream);
      return;
   }

   /* When the number of bits is zero, the next byte to take is at buffer + 1 */
   memcpy(dst, bit_stream->buffer + 1, bytes_to_copy);
   bit_stream->buffer += bytes_to_copy;
   bit_stream->bytes -= bytes_to_copy;
}

/*****************************************************************************/
uint32_t vc_container_bits_read_u32(VC_CONTAINER_BITS_T *bit_stream,
      uint32_t value_bits)
{
   uint32_t value = 0;
   uint32_t needed = value_bits;
   uint32_t bits;

   vc_container_assert(value_bits <= 32);

   if (needed > vc_container_bits_available(bit_stream))
      return vc_container_bits_invalidate(bit_stream);

   bits = bit_stream->bits;
   while (needed)
   {
      uint32_t take;

      if (!bits)
      {
         bit_stream->bytes--;
         bit_stream->buffer++;
         bits = 8;
      }

      take = bits;
      if (needed < take) take = needed;

      bits -= take;
      needed -= take;

      value <<= take;
      if (take == 8)
         value |= *bit_stream->buffer;  /* optimize whole byte case */
      else
         value |= (*bit_stream->buffer >> bits) & ((1 << take) - 1);
   }

   bit_stream->bits = bits;
   return value;
}

/*****************************************************************************/
void vc_container_bits_skip_exp_golomb(VC_CONTAINER_BITS_T *bit_stream)
{
   vc_container_bits_skip(bit_stream, vc_container_bits_get_leading_zero_bits(bit_stream));
}

/*****************************************************************************/
uint32_t vc_container_bits_read_u32_exp_golomb(VC_CONTAINER_BITS_T *bit_stream)
{
   uint32_t leading_zero_bits;
   uint32_t codeNum;

   leading_zero_bits = vc_container_bits_get_leading_zero_bits(bit_stream);

   /* Anything bigger than 32 bits is definitely overflow */
   if (leading_zero_bits > 32)
      return vc_container_bits_invalidate(bit_stream);

   codeNum = vc_container_bits_read_u32(bit_stream, leading_zero_bits);

   if (leading_zero_bits == 32)
   {
      /* If codeNum is non-zero, it would need 33 bits, so is also overflow */
      if (codeNum)
         return vc_container_bits_invalidate(bit_stream);

      return 0xFFFFFFFF;
   }

   return codeNum + (1 << leading_zero_bits) - 1;
}

/*****************************************************************************/
int32_t vc_container_bits_read_s32_exp_golomb(VC_CONTAINER_BITS_T *bit_stream)
{
   uint32_t uval;

   uval = vc_container_bits_read_u32_exp_golomb(bit_stream);

   /* The signed Exp-Golomb code 0xFFFFFFFF cannot be represented as a signed 32-bit
    * integer, because it should be one larger than the largest positive value. */
   if (uval == 0xFFFFFFFF)
      return vc_container_bits_invalidate(bit_stream);

   /* Definition of conversion is
    *    s = ((-1)^(u + 1)) * Ceil(u / 2)
    * where '^' is power, but this should be equivalent */
   return ((int32_t)((uval & 1) << 1) - 1) * (int32_t)((uval >> 1) + (uval & 1));
}

#ifdef    ENABLE_CONTAINERS_LOG_FORMAT

/*****************************************************************************/
void vc_container_bits_log(VC_CONTAINER_T *p_ctx,
      uint32_t indent,
      const char *txt,
      VC_CONTAINER_BITS_T *bit_stream,
      VC_CONTAINER_BITS_LOG_OP_T op,
      uint32_t length)
{
   const char *valid_str = vc_container_bits_valid_str(bit_stream);
   const char *indent_str = vc_container_bits_indent_str(indent);

   switch (op)
   {
   case VC_CONTAINER_BITS_LOG_SKIP:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: %u bits skipped%s", indent_str, txt, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_SKIP_BYTES:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: %u bytes skipped%s", indent_str, txt, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_COPY_BYTES:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: %u bytes copied%s", indent_str, txt, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_REDUCE_BYTES:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: %u bytes reduced%s", indent_str, txt, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_EG_SKIP:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: Exp-Golomb value skipped%s", indent_str, txt, valid_str);
      break;
   default:
      /* Unexpected operation. Check bit stream logging macros */
      vc_container_assert(0);
   }
}

/*****************************************************************************/
uint32_t vc_container_bits_log_u32(VC_CONTAINER_T *p_ctx,
      uint32_t indent,
      const char *txt,
      VC_CONTAINER_BITS_T *bit_stream,
      VC_CONTAINER_BITS_LOG_OP_T op,
      uint32_t length,
      uint32_t value)
{
   const char *valid_str = vc_container_bits_valid_str(bit_stream);
   const char *indent_str = vc_container_bits_indent_str(indent);

   switch (op)
   {
   case VC_CONTAINER_BITS_LOG_U8:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: 0x%02x (%u) in %u bits%s", indent_str, txt, value, value, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_U16:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: 0x%04x (%u) in %u bits%s", indent_str, txt, value, value, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_U32:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: 0x%08x (%u) in %u bits%s", indent_str, txt, value, value, length, valid_str);
      break;
   case VC_CONTAINER_BITS_LOG_EG_U32:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: 0x%08x (%u) unsigned Exp-Golomb%s", indent_str, txt, value, value, valid_str);
      break;
   default:
      /* Unexpected operation. Check bit stream logging macros */
      vc_container_assert(0);
   }

   return value;
}

/*****************************************************************************/
int32_t vc_container_bits_log_s32(VC_CONTAINER_T *p_ctx,
      uint32_t indent,
      const char *txt,
      VC_CONTAINER_BITS_T *bit_stream,
      VC_CONTAINER_BITS_LOG_OP_T op,
      uint32_t length,
      int32_t value)
{
   const char *valid_str = vc_container_bits_valid_str(bit_stream);
   const char *indent_str = vc_container_bits_indent_str(indent);

   VC_CONTAINER_PARAM_UNUSED(length);

   switch (op)
   {
   case VC_CONTAINER_BITS_LOG_EG_S32:
      vc_container_log(p_ctx, VC_CONTAINER_LOG_FORMAT, "%s%s: 0x%08x (%d) signed Exp-Golomb%s", indent_str, txt, value, value, valid_str);
      break;
   default:
      /* Unexpected operation. Check bit stream logging macros */
      vc_container_assert(0);
   }

   return value;
}

#endif /* ENABLE_CONTAINERS_LOG_FORMAT */
