/* Copyright  (C) 2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (network_stream.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_SDK_NETWORK_STREAM_H
#define _LIBRETRO_SDK_NETWORK_STREAM_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   NETSTREAM_SEEK_SET = 0,
   NETSTREAM_SEEK_CUR,
   NETSTREAM_SEEK_END
};

typedef struct netstream
{
   void   *buf;
   size_t size;
   size_t used;
   size_t pos;
} netstream_t;

/**
 * netstream_open:
 *
 * @stream : Pointer to a network stream object.
 * @buf    : Pre-allocated buffer. Pass NULL to dynamically allocate a buffer.
 * @size   : Buffer size. Pass 0 for no pre-allocated/initial buffer.
 * @used   : Buffer bytes in use. Ignored for non pre-allocated buffers.
 *
 * Opens a network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_open(netstream_t *stream, void *buf, size_t size, size_t used);

/**
 * netstream_close:
 *
 * @stream  : Pointer to a network stream object.
 * @dealloc : Whether to deallocate/free the buffer or not.
 *
 * Closes a network stream.
 *
 */
void netstream_close(netstream_t *stream, bool dealloc);

/**
 * netstream_reset:
 *
 * @stream : Pointer to a network stream object.
 *
 * Resets a network stream to its initial position,
 * discarding any used bytes in the process.
 *
 */
void netstream_reset(netstream_t *stream);

/**
 * netstream_truncate:
 *
 * @stream : Pointer to a network stream object.
 * @used   : Amount of bytes used.
 *
 * Truncates the network stream.
 * Truncation can either extend or reduce the amount of bytes used.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_truncate(netstream_t *stream, size_t used);

/**
 * netstream_data:
 *
 * @stream : Pointer to a network stream object.
 * @data   : Pointer to an object to store a reference of the stream's data.
 * @len    : Pointer to an object to store the amount of bytes in use.
 *
 * Gets the network stream's data.
 *
 */
void netstream_data(netstream_t *stream, void **data, size_t *len);

/**
 * netstream_eof:
 *
 * @stream : Pointer to a network stream object.
 *
 * Checks whether the network stream is at EOF or not.
 *
 * Returns: true if the stream is at EOF, false otherwise.
 */
bool netstream_eof(netstream_t *stream);

/**
 * netstream_tell:
 *
 * @stream : Pointer to a network stream object.
 *
 * Gets the network stream's current position.
 *
 * Returns: current value of the position indicator.
 */
size_t netstream_tell(netstream_t *stream);

/**
 * netstream_seek:
 *
 * @stream : Pointer to a network stream object.
 * @offset : Position's offset.
 * @origin : Position used as reference for the offset.
 *
 * Sets the network stream's current position.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_seek(netstream_t *stream, long offset, int origin);

/**
 * netstream_read:
 *
 * @stream : Pointer to a network stream object.
 * @data   : Pointer to a storage for data read from the network stream.
 * @len    : Amount of bytes to read. Pass 0 to read all remaining bytes.
 *
 * Reads raw data from the network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_read(netstream_t *stream, void *data, size_t len);

/**
 * netstream_read_(type):
 *
 * @stream : Pointer to a network stream object.
 * @data   : Pointer to a storage for data read from the network stream.
 *
 * Reads data from the network stream.
 * Network byte order is always big endian.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_read_byte(netstream_t   *stream, uint8_t  *data);
bool netstream_read_word(netstream_t   *stream, uint16_t *data);
bool netstream_read_dword(netstream_t  *stream, uint32_t *data);
bool netstream_read_qword(netstream_t  *stream, uint64_t *data);
#ifdef __STDC_IEC_559__
bool netstream_read_float(netstream_t  *stream, float    *data);
bool netstream_read_double(netstream_t *stream, double   *data);
#endif

/**
 * netstream_read_string:
 *
 * @stream : Pointer to a network stream object.
 * @s      : Pointer to a string buffer.
 * @len    : Size of the string buffer.
 *
 * Reads a string from the network stream.
 *
 * Returns: Length of the original string on success or
 * a negative value on error.
 */
int netstream_read_string(netstream_t *stream, char *s, size_t len);

/**
 * netstream_read_fixed_string:
 *
 * @stream : Pointer to a network stream object.
 * @s      : Pointer to a string buffer.
 * @len    : Size of the string buffer.
 *
 * Reads a fixed-length string from the network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_read_fixed_string(netstream_t *stream, char *s, size_t len);

/**
 * netstream_write:
 *
 * @stream : Pointer to a network stream object.
 * @data   : Data to write into the network stream.
 * @len    : Amount of bytes to write.
 *
 * Writes raw data into the network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_write(netstream_t *stream, const void *data, size_t len);

/**
 * netstream_write_(type):
 *
 * @stream : Pointer to a network stream object.
 * @data   : Data to write into the network stream.
 *
 * Writes data into the network stream.
 * Network byte order is always big endian.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_write_byte(netstream_t   *stream, uint8_t  data);
bool netstream_write_word(netstream_t   *stream, uint16_t data);
bool netstream_write_dword(netstream_t  *stream, uint32_t data);
bool netstream_write_qword(netstream_t  *stream, uint64_t data);
#ifdef __STDC_IEC_559__
bool netstream_write_float(netstream_t  *stream, float    data);
bool netstream_write_double(netstream_t *stream, double   data);
#endif

/**
 * netstream_write_string:
 *
 * @stream : Pointer to a network stream object.
 * @s      : Pointer to a string.
 *
 * Writes a null-terminated string into the network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_write_string(netstream_t *stream, const char *s);

/**
 * netstream_write_fixed_string:
 *
 * @stream : Pointer to a network stream object.
 * @s      : Pointer to a string.
 * @len    : Size of the string.
 *
 * Writes a null-terminated fixed-length string into the network stream.
 *
 * Returns: true on success, false otherwise.
 */
bool netstream_write_fixed_string(netstream_t *stream, const char *s,
      size_t len);

RETRO_END_DECLS

#endif
