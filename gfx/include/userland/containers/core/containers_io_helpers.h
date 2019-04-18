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
#ifndef VC_CONTAINERS_IO_HELPERS_H
#define VC_CONTAINERS_IO_HELPERS_H

/** \file containers_io_helpers.h
 * Helper functions and macros which provide functionality which is often used by containers
 */

#include "containers/core/containers_io.h"
#include "containers/core/containers_utils.h"

/*****************************************************************************
 * Helper inline functions to read integers from an i/o stream
 *****************************************************************************/

/** Reads an unsigned 8 bits integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint8_t vc_container_io_read_uint8(VC_CONTAINER_IO_T *io)
{
   uint8_t value;
   size_t ret = vc_container_io_read(io, &value, 1);
   return ret == 1 ? value : 0;
}

/** Reads a FOURCC from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The FOURCC to read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE VC_CONTAINER_FOURCC_T vc_container_io_read_fourcc(VC_CONTAINER_IO_T *io)
{
   VC_CONTAINER_FOURCC_T value;
   size_t ret = vc_container_io_read(io, (int8_t *)&value, 4);
   return ret == 4 ? value : 0;
}

/** Reads an unsigned 16 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint16_t vc_container_io_read_be_uint16(VC_CONTAINER_IO_T *io)
{
   uint8_t value[2];
   size_t ret = vc_container_io_read(io, value, 2);
   return ret == 2 ? (value[0] << 8) | value[1] : 0;
}

/** Reads an unsigned 24 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_read_be_uint24(VC_CONTAINER_IO_T *io)
{
   uint8_t value[3];
   size_t ret = vc_container_io_read(io, value, 3);
   return ret == 3 ? (value[0] << 16) | (value[1] << 8) | value[2] : 0;
}

/** Reads an unsigned 32 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_read_be_uint32(VC_CONTAINER_IO_T *io)
{
   uint8_t value[4];
   size_t ret = vc_container_io_read(io, value, 4);
   return ret == 4 ? (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3] : 0;
}

/** Reads an unsigned 40 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_be_uint40(VC_CONTAINER_IO_T *io)
{
   uint8_t value[5];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 5);

   value1 = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
   value2 = value[4];

   return ret == 5 ? (((uint64_t)value1) << 8)|value2 : 0;
}

/** Reads an unsigned 48 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_be_uint48(VC_CONTAINER_IO_T *io)
{
   uint8_t value[6];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 6);

   value1 = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
   value2 = (value[4] << 8) | value[5];

   return ret == 6 ? (((uint64_t)value1) << 16)|value2 : 0;
}

/** Reads an unsigned 56 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_be_uint56(VC_CONTAINER_IO_T *io)
{
   uint8_t value[7];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 7);

   value1 = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
   value2 = (value[4] << 16) | (value[5] << 8) | value[6];

   return ret == 7 ? (((uint64_t)value1) << 24)|value2 : 0;
}

/** Reads an unsigned 64 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_be_uint64(VC_CONTAINER_IO_T *io)
{
   uint8_t value[8];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 8);

   value1 = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
   value2 = (value[4] << 24) | (value[5] << 16) | (value[6] << 8) | value[7];

   return ret == 8 ? (((uint64_t)value1) << 32)|value2 : 0;
}

/** Reads an unsigned 16 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint16_t vc_container_io_read_le_uint16(VC_CONTAINER_IO_T *io)
{
   uint8_t value[2];
   size_t ret = vc_container_io_read(io, value, 2);
   return ret == 2 ? (value[1] << 8) | value[0] : 0;
}

/** Reads an unsigned 24 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_read_le_uint24(VC_CONTAINER_IO_T *io)
{
   uint8_t value[3];
   size_t ret = vc_container_io_read(io, value, 3);
   return ret == 3 ? (value[2] << 16) | (value[1] << 8) | value[0] : 0;
}

/** Reads an unsigned 32 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_read_le_uint32(VC_CONTAINER_IO_T *io)
{
   uint8_t value[4];
   size_t ret = vc_container_io_read(io, value, 4);
   return ret == 4 ? (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0] : 0;
}

/** Reads an unsigned 40 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_le_uint40(VC_CONTAINER_IO_T *io)
{
   uint8_t value[5];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 5);

   value1 = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
   value2 = value[4];

   return ret == 5 ? (((uint64_t)value2) << 32)|value1 : 0;
}

/** Reads an unsigned 48 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_le_uint48(VC_CONTAINER_IO_T *io)
{
   uint8_t value[6];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 6);

   value1 = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
   value2 = (value[5] << 8) | value[4];

   return ret == 6 ? (((uint64_t)value2) << 32)|value1 : 0;
}

/** Reads an unsigned 56 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_le_uint56(VC_CONTAINER_IO_T *io)
{
   uint8_t value[7];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 7);

   value1 = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
   value2 = (value[6] << 16) | (value[5] << 8) | value[4];

   return ret == 7 ? (((uint64_t)value2) << 32)|value1 : 0;
}

/** Reads an unsigned 64 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_read_le_uint64(VC_CONTAINER_IO_T *io)
{
   uint8_t value[8];
   uint32_t value1, value2;
   size_t ret = vc_container_io_read(io, value, 8);

   value1 = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
   value2 = (value[7] << 24) | (value[6] << 16) | (value[5] << 8) | value[4];

   return ret == 8 ? (((uint64_t)value2) << 32)|value1 : 0;
}

/*****************************************************************************
 * Helper inline functions to peek integers from an i/o stream
 *****************************************************************************/

/** Peeks an unsigned 8 bits integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint8_t vc_container_io_peek_uint8(VC_CONTAINER_IO_T *io)
{
   uint8_t value;
   size_t ret = vc_container_io_peek(io, &value, 1);
   return ret == 1 ? value : 0;
}

/** Peeks an unsigned 16 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint16_t vc_container_io_peek_be_uint16(VC_CONTAINER_IO_T *io)
{
   uint8_t value[2];
   size_t ret = vc_container_io_peek(io, value, 2);
   return ret == 2 ? (value[0] << 8) | value[1] : 0;
}

/** Peeks an unsigned 24 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_peek_be_uint24(VC_CONTAINER_IO_T *io)
{
   uint8_t value[3];
   size_t ret = vc_container_io_peek(io, value, 3);
   return ret == 3 ? (value[0] << 16) | (value[1] << 8) | value[2] : 0;
}

/** Peeks an unsigned 32 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_peek_be_uint32(VC_CONTAINER_IO_T *io)
{
   uint8_t value[4];
   size_t ret = vc_container_io_peek(io, value, 4);
   return ret == 4 ? (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3] : 0;
}

/** Peeks an unsigned 64 bits big endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_peek_be_uint64(VC_CONTAINER_IO_T *io)
{
   uint8_t value[8];
   uint32_t value1, value2;
   size_t ret = vc_container_io_peek(io, value, 8);

   value1 = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];
   value2 = (value[4] << 24) | (value[5] << 16) | (value[6] << 8) | value[7];

   return ret == 8 ? (((uint64_t)value1) << 32)|value2 : 0;
}

/** Peeks an unsigned 16 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint16_t vc_container_io_peek_le_uint16(VC_CONTAINER_IO_T *io)
{
   uint8_t value[2];
   size_t ret = vc_container_io_peek(io, value, 2);
   return ret == 2 ? (value[1] << 8) | value[0] : 0;
}

/** Peeks an unsigned 24 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_peek_le_uint24(VC_CONTAINER_IO_T *io)
{
   uint8_t value[3];
   size_t ret = vc_container_io_peek(io, value, 3);
   return ret == 3 ? (value[2] << 16) | (value[1] << 8) | value[0] : 0;
}

/** Peeks an unsigned 32 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint32_t vc_container_io_peek_le_uint32(VC_CONTAINER_IO_T *io)
{
   uint8_t value[4];
   size_t ret = vc_container_io_peek(io, value, 4);
   return ret == 4 ? (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0] : 0;
}

/** Peeks an unsigned 64 bits little endian integer from an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \return             The integer read. In case of failure during the read,
 *                     this will return a value of 0.
 */
STATIC_INLINE uint64_t vc_container_io_peek_le_uint64(VC_CONTAINER_IO_T *io)
{
   uint8_t value[8];
   uint32_t value1, value2;
   size_t ret = vc_container_io_peek(io, value, 8);

   value1 = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
   value2 = (value[7] << 24) | (value[6] << 16) | (value[5] << 8) | value[4];

   return ret == 8 ? (((uint64_t)value2) << 32)|value1 : 0;
}

/*****************************************************************************
 * Helper inline functions to write integers to an i/o stream
 *****************************************************************************/

/** Writes an unsigned 8 bits integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_uint8(VC_CONTAINER_IO_T *io, uint8_t value)
{
   size_t ret = vc_container_io_write(io, &value, 1);
   return ret == 1 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes a FOURCC to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The FOURCC to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_fourcc(VC_CONTAINER_IO_T *io, VC_CONTAINER_FOURCC_T value)
{
   size_t ret = vc_container_io_write(io, (uint8_t *)&value, 4);
   return ret == 4 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 16 bits big endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_be_uint16(VC_CONTAINER_IO_T *io, uint16_t value)
{
   uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
   size_t ret = vc_container_io_write(io, bytes, 2);
   return ret == 2 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 24 bits big endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_be_uint24(VC_CONTAINER_IO_T *io, uint32_t value)
{
   uint8_t bytes[3] = {(uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value};
   size_t ret = vc_container_io_write(io, bytes, 3);
   return ret == 3 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 32 bits big endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_be_uint32(VC_CONTAINER_IO_T *io, uint32_t value)
{
   uint8_t bytes[4] = {value >> 24, value >> 16, value >> 8, value};
   size_t ret = vc_container_io_write(io, bytes, 4);
   return ret == 4 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 64 bits big endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_be_uint64(VC_CONTAINER_IO_T *io, uint64_t value)
{
   uint8_t bytes[8] =
   {
      (uint8_t)(value >> 56),
      (uint8_t)(value >> 48),
      (uint8_t)(value >> 40),
      (uint8_t)(value >> 32),
      (uint8_t)(value >> 24),
      (uint8_t)(value >> 16),
      (uint8_t)(value >> 8),
      (uint8_t) value
   };
   size_t ret = vc_container_io_write(io, bytes, 8);
   return ret == 8 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 16 bits little endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_le_uint16(VC_CONTAINER_IO_T *io, uint16_t value)
{
   uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
   size_t ret = vc_container_io_write(io, bytes, 2);
   return ret == 2 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 24 bits little endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_le_uint24(VC_CONTAINER_IO_T *io, uint32_t value)
{
   uint8_t bytes[3] = {value, value >> 8, value >> 16};
   size_t ret = vc_container_io_write(io, bytes, 3);
   return ret == 3 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 32 bits little endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_le_uint32(VC_CONTAINER_IO_T *io, uint32_t value)
{
   uint8_t bytes[4] = {value, value >> 8, value >> 16, value >> 24};
   size_t ret = vc_container_io_write(io, bytes, 4);
   return ret == 4 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/** Writes an unsigned 64 bits little endian integer to an i/o stream.
 * \param  io          Pointer to the VC_CONTAINER_IO_T instance to use
 * \param  value       The integer to write.
 * \return             The status of the operation.
 */
STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_io_write_le_uint64(VC_CONTAINER_IO_T *io, uint64_t value)
{
   uint8_t bytes[8] =
   {
      (uint8_t) value,
      (uint8_t)(value >> 8),
      (uint8_t)(value >> 16),
      (uint8_t)(value >> 24),
      (uint8_t)(value >> 32),
      (uint8_t)(value >> 40),
      (uint8_t)(value >> 48),
      (uint8_t)(value >> 56)
   };
   size_t ret = vc_container_io_write(io, bytes, 8);
   return ret == 8 ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
}

/*****************************************************************************
 * Helper macros for accessing the i/o stream. These will also call the right
 * functions depending on the endianness defined.
 *****************************************************************************/

/** Macro which returns the current position within the stream */
#define STREAM_POSITION(ctx) (ctx)->priv->io->offset
/** Macro which returns true if the end of stream has been reached */
#define STREAM_EOS(ctx) ((ctx)->priv->io->status == VC_CONTAINER_ERROR_EOS)
/** Macro which returns the status of the stream */
#define STREAM_STATUS(ctx) (ctx)->priv->io->status
/** Macro which returns true if an error other than end of stream has occurred */
#define STREAM_ERROR(ctx) ((ctx)->priv->io->status && (ctx)->priv->io->status != VC_CONTAINER_ERROR_EOS)
/** Macro which returns true if we can seek into the stream */
#define STREAM_SEEKABLE(ctx) (!((ctx)->priv->io->capabilities & VC_CONTAINER_IO_CAPS_CANT_SEEK))

#define PEEK_BYTES(ctx, buffer, size) vc_container_io_peek((ctx)->priv->io, buffer, (size_t)(size))
#define READ_BYTES(ctx, buffer, size) vc_container_io_read((ctx)->priv->io, buffer, (size_t)(size))
#define SKIP_BYTES(ctx, size) vc_container_io_skip((ctx)->priv->io, (size_t)(size))
#define SEEK(ctx, off) vc_container_io_seek((ctx)->priv->io, (int64_t)(off))
#define CACHE_BYTES(ctx, size) vc_container_io_cache((ctx)->priv->io, (size_t)(size))

#define _SKIP_GUID(ctx) vc_container_io_skip((ctx)->priv->io, 16)
#define _SKIP_U8(ctx)  (vc_container_io_skip((ctx)->priv->io, 1) != 1)
#define _SKIP_U16(ctx) (vc_container_io_skip((ctx)->priv->io, 2) != 2)
#define _SKIP_U24(ctx) (vc_container_io_skip((ctx)->priv->io, 3) != 3)
#define _SKIP_U32(ctx) (vc_container_io_skip((ctx)->priv->io, 4) != 4)
#define _SKIP_U64(ctx) (vc_container_io_skip((ctx)->priv->io, 8) != 8)
#define _SKIP_FOURCC(ctx) (vc_container_io_skip((ctx)->priv->io, 4) != 4)

#define _READ_GUID(ctx, buffer) vc_container_io_read((ctx)->priv->io, buffer, 16)
#define _READ_U8(ctx)  vc_container_io_read_uint8((ctx)->priv->io)
#define _READ_FOURCC(ctx) vc_container_io_read_fourcc((ctx)->priv->io)
#define PEEK_GUID(ctx, buffer) vc_container_io_peek((ctx)->priv->io, buffer, 16)
#define PEEK_U8(ctx)  vc_container_io_peek_uint8((ctx)->priv->io)
#ifdef CONTAINER_IS_BIG_ENDIAN
# define _READ_U16(ctx) vc_container_io_read_be_uint16((ctx)->priv->io)
# define _READ_U24(ctx) vc_container_io_read_be_uint24((ctx)->priv->io)
# define _READ_U32(ctx) vc_container_io_read_be_uint32((ctx)->priv->io)
# define _READ_U40(ctx) vc_container_io_read_be_uint40((ctx)->priv->io)
# define _READ_U48(ctx) vc_container_io_read_be_uint48((ctx)->priv->io)
# define _READ_U56(ctx) vc_container_io_read_be_uint56((ctx)->priv->io)
# define _READ_U64(ctx) vc_container_io_read_be_uint64((ctx)->priv->io)
# define PEEK_U16(ctx) vc_container_io_peek_be_uint16((ctx)->priv->io)
# define PEEK_U24(ctx) vc_container_io_peek_be_uint24((ctx)->priv->io)
# define PEEK_U32(ctx) vc_container_io_peek_be_uint32((ctx)->priv->io)
# define PEEK_U64(ctx) vc_container_io_peek_be_uint64((ctx)->priv->io)
#else
# define _READ_U16(ctx) vc_container_io_read_le_uint16((ctx)->priv->io)
# define _READ_U24(ctx) vc_container_io_read_le_uint24((ctx)->priv->io)
# define _READ_U32(ctx) vc_container_io_read_le_uint32((ctx)->priv->io)
# define _READ_U40(ctx) vc_container_io_read_le_uint40((ctx)->priv->io)
# define _READ_U48(ctx) vc_container_io_read_le_uint48((ctx)->priv->io)
# define _READ_U56(ctx) vc_container_io_read_le_uint56((ctx)->priv->io)
# define _READ_U64(ctx) vc_container_io_read_le_uint64((ctx)->priv->io)
# define PEEK_U16(ctx) vc_container_io_peek_le_uint16((ctx)->priv->io)
# define PEEK_U24(ctx) vc_container_io_peek_le_uint24((ctx)->priv->io)
# define PEEK_U32(ctx) vc_container_io_peek_le_uint32((ctx)->priv->io)
# define PEEK_U64(ctx) vc_container_io_peek_le_uint64((ctx)->priv->io)
#endif

#define WRITE_BYTES(ctx, buffer, size) vc_container_io_write((ctx)->priv->io, buffer, (size_t)(size))
#define _WRITE_GUID(ctx, buffer) vc_container_io_write((ctx)->priv->io, buffer, 16)
#define _WRITE_U8(ctx, v)  vc_container_io_write_uint8((ctx)->priv->io, v)
#define _WRITE_FOURCC(ctx, v) vc_container_io_write_fourcc((ctx)->priv->io, v)
#ifdef CONTAINER_IS_BIG_ENDIAN
# define _WRITE_U16(ctx, v) vc_container_io_write_be_uint16((ctx)->priv->io, v)
# define _WRITE_U24(ctx, v) vc_container_io_write_be_uint24((ctx)->priv->io, v)
# define _WRITE_U32(ctx, v) vc_container_io_write_be_uint32((ctx)->priv->io, v)
# define _WRITE_U64(ctx, v) vc_container_io_write_be_uint64((ctx)->priv->io, v)
#else
# define _WRITE_U16(ctx, v) vc_container_io_write_le_uint16((ctx)->priv->io, v)
# define _WRITE_U24(ctx, v) vc_container_io_write_le_uint24((ctx)->priv->io, v)
# define _WRITE_U32(ctx, v) vc_container_io_write_le_uint32((ctx)->priv->io, v)
# define _WRITE_U64(ctx, v) vc_container_io_write_le_uint64((ctx)->priv->io, v)
#endif

#ifndef CONTAINER_HELPER_LOG_INDENT
# define CONTAINER_HELPER_LOG_INDENT(a) 0
#endif

#ifdef CONTAINER_IS_BIG_ENDIAN
# define LOG_FORMAT_TYPE_UINT LOG_FORMAT_TYPE_UINT_BE
# define LOG_FORMAT_TYPE_STRING_UTF16 LOG_FORMAT_TYPE_STRING_UTF16_BE
#else
# define LOG_FORMAT_TYPE_UINT LOG_FORMAT_TYPE_UINT_LE
# define LOG_FORMAT_TYPE_STRING_UTF16 LOG_FORMAT_TYPE_STRING_UTF16_LE
#endif

#ifndef ENABLE_CONTAINERS_LOG_FORMAT
#define SKIP_GUID(ctx,n) _SKIP_GUID(ctx)
#define SKIP_U8(ctx,n)  _SKIP_U8(ctx)
#define SKIP_U16(ctx,n) _SKIP_U16(ctx)
#define SKIP_U24(ctx,n) _SKIP_U24(ctx)
#define SKIP_U32(ctx,n) _SKIP_U32(ctx)
#define SKIP_U64(ctx,n) _SKIP_U64(ctx)
#define SKIP_FOURCC(ctx,n) _SKIP_FOURCC(ctx)
#define READ_GUID(ctx,buffer,n) _READ_GUID(ctx,(uint8_t *)buffer)
#define READ_U8(ctx,n)  _READ_U8(ctx)
#define READ_U16(ctx,n) _READ_U16(ctx)
#define READ_U24(ctx,n) _READ_U24(ctx)
#define READ_U32(ctx,n) _READ_U32(ctx)
#define READ_U40(ctx,n) _READ_U40(ctx)
#define READ_U48(ctx,n) _READ_U48(ctx)
#define READ_U56(ctx,n) _READ_U56(ctx)
#define READ_U64(ctx,n) _READ_U64(ctx)
#define READ_FOURCC(ctx,n) _READ_FOURCC(ctx)
#define READ_STRING(ctx,buffer,sz,n) READ_BYTES(ctx,buffer,sz)
#define READ_STRING_UTF16(ctx,buffer,sz,n) READ_BYTES(ctx,buffer,sz)
#define SKIP_STRING(ctx,sz,n) SKIP_BYTES(ctx,sz)
#define SKIP_STRING_UTF16(ctx,sz,n) SKIP_BYTES(ctx,sz)
#else
#define SKIP_GUID(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_GUID, 16, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_U8(ctx,n)  vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 1, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_U16(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 2, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_U24(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 3, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_U32(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 4, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_U64(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 8, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_FOURCC(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_FOURCC, 4, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define READ_GUID(ctx,buffer,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_GUID, 16, n, (uint8_t *)buffer, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U8(ctx,n)  (uint8_t)vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 1, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U16(ctx,n) (uint16_t)vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 2, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U24(ctx,n) (uint32_t)vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 3, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U32(ctx,n) (uint32_t)vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 4, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U40(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 5, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U48(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 6, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U56(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 7, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_U64(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_UINT, 8, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_FOURCC(ctx,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_FOURCC, 4, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_STRING_UTF16(ctx,buffer,sz,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_STRING_UTF16, sz, n, (uint8_t *)buffer, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define READ_STRING(ctx,buffer,sz,n) vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_STRING, sz, n, (uint8_t *)buffer, CONTAINER_HELPER_LOG_INDENT(ctx), 0)
#define SKIP_STRING_UTF16(ctx,sz,n)  vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_STRING_UTF16, sz, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#define SKIP_STRING(ctx,sz,n)  vc_container_helper_read_debug(ctx, LOG_FORMAT_TYPE_STRING, sz, n, 0, CONTAINER_HELPER_LOG_INDENT(ctx), 1)
#endif

#ifndef ENABLE_CONTAINERS_LOG_FORMAT
#define WRITE_GUID(ctx,buffer,n) _WRITE_GUID(ctx,(const uint8_t *)buffer)
#define WRITE_U8(ctx,v,n) _WRITE_U8(ctx,(uint8_t)(v))
#define WRITE_FOURCC(ctx,v,n) _WRITE_FOURCC(ctx,(uint32_t)(v))
#define WRITE_U16(ctx,v,n) _WRITE_U16(ctx,(uint16_t)(v))
#define WRITE_U24(ctx,v,n) _WRITE_U24(ctx,(uint32_t)(v))
#define WRITE_U32(ctx,v,n) _WRITE_U32(ctx,(uint32_t)(v))
#define WRITE_U64(ctx,v,n) _WRITE_U64(ctx,(uint64_t)(v))
#define WRITE_STRING(ctx,buffer,size,n) WRITE_BYTES(ctx, buffer, size)
#else
#define WRITE_GUID(ctx,buffer,n) (vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_GUID, 16, n, UINT64_C(0), (const uint8_t *)buffer, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module) ? 0 : 16)
#define WRITE_U8(ctx,v,n)  vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_UINT, 1, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_FOURCC(ctx,v,n) vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_FOURCC, 4, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_U16(ctx,v,n) (uint16_t)vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_UINT, 2, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_U24(ctx,v,n) vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_UINT, 3, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_U32(ctx,v,n) vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_UINT, 4, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_U64(ctx,v,n) vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_UINT, 8, n, (uint64_t)(v), 0, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module)
#define WRITE_STRING(ctx,buffer,size,n) (vc_container_helper_write_debug(ctx, LOG_FORMAT_TYPE_STRING, size, n, UINT64_C(0), (const uint8_t *)buffer, CONTAINER_HELPER_LOG_INDENT(ctx), !(ctx)->priv->io->module) ? 0 : size)
#endif

#ifdef ENABLE_CONTAINERS_LOG_FORMAT
#define LOG_FORMAT(ctx, ...) do { if((ctx)->priv->io->module) vc_container_helper_format_debug(ctx, CONTAINER_HELPER_LOG_INDENT(ctx), __VA_ARGS__); } while(0)
#else
#define LOG_FORMAT(ctx, ...) do {} while (0)
#endif

#define LOG_FORMAT_TYPE_UINT_LE  0
#define LOG_FORMAT_TYPE_UINT_BE  1
#define LOG_FORMAT_TYPE_STRING   2
#define LOG_FORMAT_TYPE_STRING_UTF16_LE 3
#define LOG_FORMAT_TYPE_STRING_UTF16_BE 4
#define LOG_FORMAT_TYPE_FOURCC   5
#define LOG_FORMAT_TYPE_GUID     6
#define LOG_FORMAT_TYPE_HEX  0x100

uint64_t vc_container_helper_int_debug(VC_CONTAINER_T *ctx, int type, uint64_t value, const char *name, int indent);
uint64_t vc_container_helper_read_debug(VC_CONTAINER_T *ctx, int type, int size, const char *name,
   uint8_t *buffer, int indent, int b_skip);
VC_CONTAINER_STATUS_T vc_container_helper_write_debug(VC_CONTAINER_T *ctx, int type, int size, const char *name,
   uint64_t value, const uint8_t *buffer, int indent, int silent);
void vc_container_helper_format_debug(VC_CONTAINER_T *ctx, int indent, const char *format, ...);

#endif /* VC_CONTAINERS_IO_HELPERS_H */
/* End of file */
/*-----------------------------------------------------------------------------*/
