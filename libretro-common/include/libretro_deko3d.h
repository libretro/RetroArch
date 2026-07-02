/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBRETRO_DEKO3D_H__
#define LIBRETRO_DEKO3D_H__

#include <libretro.h>
#include <deko3d.h>

#define RETRO_HW_RENDER_INTERFACE_DEKO3D_VERSION 1

/* The image the core has rendered for this frame.
 *
 * `image` is an opaque DkImage initialized by the core. The pointer must
 * remain valid until retro_video_refresh_t has returned. The frontend
 * does not take ownership; the frontend will only sample/blit from it.
 *
 * The image must be sampleable (bit DkImageFlags_UsageRender |
 * DkImageFlags_UsageLoadStore is recommended on the core side). Tile-mode
 * images are fine; the frontend will use dkCmdBufBlitImage which honors
 * the layout. The image must be in a state safe for sampling at the time
 * the frontend processes the frame.
 *
 * `width` / `height` describe the visible region the core has rendered
 * inside `image`. The frontend will treat the rest as undefined.
 *
 * `src_x` / `src_y` are the top-left offset of the visible region within
 * `image`. Defaults to 0. Allows the core to expose a sub-rect of a
 * larger working texture (e.g. a VRAM atlas) without first copying it
 * to an origin-anchored intermediate.
 *
 * `image_width` / `image_height` are the full extent of the underlying
 * `image`. The frontend uses them with `src_x`/`src_y` to compute the
 * source rectangle for its blit. If left as 0, the frontend assumes the
 * core's working texture has no atlas offset and falls back to
 * `image_width == width` / `image_height == height` (legacy behaviour
 * for cores written against the v1 interface).
 *
 * `display_aspect_ratio` is the intended on-screen aspect ratio (e.g.
 * 4.0/3.0, 16.0/9.0). 0.0 = let the frontend decide based on width/height. */
struct retro_deko3d_image
{
   const DkImage *image;
   unsigned width;
   unsigned height;
   unsigned src_x;
   unsigned src_y;
   unsigned image_width;
   unsigned image_height;
   float display_aspect_ratio;
};

/* Hands the frontend the image the core just rendered, plus optional
 * synchronization fences.
 *
 * If `acquire_fence` is non-NULL, the frontend will wait on this fence
 * (via dkQueueWaitFence) before the GPU samples `image`. This lets the
 * core submit its work on a private command buffer that signals the
 * fence and have the frontend ordered after that submission without an
 * explicit dkQueueSubmit roundtrip in the core.
 *
 * If `release_fence` is non-NULL, the frontend will signal it on its
 * queue (via dkCmdBufSignalFence flushed before present) once it has
 * finished reading `image`. The core can wait on this fence before
 * reusing the image to avoid races.
 *
 * Calling set_image with image==NULL is undefined; pass a stale image
 * pointer if you need duped frames.
 *
 * The fences themselves must be valid for the lifetime of the
 * retro_video_refresh_t call only. */
typedef void (*retro_deko3d_set_image_t)(void *handle,
      const struct retro_deko3d_image *image,
      DkFence *acquire_fence,
      DkFence *release_fence);

/* The current frame's sync index. Mirrors libretro_vulkan: bit N of
 * get_sync_index_mask() set means N is a valid index. Cores can use
 * this to maintain a ring of per-frame resources without stalling. */
typedef uint32_t (*retro_deko3d_get_sync_index_t)(void *handle);
typedef uint32_t (*retro_deko3d_get_sync_index_mask_t)(void *handle);

/* CPU-blocking wait until the GPU has finished the work scheduled for
 * the current sync index. Intended for cleanup paths only — do not call
 * this every frame. */
typedef void (*retro_deko3d_wait_sync_index_t)(void *handle);

/* If the core ever submits work to the frontend's DkQueue from any
 * thread (including the retro_run thread), it must do so under
 * lock_queue() / unlock_queue(). Submissions are heavyweight; expect
 * contention if frontend rendering and core rendering serialize. The
 * preferred path is for the core to submit on its own thread/queue and
 * use acquire_fence/release_fence in set_image instead. */
typedef void (*retro_deko3d_lock_queue_t)(void *handle);
typedef void (*retro_deko3d_unlock_queue_t)(void *handle);

struct retro_hw_render_interface_deko3d
{
   /* Must be set to RETRO_HW_RENDER_INTERFACE_DEKO3D. */
   enum retro_hw_render_interface_type interface_type;
   /* Must be set to RETRO_HW_RENDER_INTERFACE_DEKO3D_VERSION. */
   unsigned interface_version;

   /* Opaque handle the core must pass to every function pointer. */
   void *handle;

   /* The DkDevice the frontend has created. The core MUST use this
    * device for any GPU work whose results it hands back to the
    * frontend (set_image image, fences). Allocating a private DkDevice
    * is permitted but its DkMemBlocks will not be visible to the
    * frontend. */
   DkDevice device;

   /* The graphics+compute DkQueue the frontend uses for compositing
    * and present. The core may submit to this queue (under
    * lock_queue/unlock_queue) but the recommended path is for the core
    * to own its own DkQueue and synchronize via fences in set_image. */
   DkQueue queue;

   retro_deko3d_set_image_t              set_image;
   retro_deko3d_get_sync_index_t         get_sync_index;
   retro_deko3d_get_sync_index_mask_t    get_sync_index_mask;
   retro_deko3d_wait_sync_index_t        wait_sync_index;
   retro_deko3d_lock_queue_t             lock_queue;
   retro_deko3d_unlock_queue_t           unlock_queue;
};

#endif
