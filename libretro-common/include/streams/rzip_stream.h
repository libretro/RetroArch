/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_stream.h).
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

#ifndef _LIBRETRO_SDK_FILE_RZIP_STREAM_H
#define _LIBRETRO_SDK_FILE_RZIP_STREAM_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Rudimentary interface for streaming data to/from a
 * zlib-compressed chunk-based RZIP archive file.
 * 
 * This is somewhat less efficient than using regular
 * gzip code, but this is by design - the intention here
 * is to create an interface that integrates seamlessly
 * with normal RetroArch functionality, using only
 * standard/existing libretro-common routines.
 * (Actual efficiency is pretty good, regardless:
 * archived file size is almost identical to a solid
 * zip file, and compression/decompression speed is
 * not substantially worse than external archiving tools;
 * it is certainly acceptable for use in real-time
 * frontend applications)
 * 
 * When reading existing files, uncompressed content
 * is handled automatically. File type (compressed/
 * uncompressed) is detected via the RZIP header.
 * 
 * ## RZIP file format:
 * 
 * <file id header>:                8 bytes
 *                                  - [#][R][Z][I][P][v][file format version][#]
 * <uncompressed chunk size>:       4 bytes, little endian order
 *                                  - nominal (maximum) size of each uncompressed
 *                                    chunk, in bytes
 * <total uncompressed data size>:  8 bytes, little endian order
 * <size of next compressed chunk>: 4 bytes, little endian order
 *                                  - size on-disk of next compressed data
 *                                    chunk, in bytes
 * <next compressed chunk>:         n bytes of zlib compressed data
 * ...
 * <size of next compressed chunk> : repeated until end of file
 * <next compressed chunk>         :
 * 
 */

/* Prevent direct access to rzipstream_t members */
typedef struct rzipstream rzipstream_t;

/* File Open */

/* Opens a new or existing RZIP file
 * > Supported 'mode' values are:
 *   - RETRO_VFS_FILE_ACCESS_READ
 *   - RETRO_VFS_FILE_ACCESS_WRITE
 * > When reading, 'path' may reference compressed
 *   or uncompressed data
 * Returns NULL if arguments are invalid, file
 * is invalid or an IO error occurs */
rzipstream_t* rzipstream_open(const char *path, unsigned mode);

/* File Read */

/* Reads (a maximum of) 'len' bytes from an RZIP file.
 * Returns actual number of bytes read, or -1 in
 * the event of an error */
int64_t rzipstream_read(rzipstream_t *stream, void *data, int64_t len);

/* File Write */

/* Writes 'len' bytes to an RZIP file.
 * Returns actual number of bytes written, or -1
 * in the event of an error */
int64_t rzipstream_write(rzipstream_t *stream, const void *data, int64_t len);

/* File Status */

/* Returns total size (in bytes) of the *uncompressed*
 * data in an RZIP file.
 * (If reading an uncompressed file, this corresponds
 * to the 'physical' file size in bytes)
 * Returns -1 in the event of a error. */
int64_t rzipstream_get_size(rzipstream_t *stream);

/* Returns EOF when no further *uncompressed* data
 * can be read from an RZIP file. */
int rzipstream_eof(rzipstream_t *stream);

/* File Close */

/* Closes RZIP file. If file is open for writing,
 * flushes any remaining buffered data to disk.
 * Returns -1 in the event of a error. */
int rzipstream_close(rzipstream_t *stream);

RETRO_END_DECLS

#endif
