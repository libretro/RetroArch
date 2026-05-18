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


#ifndef RA_DEKO3D_COMMON_H
#define RA_DEKO3D_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <deko3d.h>
#include <switch.h>

#include <gfx/math/matrix_4x4.h>

#include <libretro_deko3d.h>

#include "../video_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compile-time maxima for fixed-size arrays inside dk3d_t. Runtime
 * counts (dk3d->num_swapchain_images, dk3d->num_frames_in_flight) are
 * populated from settings at init and may be smaller than these caps. */
#define DK3D_MAX_SWAPCHAIN_IMAGES 8
#define DK3D_MAX_FRAMES_IN_FLIGHT 8
#define DK3D_CMDBUF_SIZE          (256 * 1024)
#define DK3D_STAGING_SIZE         (8  * 1024 * 1024)
#define DK3D_MENU_STAGING_SIZE    (4  * 1024 * 1024)
#define DK3D_MENU_VBO_SIZE        (256 * 1024)
#define DK3D_MENU_UBO_SIZE        (64  * 1024)
#define DK3D_MENU_IMAGE_DESCS     256   /* per-frame image descriptor heap slots */

/* Per-frame resources: command buffer + memblock storage + fence the queue
 * signals when this frame's GPU work has completed. The set is rotated
 * round-robin every frame to avoid CPU waits on the GPU. */
typedef struct dk3d_frame
{
   DkCmdBuf   cmdbuf;
   DkMemBlock cmd_memblock;
   DkFence    done_fence;
   bool       done_fence_valid;

   /* Per-frame menu vertex buffer ring. Bump-allocated per draw; reset at
    * frame start. Holds interleaved (pos,uv,color) tuples. */
   DkMemBlock menu_vbo_mem;
   void      *menu_vbo_ptr;
   DkGpuAddr  menu_vbo_gpu;
   uint32_t   menu_vbo_off;
   uint32_t   menu_vbo_cap;

   /* Per-frame menu uniform buffer ring. Each draw allocates one slice for
    * its mvp matrix (or pipeline-supplied UBO data). Bump-allocator. */
   DkMemBlock menu_ubo_mem;
   void      *menu_ubo_ptr;
   DkGpuAddr  menu_ubo_gpu;
   uint32_t   menu_ubo_off;
   uint32_t   menu_ubo_cap;

   /* Per-frame image-descriptor heap. CPU writes new dk::ImageDescriptor
    * entries during draws; GPU reads via the cmdbuf-bound heap. Reset per
    * frame so unfenced-frame reuse cannot corrupt in-flight reads. */
   DkMemBlock menu_img_desc_mem;
   void      *menu_img_desc_ptr;
   DkGpuAddr  menu_img_desc_gpu;
   uint32_t   menu_img_desc_next;
} dk3d_frame_t;

/* Image tracked alongside its backing memory so we can free both. */
typedef struct dk3d_image
{
   DkImage      image;       /* opaque: must outlive the queue work referencing it */
   DkMemBlock   memblock;
   uint32_t     width;
   uint32_t     height;
   DkImageFormat format;
} dk3d_image_t;

/* Linear staging buffer pair: a CPU-visible DkMemBlock that the CPU writes
 * pixel data into, and a tile-mode DkImage that the GPU samples from after
 * a CopyBufferToImage. We keep both sized to the maximum content the driver
 * can be asked to display (1080p RGBA8 for SW frames; ~512x512 for menu). */
typedef struct dk3d_stage
{
   DkMemBlock cpu_memblock;        /* CpuUncached | GpuCached */
   void      *cpu_ptr;             /* mapped pointer into cpu_memblock */
   uint32_t   cpu_capacity;        /* bytes */

   dk3d_image_t image;             /* tile-mode DkImage: dst of upload, src of blit */
   uint32_t     used_width;
   uint32_t     used_height;
   DkImageFormat used_format;
   bool         valid;
   /* Set by the CPU-side writer when it has fresh pixels in cpu_memblock.
    * Cleared by dk3d_stage_upload_record() after the GPU copy command is
    * placed into the current frame's cmdbuf. Lets writers (e.g.
    * set_texture_frame) be called from a different point in the frame
    * lifecycle than dkCmdBufClear without losing the upload. */
   bool         needs_gpu_upload;
} dk3d_stage_t;

/* Resources backing the gfx_display_ctx_deko3d implementation. Shared
 * across frame slots; per-frame VBO storage lives on dk3d_frame_t. */
typedef struct dk3d_menu_pipeline
{
   DkShader     vsh;
   DkShader     fsh;
   DkMemBlock   shader_mem;

   /* Immutable single-entry sampler heap (linear, clamp-to-edge). Shared
    * across frames — never modified during rendering, so no per-frame copy. */
   DkMemBlock   sampler_desc_mem;
   DkGpuAddr    sampler_desc_gpu;

   /* 1x1 white fallback texture, sampled when draw->texture == 0. */
   dk3d_image_t blank_tex;

   /* ortho(0,1,1,0,-1,1): RA menu coords normalized [0,1] top-left origin. */
   math_matrix_4x4 mvp_no_rot;

   /* Sticky state from blend/scissor ctx callbacks; consumed by draw(). */
   bool         blend_on;
   bool         scissor_on;
   int32_t      scissor_x, scissor_y;
   uint32_t     scissor_w, scissor_h;
} dk3d_menu_pipeline_t;

/* The driver-private state. Mirrors switch_video_t in spirit but holds
 * deko3d objects instead of a CPU framebuffer. */
typedef struct dk3d
{
   /* Window / viewport */
   NWindow             *win;
   unsigned             surface_width;
   unsigned             surface_height;
   struct video_viewport vp;
   bool                  keep_aspect;
   bool                  smooth;       /* bilinear vs point */
   bool                  rgb32;
   bool                  vsync;
   bool                  hard_sync;              /* settings->bools.video_hard_sync */
   unsigned              hard_sync_frames;       /* settings->uints.video_hard_sync_frames, 0..3 */
   unsigned              swap_interval;          /* dkSwapchainSetSwapInterval value when vsync on */
   unsigned              num_swapchain_images;   /* effective count, <= DK3D_MAX_SWAPCHAIN_IMAGES */
   unsigned              num_frames_in_flight;   /* effective ring depth, <= DK3D_MAX_FRAMES_IN_FLIGHT */
   bool                  is_threaded;
   bool                  should_resize;
   unsigned              rotation;
   unsigned              last_width;
   unsigned              last_height;
   /* AppletOperationMode at last init/resize. dk3d_check_resize() recreates
    * the swapchain when this changes (dock <-> handheld). */
   int                   applet_op_mode;

   /* Core deko3d objects */
   DkDevice    device;
   DkQueue     queue;

   /* Per-image swapchain backing.
    * dk_images[i] outlives sc; sc references their addresses. */
   dk3d_image_t       sc_images[DK3D_MAX_SWAPCHAIN_IMAGES];
   const DkImage     *sc_image_ptrs[DK3D_MAX_SWAPCHAIN_IMAGES];
   DkSwapchain        swapchain;
   int                acquired_slot;

   /* Frame ring */
   dk3d_frame_t       frames[DK3D_MAX_FRAMES_IN_FLIGHT];
   uint32_t           current_frame;     /* index into frames[] */
   uint32_t           frame_count;       /* monotonic */

   /* SW path (frame() with non-NULL frame and no HW set_image) */
   dk3d_stage_t       sw_stage;

   /* Menu texture (set_texture_frame) */
   dk3d_stage_t       menu_stage;
   bool               menu_enable;
   bool               menu_full_screen;
   float              menu_alpha;

   /* gfx_display ctx state — proper menu driver (Ozone/XMB/MaterialUI). */
   dk3d_menu_pipeline_t menu_pipe;

   /* HW render path: set_image hands us this. Pointers must stay valid
    * for the duration of the corresponding video_refresh call. */
   const DkImage     *hw_image;
   unsigned           hw_image_width;
   unsigned           hw_image_height;
   unsigned           hw_src_x;
   unsigned           hw_src_y;
   unsigned           hw_image_full_w;
   unsigned           hw_image_full_h;
   float              hw_aspect;
   DkFence           *hw_acquire_fence;
   DkFence           *hw_release_fence;
   bool               hw_enable;

   struct retro_hw_render_interface_deko3d hw_iface;

   /* 3D-engine blit pipeline (replaces 2D dkCmdBufBlitImage for HW path).
    * Vertex shader uses gl_VertexIndex (no VBO). Sampler is bilinear
    * clamp-to-edge, matching the menu pipeline's default. */
   DkShader     blit_vsh;
   DkShader     blit_fsh;
   DkMemBlock   blit_shader_mem;
   DkMemBlock   blit_sampler_desc_mem;
   DkGpuAddr    blit_sampler_desc_gpu;
} dk3d_t;

/* Helpers (dk3d_init.c-style: implemented in deko3d.c for now). */
bool  dk3d_create_image_2d(DkDevice device,
      uint32_t width, uint32_t height, DkImageFormat fmt,
      uint32_t flags, dk3d_image_t *out);
void  dk3d_destroy_image(dk3d_image_t *img);

bool  dk3d_stage_create(DkDevice device, uint32_t cpu_capacity,
      uint32_t max_width, uint32_t max_height, DkImageFormat fmt,
      dk3d_stage_t *out);
void  dk3d_stage_destroy(dk3d_stage_t *stage);

#ifdef __cplusplus
}
#endif

#endif
