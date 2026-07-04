/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image.h).
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

#ifndef __RARCH_IMAGE_CONTEXT_H
#define __RARCH_IMAGE_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

enum image_process_code
{
   IMAGE_PROCESS_ERROR     = -2,
   IMAGE_PROCESS_ERROR_END = -1,
   IMAGE_PROCESS_NEXT      =  0,
   IMAGE_PROCESS_END       =  1
};

struct texture_image
{
   uint32_t *pixels;
   unsigned width;
   unsigned height;
   bool supports_rgba;
};

enum image_type_enum
{
   IMAGE_TYPE_NONE = 0,
   IMAGE_TYPE_PNG,
   IMAGE_TYPE_JPEG,
   IMAGE_TYPE_BMP,
   IMAGE_TYPE_TGA,
   IMAGE_TYPE_WEBP,
   IMAGE_TYPE_DDS
};

enum image_type_enum image_texture_get_type(const char *path);

bool image_texture_load_buffer(struct texture_image *img,
   enum image_type_enum type, void *s, size_t len);

bool image_texture_load(struct texture_image *img, const char *path);
void image_texture_free(struct texture_image *img);

/* Image transfer */

void image_transfer_free(void *data, enum image_type_enum type);

void *image_transfer_new(enum image_type_enum type);

bool image_transfer_start(void *data, enum image_type_enum type);

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr,
      size_t len);

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf,
      size_t len,
      unsigned *width,
      unsigned *height,
      bool supports_rgba);

bool image_transfer_iterate(void *data, enum image_type_enum type);

bool image_transfer_is_valid(void *data, enum image_type_enum type);

/* Animation (animated images, currently WEBP only).
 *
 * image_transfer_anim_new returns an opaque animation handle, or NULL
 * for still images / unsupported types, so a caller can try it first and
 * fall back to the single-frame image_transfer_* path. Frames are fully
 * composited RGBA canvases (memory order R,G,B,A); the caller advances
 * frames on its own clock using each frame's duration in milliseconds. */

void *image_transfer_anim_new(void *buf, size_t len,
      enum image_type_enum type);

void image_transfer_anim_free(void *anim, enum image_type_enum type);

int image_transfer_anim_num_frames(void *anim, enum image_type_enum type);

void image_transfer_anim_get_info(void *anim, enum image_type_enum type,
      unsigned *width, unsigned *height, int *loop_count);

const uint32_t *image_transfer_anim_get_frame(void *anim,
      enum image_type_enum type, int index, int *duration_ms);

/* Streaming animation: memory use independent of frame count. The
 * buffer passed to image_transfer_anim_stream_new is BORROWED and must
 * outlive the stream. next() returns the stream's internal canvas
 * (valid until the next call, do not free); NULL means end of one
 * pass - rewind to loop. */

void *image_transfer_anim_stream_new(void *buf, size_t len,
      enum image_type_enum type);

void image_transfer_anim_stream_free(void *stream,
      enum image_type_enum type);

void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count);

const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms);

void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type);

RETRO_END_DECLS

#endif
