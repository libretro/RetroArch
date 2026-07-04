/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rdds.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RDDS_H__
#define __LIBRETRO_SDK_FORMAT_RDDS_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

struct image_gpu_layout;

typedef struct rdds rdds_t;

/* Decodes mip level 0 of a block-compressed (or uncompressed) DDS
 * image to a freshly malloc'd 32bpp buffer.  On success returns
 * IMAGE_PROCESS_END, sets *buf to the decoded pixels (caller frees)
 * and the width / height to the base dimensions.  On any error returns
 * IMAGE_PROCESS_ERROR and leaves *buf NULL.
 *
 * Output pixel order matches the rest of libretro-common's image
 * backends: with supports_rgba the uint32 is A<<24|B<<16|G<<8|R
 * (memory bytes R,G,B,A); otherwise A<<24|R<<16|G<<8|B (B,G,R,A). */
int rdds_process_image(rdds_t *rdds, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

/* GPU-native BCn mip layout for direct upload (no decode).  Returns
 * false for formats that must be CPU-decoded.  See rdds.c. */
bool rdds_get_gpu_layout(rdds_t *rdds, size_t len,
      struct image_gpu_layout *out);

bool rdds_set_buf_ptr(rdds_t *rdds, void *data);

void rdds_free(rdds_t *rdds);

rdds_t *rdds_alloc(void);

RETRO_END_DECLS

#endif
