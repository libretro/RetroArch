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
#include <stdarg.h>

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

/* Reads next character from an RZIP file.
 * Returns character value, or EOF if no data
 * remains.
 * Note: Always returns EOF if file is open
 * for writing. */
int rzipstream_getc(rzipstream_t *stream);

/* Reads one line from an RZIP file and stores it
 * in the character array pointed to by 's'.
 * It stops reading when either (len-1) characters
 * are read, the newline character is read, or the
 * end-of-file is reached, whichever comes first.
 * On success, returns 's'. In the event of an error,
 * or if end-of-file is reached and no characters
 * have been read, returns NULL. */
char* rzipstream_gets(rzipstream_t *stream, char *s, size_t len);

/* Reads all data from file specified by 'path' and
 * copies it to 'buf'.
 * - 'buf' will be allocated and must be free()'d manually.
 * - Allocated 'buf' size is equal to 'len'.
 * Returns false in the event of an error */
bool rzipstream_read_file(const char *path, void **buf, int64_t *len);

/* File Write */

/* Writes 'len' bytes to an RZIP file.
 * Returns actual number of bytes written, or -1
 * in the event of an error */
int64_t rzipstream_write(rzipstream_t *stream, const void *data, int64_t len);

/* Writes a single character to an RZIP file.
 * Returns character written, or EOF in the event
 * of an error */
int rzipstream_putc(rzipstream_t *stream, int c);

/* Writes a variable argument list to an RZIP file.
 * Ugly 'internal' function, required to enable
 * 'printf' support in the higher level 'interface_stream'.
 * Returns actual number of bytes written, or -1
 * in the event of an error */
int rzipstream_vprintf(rzipstream_t *stream, const char* format, va_list args);

/* Writes formatted output to an RZIP file.
 * Returns actual number of bytes written, or -1
 * in the event of an error */
int rzipstream_printf(rzipstream_t *stream, const char* format, ...);

/* Writes contents of 'data' buffer to file
 * specified by 'path'.
 * Returns false in the event of an error */
bool rzipstream_write_file(const char *path, const void *data, int64_t len);

/* File Control */

/* Sets file position to the beginning of the
 * specified RZIP file.
 * Note: It is not recommended to rewind a file
 * that is open for writing, since the caller
 * may end up with a file containing junk data
 * at the end (harmless, but a waste of space). */
void rzipstream_rewind(rzipstream_t *stream);

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

/* Returns the offset of the current byte of *uncompressed*
 * data relative to the beginning of an RZIP file.
 * Returns -1 in the event of a error. */
int64_t rzipstream_tell(rzipstream_t *stream);

/* Returns true if specified RZIP file contains
 * compressed content */
bool rzipstream_is_compressed(rzipstream_t *stream);

/* File Close */

/* Closes RZIP file. If file is open for writing,
 * flushes any remaining buffered data to disk.
 * Returns -1 in the event of a error. */
int rzipstream_close(rzipstream_t *stream);

RETRO_END_DECLS

#endif
