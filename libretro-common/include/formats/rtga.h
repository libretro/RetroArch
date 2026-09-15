/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtga.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RTGA_H__
#define __LIBRETRO_SDK_FORMAT_RTGA_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rtga rtga_t;

/**
 * Raises the resident-byte frontier for a decode running against a
 * partially filled buffer. Bytes at or past @avail are not read; a
 * slice that reaches the frontier with more of the file still to
 * arrive returns \c IMAGE_PROCESS_WAIT and sets rtga_need_more()
 * rather than treating the wall as EOF. The frontier only ever moves
 * forward. Never calling this decodes the whole buffer exactly as
 * before.
 */
/** True when @len bytes from the head of a TGA are enough for
 * rtga_process_image() to parse the header, id field and colour map
 * and begin painting pixels, i.e. when a partial-buffer decode can
 * usefully start. False for a type this decoder does not slice, so a
 * caller falls back to loading the file whole. */
bool rtga_header_ready(const uint8_t *data, size_t len);

void rtga_set_avail(rtga_t *rtga, size_t avail);

/** True when the last rtga_process_image() stopped at the frontier
 * set by rtga_set_avail() rather than finishing. */
bool rtga_need_more(rtga_t *rtga);

int rtga_process_image(rtga_t *rtga, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rtga_set_buf_ptr(rtga_t *rtga, void *data);

void rtga_free(rtga_t *rtga);

rtga_t *rtga_alloc(void);

RETRO_END_DECLS

#endif
