/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stdin_stream.h).
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

#ifndef LIBRETRO_SDK_STDIN_STREAM_H__
#define LIBRETRO_SDK_STDIN_STREAM_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_miscellaneous.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Reads data from \c stdin if supported by the current platform.
 * @param buf[out] The buffer to read data into.
 * @param size The length of \c buf in bytes.
 * @return The number of bytes that were read,
 * or 0 if there was an error
 * (including a lack of platform support).
 * @note \c stdin is commonly used for text,
 * but this function can read binary data as well.
 * @see https://man7.org/linux/man-pages/man3/stdout.3.html
 */
size_t read_stdin(char *buf, size_t size);

RETRO_END_DECLS

#endif
