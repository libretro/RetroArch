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
   /**
    * Indicates that \c netstream_seek should seek
    * relative to the beginning of the stream.
    */
   NETSTREAM_SEEK_SET = 0,

   /**
    * Indicates that \c netstream_seek should seek
    * relative to its current position.
    */
   NETSTREAM_SEEK_CUR,

   /**
    * Indicates that \c netstream_seek should seek
    * relative to the end of the stream.
    */
   NETSTREAM_SEEK_END
};

/**
 * A stream that ensures data is read/written in network byte order (big endian).
 *
 * @note Despite what the name may suggests,
 * this object does \em not actually perform any network operations.
 * It is intended to be used as input/output for functions that do.
 */
typedef struct netstream
{
   /** The buffer used by the stream for reading or writing. */
   void   *buf;

   /** The size of \c buf in bytes. */
   size_t size;

   /** The number of bytes that have been written to \c buf. */
   size_t used;

   /**
    * The current position of the stream, in bytes.
    * @see netstream_seek
    */
   size_t pos;
} netstream_t;

/**
 * Opens a network-order stream.
 *
 * @param stream Pointer to the network-order stream to initialize.
 * Behavior is undefined if \c NULL.
 * @param buf Pre-allocated buffer.
 * May be \c NULL, in which case a new buffer will be created with \c malloc.
 * @param size Buffer size in bytes.
 * If \c buf is \c NULL, then this will be the size of the newly-allocated buffer.
 * If not, then this is the size of the existing buffer.
 * If zero, then a buffer will not be allocated.
 * @param used The number of bytes in use.
 * Ignored for non pre-allocated buffers.
 * @return \c true if the stream was opened.
 * For new buffers, \c false if allocation failed.
 * For existing buffers, \c false if \c size is zero
 * or less than \c used.
 * @see netstream_close
 */
bool netstream_open(netstream_t *stream, void *buf, size_t len, size_t used);

/**
 * Closes a network-order stream.
 *
 * @param stream Pointer to the network-order stream to close.
 * The stream itself is not deallocated,
 * but its fields will be reset.
 * Behavior is undefined if \c NULL.
 * @param dealloc Whether to release the underlying buffer with \c free.
 * Set to \c true if the creating \c netstream_open call allocated a buffer,
 * or else its memory will be leaked.
 * @note \c stream itself is not deallocated.
 * This function can be used on static or local variables.
 * @see netstream_open
 */
void netstream_close(netstream_t *stream, bool dealloc);

/**
 * Resets the stream to the beginning and discards any used bytes.
 *
 * @param stream The network-order stream to reset.
 * Behavior is undefined if \c NULL.
 *
 * @note This does not deallocate the underlying buffer,
 * nor does it wipe its memory.
 */
void netstream_reset(netstream_t *stream);

/**
 * Resizes the "used" portion of the stream.
 *
 * @param stream The network-order stream to resize.
 * Behavior is undefined if \c NULL.
 * @param used The number of bytes in the stream that are considered written.
 * @return \c true if the stream's "used" region was resized,
 * \c false if it would've been extended past the buffer's capacity.
 * @note This function does not reallocate or clear the underlying buffer.
 * It only sets the boundaries of the "used" portion of the stream.
 */
bool netstream_truncate(netstream_t *stream, size_t used);

/**
 * Retrieves the network-order stream's data.
 *
 * @param stream Pointer to the network-order stream.
 * Behavior is undefined if \c NULL.
 * @param data[out] Pointer to a variable to store the stream's data.
 * The data itself is not copied,
 * so the pointer will be invalidated
 * if the stream is closed or reallocated.
 * @param len[out] Set to the length of the stream's data in bytes.
 */
void netstream_data(netstream_t *stream, void **data, size_t *len);

/**
 * Checks whether the network-order stream has any more data to read,
 * or any more room to write data.
 *
 * @param stream The network-order stream to check.
 * @return \c true if the stream is at EOF, \c false otherwise.
 */
bool netstream_eof(netstream_t *stream);

/**
 * Gets the network-order stream's current position.
 *
 * @param stream Pointer to a network-order stream.
 * @return The stream's position indicator.
 */
size_t netstream_tell(netstream_t *stream);

/**
 * Sets the network-order stream's current position.
 *
 * @param stream Pointer to a network-order stream.
 * @param offset The new position of the stream, in bytes.
 * @param origin The position used as reference for the offset.
 * Must be one of \c NETSTREAM_SEEK_SET, \c NETSTREAM_SEEK_CUR or \c NETSTREAM_SEEK_END.
 * @return \c true on success, \c false on failure.
 */
bool netstream_seek(netstream_t *stream, long offset, int origin);

/**
 * Reads data from the network-order stream.
 * Does not byte-swap any data;
 * this is useful for reading strings or unspecified binary data.
 *
 * @param stream The network-order stream to read from.
 * @param data The buffer that will receive data from the stream.
 * @param len The number of bytes to read.
 * If 0, will read all remaining bytes.
 * @return \c true on success, \c false on failure.
 */
bool netstream_read(netstream_t *stream, void *data, size_t len);

/**
 * Reads a single byte from the network-order stream.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the byte that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_byte(netstream_t   *stream, uint8_t  *data);

/**
 * Reads an unsigned 16-bit integer from the network-order stream,
 * byte-swapping it from big endian to host-native byte order if necessary.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the value that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_word(netstream_t   *stream, uint16_t *data);

/**
 * Reads an unsigned 32-bit integer from the network-order stream,
 * byte-swapping it from big endian to host-native byte order if necessary.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the value that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_dword(netstream_t  *stream, uint32_t *data);

/**
 * Reads an unsigned 64-bit integer from the network-order stream,
 * byte-swapping it from big endian to host-native byte order if necessary.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the value that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_qword(netstream_t  *stream, uint64_t *data);
#ifdef __STDC_IEC_559__
/**
 * Reads a 32-bit floating-point number from the network-order stream,
 * byte-swapping it from big endian to host-native byte order if necessary.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the value that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_float(netstream_t  *stream, float    *data);

/**
 * Reads a 64-bit floating-point number from the network-order stream,
 * byte-swapping it from big endian to host-native byte order if necessary.
 *
 * @param stream[in] The network-order stream to read from.
 * @param data[out] Pointer to the value that will receive the read value.
 * @return \c true on success, \c false if there was an error.
 */
bool netstream_read_double(netstream_t *stream, double   *data);
#endif

/**
 * Reads a null-terminated string from the network-order stream,
 * up to the given length.
 *
 * @param stream Pointer to a network stream object.
 * @param s[out] The buffer that will receive the string.
 * Will be \c NULL-terminated.
 * @param len The length of \c s, in bytes.
 * @return The length of the read string in bytes,
 * or -1 if there was an error.
 */
int netstream_read_string(netstream_t *stream, char *s, size_t len);

/**
 * Reads a string of fixed length from a network-order stream.
 * Will fail if there isn't enough data to read.
 *
 * @param stream Pointer to a network stream object.
 * @param s The buffer that will receive the string.
 * Will be \c NULL-terminated.
 * @param len The length of \c s in bytes,
 * including the \c NULL terminator.
 *
 * @return \c true if a string of the exact length was read,
 * \c false if there was an error.
 */
bool netstream_read_fixed_string(netstream_t *stream, char *s, size_t len);

/**
 * Writes arbitrary data to a network-order stream.
 * Does not byte-swap any data;
 * this is useful for writing strings or unspecified binary data.
 *
 * @param stream Pointer to a network stream object.
 * @param data The data to write into the network stream.
 * @param len The length of \c data, in bytes.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write(netstream_t *stream, const void *data, size_t len);

/**
 * Writes a single byte to a network-order stream.
 *
 * @param stream Pointer to a network stream object.
 * @param data The byte to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_byte(netstream_t   *stream, uint8_t  data);

/**
 * Writes an unsigned 16-bit integer to a network-order stream,
 * byte-swapping it from host-native byte order to big-endian if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param data The value to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_word(netstream_t   *stream, uint16_t data);

/**
 * Writes an unsigned 32-bit integer to a network-order stream,
 * byte-swapping it from host-native byte order to big-endian if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param data The value to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_dword(netstream_t  *stream, uint32_t data);

/**
 * Writes an unsigned 64-bit integer to a network-order stream,
 * byte-swapping it from host-native byte order to big-endian if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param data The value to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_qword(netstream_t  *stream, uint64_t data);
#ifdef __STDC_IEC_559__

/**
 * Writes a 32-bit floating-point number to a network-order stream,
 * byte-swapping it from host-native byte order to big-endian if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param data The value to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_float(netstream_t  *stream, float    data);

/**
 * Writes a 64-bit floating-point number to a network-order stream,
 * byte-swapping it from host-native byte order to big-endian if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param data The value to write to the stream.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_double(netstream_t *stream, double   data);
#endif

/**
 * Writes a null-terminated string to a network-order stream.
 * Does not byte-swap any data.
 *
 * @param stream Pointer to a network stream object.
 * @param s A \c NULL-terminated string.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_string(netstream_t *stream, const char *s);

/**
 * Writes a string of fixed length to a network-order stream,
 * \c NULL-terminating it if necessary.
 *
 * @param stream Pointer to a network stream object.
 * @param s Pointer to a string.
 * Does not need to be \c NULL-terminated,
 * but \c NULL values will not stop processing.
 * Will be \c NULL
 * @param len Length of \c s in bytes,
 * including the \c NULL terminator.
 * Exactly this many bytes will be written to the stream;
 * the last character will be set to \c NULL.
 * @return \c true on success,
 * \c false if there was an error.
 */
bool netstream_write_fixed_string(netstream_t *stream, const char *s,
      size_t len);

RETRO_END_DECLS

#endif
