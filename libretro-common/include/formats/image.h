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

struct texture_compressed;  /* defined below, after enum image_type_enum */

struct texture_image
{
   uint32_t *pixels;
   unsigned width;
   unsigned height;
   bool supports_rgba;
   /* Optional GPU-native compressed payload (BCn).  When non-NULL a
    * capable driver may upload it directly and leave ->pixels NULL;
    * image_texture_realize_rgba() decodes to ->pixels on demand for
    * drivers that cannot sample the format. */
   struct texture_compressed *compressed;
};

enum image_type_enum
{
   IMAGE_TYPE_NONE = 0,
   IMAGE_TYPE_PNG,
   IMAGE_TYPE_JPEG,
   IMAGE_TYPE_BMP,
   IMAGE_TYPE_TGA,
   IMAGE_TYPE_WEBP,
   IMAGE_TYPE_DDS,
   IMAGE_TYPE_WEBM
};

#define IMAGE_MAX_MIPS 16

/* Several MIPS toolchains (RS90/Dingux, PSP, PS2, ...) predefine the bare
 * macro `mips` as 1 in GNU mode, which collides with the `mips` field of
 * struct texture_compressed below (and every `->mips` access downstream).
 * RA never uses the legacy macro (it uses __mips__), so drop it here. */
#ifdef mips
#undef mips
#endif

/* GPU-native compressed texture formats (block-compressed).  Producers
 * (currently the DDS loader) report one of these when a texture can be
 * uploaded to the GPU as-is; capable drivers skip the CPU decode to
 * RGBA8 entirely.  Extend with ETC2 or ASTC entries later. */
enum texture_gpu_format
{
   TEXTURE_GPU_FORMAT_NONE = 0,
   TEXTURE_GPU_FORMAT_BC1,       /* DXT1               */
   TEXTURE_GPU_FORMAT_BC2,       /* DXT3               */
   TEXTURE_GPU_FORMAT_BC3,       /* DXT5               */
   TEXTURE_GPU_FORMAT_BC4,       /* RGTC1 (1 channel)  */
   TEXTURE_GPU_FORMAT_BC5,       /* RGTC2 (2 channel)  */
   TEXTURE_GPU_FORMAT_BC6H_UF,   /* BPTC unsigned HDR  */
   TEXTURE_GPU_FORMAT_BC6H_SF,   /* BPTC signed HDR    */
   TEXTURE_GPU_FORMAT_BC7        /* BPTC LDR           */
};

/* Numeric mip layout reported by a loader without decoding.  Offsets are
 * byte offsets from the start of the source file buffer. */
struct image_gpu_layout
{
   enum texture_gpu_format format;
   unsigned                num_mips;
   unsigned                width[IMAGE_MAX_MIPS];
   unsigned                height[IMAGE_MAX_MIPS];
   size_t                  offset[IMAGE_MAX_MIPS];
   size_t                  size[IMAGE_MAX_MIPS];
};

struct texture_mip
{
   const void *data;
   unsigned    width;
   unsigned    height;
   size_t      size;
};

/* Owned, self-contained compressed payload carried on a texture_image.
 * storage holds a private copy of the source file (so the mip pointers
 * outlive the loader's buffer) and doubles as the input for the CPU
 * decode fallback in image_texture_realize_rgba(). */
struct texture_compressed
{
   void                    *storage;
   size_t                   storage_len;
   struct texture_mip      *mips;
   unsigned                 num_mips;
   enum texture_gpu_format  format;
   enum image_type_enum     type;      /* for the CPU-decode fallback */
};

enum image_type_enum image_texture_get_type(const char *path);

bool image_texture_load_buffer(struct texture_image *img,
   enum image_type_enum type, void *s, size_t len);

bool image_texture_load(struct texture_image *img, const char *path);
void image_texture_free(struct texture_image *img);

/* Force a CPU decode of a compressed texture_image into ->pixels (RGBA8).
 * No-op returning true if ->pixels is already present.  The fallback when
 * a driver cannot sample img->compressed->format. */
bool image_texture_realize_rgba(struct texture_image *img);

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

/* Report the GPU-native compressed mip layout of an already-started
 * transfer without decoding.  Returns false for formats that must be
 * CPU-decoded (non-DDS, uncompressed DDS, premultiplied, BC4/5/6H). */
bool image_transfer_get_gpu_layout(
      void *data,
      enum image_type_enum type,
      size_t len,
      struct image_gpu_layout *out);

bool image_transfer_iterate(void *data, enum image_type_enum type);

bool image_transfer_is_valid(void *data, enum image_type_enum type);

/* Animation (animated images: WEBP, and the video track of WEBM).
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
