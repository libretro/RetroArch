/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture_file.h).
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


#ifndef __LIBRETRO_SDK_STREAMS_IMAGE_TEXTURE_FILE_H__
#define __LIBRETRO_SDK_STREAMS_IMAGE_TEXTURE_FILE_H__

#include <retro_common_api.h>
#include <boolean.h>
#include <formats/image.h>

RETRO_BEGIN_DECLS

/* Images loaded by path: the file read whole through
 * data_transfer_open_prefix - filestream/VFS routing, so overlays and
 * driver assets from archive members or content:// documents load like
 * any other, 64-bit lengths, honest short reads - and decoded by
 * image_texture_load_buffer_probe. */

/* ->pix10 is an ask on the way in and an answer on the way out: set
 * it before the call to have a decoder that can emit XRGB2101010 do
 * so, and read it after to find out whether it did. Clear on entry
 * means the ordinary 8-bit image, as before. */
bool image_texture_load(struct texture_image *img, const char *path);

/* image_texture_load with an abort hook: both decode stages are
 * incremental (one chunk / one pass per step) and @should_abort is
 * asked between steps; returning true abandons the decode, which then
 * fails cleanly. For decodes on worker threads that must stop promptly
 * at shutdown or when their result is no longer wanted. @should_abort
 * may be NULL. */
bool image_texture_load_ex(struct texture_image *img, const char *path,
      bool (*should_abort)(void *ud), void *ud);

bool image_texture_load_request(struct texture_image *img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud);

/* image_texture_load_request answering, from the bytes it read, whether
 * the file is an animated PNG: 1 yes, 0 a still, -1 not a PNG or no
 * decode. Spares a caller that keeps an animation's bytes a second
 * read of every still to ask. @png_probe may be NULL. */
bool image_texture_load_request_ex(struct texture_image *img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud, int *png_probe);

RETRO_END_DECLS

#endif
