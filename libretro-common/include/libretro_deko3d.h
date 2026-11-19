/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_deko3d.h)
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

#ifndef LIBRETRO_DEKO3D_H__
#define LIBRETRO_DEKO3D_H__

#include "libretro.h"
#include <deko3d.h>

#define RETRO_HW_RENDER_INTERFACE_DEKO3D_VERSION 1

/* The image a core hands the frontend for one frame.
 *
 * `image` must be created with DkImageFlags_Usage2DEngine on the device
 * in retro_hw_render_interface_deko3d. The frontend copies the DkImage
 * struct inside set_image; the struct itself need only live for that
 * call. The memory behind it must stay valid and unwritten until
 * wait_sync_index() for the sync index the image was set under has
 * returned, or until the frontend has been given another image for that
 * index and that index's wait has returned.
 *
 * (x, y, width, height) is the visible rectangle inside the image.
 * image_width and image_height are the full extent of `image`; both
 * are required. */
struct retro_deko3d_image
{
   const DkImage *image;
   unsigned x;
   unsigned y;
   unsigned width;
   unsigned height;
   unsigned image_width;
   unsigned image_height;
};

/* Sets the image for the current sync index, before
 * video_refresh(RETRO_HW_FRAME_BUFFER_VALID). NULL withdraws the image:
 * the frontend draws nothing of the core's until it is given another.
 *
 * acquire_fence, if not NULL, is copied inside the call and waited on
 * the frontend's queue before the image is read. It must already have
 * been signalled by a processed dkQueueSubmitCommands() or
 * dkQueueSignalFence() on the core's queue: a fence still only recorded
 * in an unsubmitted command list is empty, and is not waited. */
typedef void (*retro_deko3d_set_image_t)(void *handle,
      const struct retro_deko3d_image *image,
      const DkFence *acquire_fence);

/* Bit N of get_sync_index_mask() set means N is a valid index. The
 * frontend has waited the current index before retro_run, so a core
 * keeping per-index resources may write that index's resources without
 * waiting. */
typedef uint32_t (*retro_deko3d_get_sync_index_t)(void *handle);
typedef uint32_t (*retro_deko3d_get_sync_index_mask_t)(void *handle);

/* Blocks until the frontend is done with everything set under the
 * current sync index. For teardown and resource destruction. */
typedef void (*retro_deko3d_wait_sync_index_t)(void *handle);

/* Query with GET_HW_RENDER_INTERFACE from context_reset after requesting
 * RETRO_HW_CONTEXT_DEKO3D. Valid until context_destroy. The core
 * creates its own DkQueue(s) on `device`; the frontend's queue is not
 * shared. All calls are made from the thread that calls retro_run. */
struct retro_hw_render_interface_deko3d
{
   enum retro_hw_render_interface_type interface_type;
   unsigned interface_version;
   void *handle;
   DkDevice device;
   retro_deko3d_set_image_t           set_image;
   retro_deko3d_get_sync_index_t      get_sync_index;
   retro_deko3d_get_sync_index_mask_t get_sync_index_mask;
   retro_deko3d_wait_sync_index_t     wait_sync_index;
};

#endif
