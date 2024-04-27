/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_stream.h).
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

#ifndef __LIBRETRO_SDK_FILE_STREAM_H
#define __LIBRETRO_SDK_FILE_STREAM_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <sys/types.h>

#include <libretro.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include <stdarg.h>
#include <vfs/vfs_implementation.h>

/** @defgroup file_stream File Streams
 *
 * All functions in this header will use the VFS interface set in \ref filestream_vfs_init if possible,
 * or else they will fall back to built-in equivalents.
 *
 * @note These functions are modeled after those in the C standard library
 * (and may even use them internally),
 * but identical behavior is not guaranteed.
 *
 * @{
 */

/**
 * The minimum version of the VFS interface required by the \c filestream functions.
 */
#define FILESTREAM_REQUIRED_VFS_VERSION 2

RETRO_BEGIN_DECLS

/**
 * Opaque handle to a file stream.
 * @warning This is not interchangeable with \c FILE* or \c retro_vfs_file_handle.
 */
typedef struct RFILE RFILE;

#define FILESTREAM_REQUIRED_VFS_VERSION 2

/**
 * Initializes the \c filestream functions to use the VFS interface provided by the frontend.
 * Optional; if not called, all \c filestream functions
 * will use libretro-common's built-in implementations.
 *
 * @param vfs_info The VFS interface returned by the frontend.
 * If \c vfs_info::iface (but \em not \c vfs_info itself) is \c NULL,
 * then libretro-common's built-in VFS implementation will be used.
 */
void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info);

/**
 * Returns the size of the given file, in bytes.
 *
 * @param stream The open file to query.
 * @return The size of \c stream in bytes,
 * or -1 if there was an error.
 * @see retro_vfs_size_t
 */
int64_t filestream_get_size(RFILE *stream);

/**
 * Sets the size of the given file,
 * truncating or extending it as necessary.
 *
 * @param stream The file to resize.
 * @param length The new size of \c stream, in bytes.
 * @return 0 if the resize was successful,
 * or -1 if there was an error.
 * @see retro_vfs_truncate_t
 */
int64_t filestream_truncate(RFILE *stream, int64_t length);

/**
 * Opens a file for reading or writing.
 *
 * @param path Path to the file to open.
 * Should be in the format described in \ref GET_VFS_INTERFACE.
 * @param mode The mode to open the file in.
 * Should be one or more of the flags in \refitem RETRO_VFS_FILE_ACCESS OR'd together,
 * and \c RETRO_VFS_FILE_ACCESS_READ or \c RETRO_VFS_FILE_ACCESS_WRITE
 * (or both) must be included.
 * @param hints Optional hints to pass to the frontend.
 *
 * @return The opened file, or \c NULL if there was an error.
 * Must be cleaned up with \c filestream_close when no longer needed.
 * @see retro_vfs_open_t
 */
RFILE* filestream_open(const char *path, unsigned mode, unsigned hints);

/**
 * Sets the current position of the file stream.
 * Use this to read specific sections of a file.
 *
 * @param stream The file to set the stream position of.
 * @param offset The new stream position, in bytes.
 * @param seek_position The position to seek from.
 * Should be one of the values in \refitem RETRO_VFS_SEEK_POSITION.
 * @return The new stream position in bytes relative to the beginning,
 * or -1 if there was an error.
 * @see RETRO_VFS_SEEK_POSITION
 * @see retro_vfs_seek_t
 */
int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position);

/**
 * Reads data from the given file into a buffer.
 * If the read is successful,
 * the file's stream position will advance by the number of bytes read.
 *
 * @param stream The file to read from.
 * @param data The buffer in which to store the read data.
 * @param len The size of \c data, in bytes.
 * @return The number of bytes read,
 * or -1 if there was an error.
 * May be less than \c len, but never more.
 * @see retro_vfs_read_t
 */
int64_t filestream_read(RFILE *stream, void *data, int64_t len);

/**
 * Writes data from a buffer to the given file.
 * If the write is successful,
 * the file's stream position will advance by the number of bytes written.
 *
 * @param stream The file to write to.
 * @param data The buffer containing the data to write.
 * @param len The size of \c data, in bytes.
 * @return The number of bytes written,
 * or -1 if there was an error.
 * May be less than \c len, but never more.
 * @see retro_vfs_write_t
 */
int64_t filestream_write(RFILE *stream, const void *data, int64_t len);

/**
 * Returns the current position of the given file in bytes.
 *
 * @param stream The file to return the stream position for.
 * @return The current stream position in bytes relative to the beginning,
 * or -1 if there was an error.
 * @see retro_vfs_tell_t
 */
int64_t filestream_tell(RFILE *stream);

/**
 * Rewinds the given file to the beginning.
 * Equivalent to <tt>filestream_seek(stream, 0, RETRO_VFS_SEEK_POSITION_START)</tt>.

 * @param stream The file to rewind.
 * May be \c NULL, in which case this function does nothing.
 */
void filestream_rewind(RFILE *stream);

/**
 * Closes the given file.
 *
 * @param stream The file to close.
 * This should have been created with \c filestream_open.
 * Behavior is undefined if \c NULL.
 * @return 0 if the file was closed successfully,
 * or -1 if there was an error.
 * @post \c stream is no longer valid and should not be used,
 * even if this function fails.
 * @see retro_vfs_close_t
 */
int filestream_close(RFILE *stream);

/**
 * Opens a file, reads its contents into a newly-allocated buffer,
 * then closes it.
 *
 * @param path[in] Path to the file to read.
 * Should be in the format described in \ref GET_VFS_INTERFACE.
 * @param buf[out] A pointer to the address of the newly-allocated buffer.
 * The buffer will contain the entirety of the file at \c path.
 * Will be allocated with \c malloc and must be freed with \c free.
 * @param len[out] Pointer to the size of the buffer in bytes.
 * May be \c NULL, in which case the length is not written.
 * Value is unspecified if this function fails.
 * @return 1 if the file was read successfully,
 * 0 if there was an error.
 * @see filestream_write_file
 */
int64_t filestream_read_file(const char *path, void **buf, int64_t *len);

/**
 * Reads a line of text from the given file,
 * up to a given length.
 *
 * Will read to the next newline or until the buffer is full,
 * whichever comes first.
 *
 * @param stream The file to read from.
 * @param s The buffer to write the retrieved line to.
 * Will contain at most \c len - 1 characters
 * plus a null terminator.
 * The newline character (if any) will not be included.
 * The line delimiter must be Unix-style (\c '\n').
 * Carriage returns (\c '\r') will not be treated specially.
 * @param len The length of the buffer \c s, in bytes.
 * @return \s if successful, \c NULL if there was an error.
 */
char* filestream_gets(RFILE *stream, char *s, size_t len);

/**
 * Reads a single character from the given file.
 *
 * @param stream The file to read from.
 * @return The character read, or -1 upon reaching the end of the file.
 */
int filestream_getc(RFILE *stream);

/**
 * Reads formatted text from the given file,
 * with arguments provided as a standard \c va_list.
 *
 * @param stream The file to read from.
 * @param format The string to write, possibly including scanf-compatible format specifiers.
 * @param args Argument list with zero or more elements
 * whose values will be updated according to the semantics of \c format.
 * @return The number of arguments in \c args that were successfully assigned,
 * or -1 if there was an error.
 * @see https://en.cppreference.com/w/c/io/fscanf
 * @see https://en.cppreference.com/w/c/variadic
 */
int filestream_vscanf(RFILE *stream, const char* format, va_list *args);

/**
 * Reads formatted text from the given file.
 *
 * @param stream The file to read from.
 * @param format The string to write, possibly including scanf-compatible format specifiers.
 * @param ... Zero or more arguments that will be updated according to the semantics of \c format.
 * @return The number of arguments in \c ... that were successfully assigned,
 * or -1 if there was an error.
 * @see https://en.cppreference.com/w/c/io/fscanf
 */
int filestream_scanf(RFILE *stream, const char* format, ...);

/**
 * Determines if there's any more data left to read from this file.
 *
 * @param stream The file to check the position of.
 * @return -1 if this stream has reached the end of the file,
 * 0 if not.
 */
int filestream_eof(RFILE *stream);

/**
 * Writes the entirety of a given buffer to a file at a given path.
 * Any file that already exists will be overwritten.
 *
 * @param path Path to the file that will be written to.
 * @param data The buffer to write to \c path.
 * @param size The size of \c data, in bytes.
 * @return \c true if the file was written successfully,
 * \c false if there was an error.
 */
bool filestream_write_file(const char *path, const void *data, int64_t size);

/**
 * Writes a single character to the given file.
 *
 * @param stream The file to write to.
 * @param c The character to write.
 * @return The character written,
 * or -1 if there was an error.
 * Will return -1 if \c stream is \c NULL.
 */
int filestream_putc(RFILE *stream, int c);

/**
 * Writes formatted text to the given file,
 * with arguments provided as a standard \c va_list.
 *
 * @param stream The file to write to.
 * @param format The string to write, possibly including printf-compatible format specifiers.
 * @param args A list of arguments to be formatted and inserted in the resulting string.
 * @return The number of characters written,
 * or -1 if there was an error.
 * @see https://en.cppreference.com/w/c/io/vfprintf
 * @see https://en.cppreference.com/w/c/variadic
 */
int filestream_vprintf(RFILE *stream, const char* format, va_list args);

/**
 * Writes formatted text to the given file.
 *
 * @param stream The file to write to.
 * @param format The string to write, possibly including printf-compatible format specifiers.
 * @param ... Zero or more arguments to be formatted and inserted into the resulting string.
 * @return The number of characters written,
 * or -1 if there was an error.
 * @see https://en.cppreference.com/w/c/io/printf
 */
int filestream_printf(RFILE *stream, const char* format, ...);

/**
 * Checks if there was an error in using the given file stream.
 *
 * @param stream The file stream to check for errors.
 * @return \c true if there was an error in using this stream,
 * \c false if not or if \c stream is \c NULL.
 */
int filestream_error(RFILE *stream);

/**
 * Flushes pending writes to the operating system's file layer.
 * There is no guarantee that pending writes will be written to disk immediately.
 *
 * @param stream The file to flush.
 * @return 0 if the flush was successful,
 * or -1 if there was an error.
 * @see retro_vfs_flush_t
 */
int filestream_flush(RFILE *stream);

/**
 * Deletes the file at the given path.
 * If the file is open by any process,
 * the behavior is platform-specific.
 *
 * @note This function might or might not delete directories recursively,
 * depending on the platform and the underlying VFS implementation.
 *
 * @param path The file to delete.
 * @return 0 if the file was deleted successfully,
 * or -1 if there was an error.
 * @see retro_vfs_remove_t
 */
int filestream_delete(const char *path);

/**
 * Moves a file to a new location, with a new name.
 *
 * @param old_path Path to the file to rename.
 * @param new_path The target name and location of the file.
 * @return 0 if the file was renamed successfully,
 * or -1 if there was an error.
 * @see retro_vfs_rename_t
 */
int filestream_rename(const char *old_path, const char *new_path);

/**
 * Get the path that was used to open a file.
 *
 * @param stream The file to get the path of.
 * @return The path that was used to open \c stream,
 * or \c NULL if there was an error.
 * The string is owned by \c stream and must not be modified or freed by the caller.
 */
const char* filestream_get_path(RFILE *stream);

/**
 * Determines if a file exists at the given path.
 *
 * @param path The path to check for existence.
 * @return \c true if a file exists at \c path,
 * \c false if not or if \c path is \c NULL or empty.
 */
bool filestream_exists(const char *path);

/**
 * Reads a line from the given file into a newly-allocated buffer.
 *
 * @param stream The file to read from.
 * @return Pointer to the line read from \c stream,
 * or \c NULL if there was an error.
 * Must be freed with \c free when no longer needed.
 */
char* filestream_getline(RFILE *stream);

/**
 * Returns the open file handle
 * that was originally returned by the VFS interface.
 *
 * @param stream File handle returned by \c filestream_open.
 * @return The file handle returned by the underlying VFS implementation.
 */
libretro_vfs_implementation_file* filestream_get_vfs_handle(RFILE *stream);

RETRO_END_DECLS

/** @} */

#endif
