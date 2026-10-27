/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_rsx.h)
 * ---------------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the
 * "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_RSX_H_
#define LIBRETRO_RSX_H_

#include "libretro.h"

#define RETRO_HW_RENDER_INTERFACE_RSX_VERSION 1

/* Linear A8R8G8B8 texture in RSX local memory. The core owns the allocation
 * until context_destroy; commands and presentation use the same FIFO. */
struct retro_rsx_image
{
   uint32_t offset;
   uint32_t pitch;
   uint32_t width;
   uint32_t height;
};

/* Query with GET_HW_RENDER_INTERFACE from context_reset, after requesting
 * RETRO_HW_CONTEXT_RSX through SET_HW_RENDER. Valid until context_destroy.
 * Both callers must submit commands on the main thread.
 * Labels 198 and 199 belong to the frontend. */
struct retro_hw_render_interface_rsx
{
   enum retro_hw_render_interface_type interface_type;
   unsigned interface_version;
   void *handle;
   void *context;
   /* The frontend owns this mapping; only [io_used, io_size) is available
    * to the core. Do not free it or replace its command-buffer callback. */
   void *io_address;
   uint32_t io_size;
   uint32_t io_used;
   /* Publish an image before video_refresh(RETRO_HW_FRAME_BUFFER_VALID).
    * NULL releases the cached image. No allocation or CPU wait is performed. */
   bool (*set_image)(void *handle, const struct retro_rsx_image *image);
   /* Transfers ownership of an rsxMemalign allocation to the frontend,
    * freed once the commands submitted before the next video_refresh have
    * finished. NULL is accepted. Returns false if the queue is full. */
   bool (*retire_memory)(void *handle, void *memory);
};

#endif /* LIBRETRO_RSX_H_ */
