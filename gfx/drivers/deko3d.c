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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <encodings/utf.h>

#include "../font_driver.h"
#include "../video_driver.h"
#include "../gfx_display.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif
#include "../common/deko3d_common.h"

#include "../../verbosity.h"
#include "../../configuration.h"
#include <features/features_cpu.h>

/* ====================================================================== *
 *  Helpers: image and staging creation
 * ====================================================================== */

/* deko3d debug callback — without this set on the device, RaiseError
 * aborts the process via svcBreak with no diagnostic. Routing it through
 * RARCH_ERR surfaces the actual deko3d error string in retroarch.log. */
static void dk3d_debug_cb(void *userData, const char *context,
      DkResult result, const char *message)
{
   (void)userData;
   RARCH_ERR("[deko3d] %s: result=%d (%s)\n",
         context ? context : "(no ctx)", (int)result,
         message ? message : "");
}

static uint32_t dk3d_align_up(uint32_t v, uint32_t a)
{
   return (v + (a - 1)) & ~(a - 1);
}

bool dk3d_create_image_2d(DkDevice device,
      uint32_t width, uint32_t height, DkImageFormat fmt,
      uint32_t flags, dk3d_image_t *out)
{
   DkImageLayoutMaker lm;
   DkImageLayout      layout;
   DkMemBlockMaker    mm;
   uint64_t           size;
   uint32_t           align;
   uint32_t           memsize;

   memset(out, 0, sizeof(*out));

   dkImageLayoutMakerDefaults(&lm, device);
   lm.flags         = flags;
   lm.format        = fmt;
   lm.dimensions[0] = width;
   lm.dimensions[1] = height;
   lm.dimensions[2] = 0;
   dkImageLayoutInitialize(&layout, &lm);

   size  = dkImageLayoutGetSize(&layout);
   align = dkImageLayoutGetAlignment(&layout);

   /* DkMemBlock storage size must be aligned to 0x1000 (4 KiB pages). */
   memsize = dk3d_align_up((uint32_t)size, 0x1000);
   if (memsize < align) /* defensive */
      memsize = dk3d_align_up(align, 0x1000);

   dkMemBlockMakerDefaults(&mm, device, memsize);
   mm.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;

   out->memblock = dkMemBlockCreate(&mm);
   if (!out->memblock)
      return false;

   dkImageInitialize(&out->image, &layout, out->memblock, 0);
   out->width  = width;
   out->height = height;
   out->format = fmt;
   return true;
}

void dk3d_destroy_image(dk3d_image_t *img)
{
   if (img->memblock)
   {
      dkMemBlockDestroy(img->memblock);
      img->memblock = NULL;
   }
   memset(&img->image, 0, sizeof(img->image));
   img->width = img->height = 0;
}

bool dk3d_stage_create(DkDevice device, uint32_t cpu_capacity,
      uint32_t max_width, uint32_t max_height, DkImageFormat fmt,
      dk3d_stage_t *out)
{
   DkMemBlockMaker mm;

   memset(out, 0, sizeof(*out));

   dkMemBlockMakerDefaults(&mm, device, dk3d_align_up(cpu_capacity, 0x1000));
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;

   out->cpu_memblock = dkMemBlockCreate(&mm);
   if (!out->cpu_memblock)
      return false;
   out->cpu_ptr      = dkMemBlockGetCpuAddr(out->cpu_memblock);
   out->cpu_capacity = mm.size;

   if (!dk3d_create_image_2d(device, max_width, max_height, fmt,
            DkImageFlags_Usage2DEngine | DkImageFlags_UsageLoadStore,
            &out->image))
   {
      dkMemBlockDestroy(out->cpu_memblock);
      out->cpu_memblock = NULL;
      return false;
   }

   out->used_format = fmt;
   return true;
}

void dk3d_stage_destroy(dk3d_stage_t *stage)
{
   dk3d_destroy_image(&stage->image);
   if (stage->cpu_memblock)
   {
      dkMemBlockDestroy(stage->cpu_memblock);
      stage->cpu_memblock = NULL;
   }
   stage->cpu_ptr      = NULL;
   stage->cpu_capacity = 0;
   stage->valid        = false;
}

/* ====================================================================== *
 *  Internal: blit + present helpers
 * ====================================================================== */

enum {
   DK3D_BLIT_LINEAR        = 1u << 0,  /* bilinear filter; else nearest      */
   DK3D_BLIT_PREMULT_BLEND = 1u << 1,  /* premultiplied alpha; else opaque   */
   DK3D_BLIT_FLIP_Y        = 1u << 2,  /* vertical flip (pitch-linear srcs)  */
};

static void dk3d_make_image_view(DkImageView *view, const DkImage *img)
{
   dkImageViewDefaults(view, img);
}

/* Compute centered destination rect inside swapchain that preserves
 * aspect ratio. surface_w/h is the swapchain image dimension. */
static void dk3d_compute_dst_rect(unsigned surface_w, unsigned surface_h,
      unsigned src_w, unsigned src_h, float aspect,
      DkImageRect *out)
{
   float src_ratio;
   float surf_ratio;
   unsigned dst_w, dst_h;

   if (src_w == 0 || src_h == 0)
   {
      out->x = out->y = out->z = 0;
      out->width  = surface_w;
      out->height = surface_h;
      out->depth  = 1;
      return;
   }

   if (aspect > 0.0f)
      src_ratio = aspect;
   else
      src_ratio = (float)src_w / (float)src_h;

   surf_ratio = (float)surface_w / (float)surface_h;

   if (src_ratio > surf_ratio)
   {
      dst_w = surface_w;
      dst_h = (unsigned)((float)surface_w / src_ratio + 0.5f);
   }
   else
   {
      dst_h = surface_h;
      dst_w = (unsigned)((float)surface_h * src_ratio + 0.5f);
   }

   if (dst_w > surface_w) dst_w = surface_w;
   if (dst_h > surface_h) dst_h = surface_h;

   out->x      = (surface_w - dst_w) / 2;
   out->y      = (surface_h - dst_h) / 2;
   out->z      = 0;
   out->width  = dst_w;
   out->height = dst_h;
   out->depth  = 1;
}

/* ====================================================================== *
 *  HW render interface — what the core sees
 * ====================================================================== */

static void dk3d_hw_set_image(void *handle,
      const struct retro_deko3d_image *image,
      DkFence *acquire_fence, DkFence *release_fence)
{
   dk3d_t *dk3d = (dk3d_t*)handle;
   if (!dk3d || !image || !image->image)
   {
      if (dk3d) dk3d->hw_image = NULL;
      return;
   }
   dk3d->hw_image          = image->image;
   dk3d->hw_image_width    = image->width;
   dk3d->hw_image_height   = image->height;
   dk3d->hw_src_x          = image->src_x;
   dk3d->hw_src_y          = image->src_y;
   /* Fall back to (visible region == whole image) if the core didn't fill
    * image_width/image_height. Keeps v1 cores working. */
   dk3d->hw_image_full_w   = image->image_width  ? image->image_width  : image->width;
   dk3d->hw_image_full_h   = image->image_height ? image->image_height : image->height;
   dk3d->hw_aspect         = image->display_aspect_ratio;
   dk3d->hw_acquire_fence  = acquire_fence;
   dk3d->hw_release_fence  = release_fence;
}

static uint32_t dk3d_hw_get_sync_index(void *handle)
{
   dk3d_t *dk3d = (dk3d_t*)handle;
   return dk3d ? dk3d->current_frame : 0;
}

static uint32_t dk3d_hw_get_sync_index_mask(void *handle)
{
   dk3d_t *dk3d = (dk3d_t*)handle;
   return dk3d ? ((1u << dk3d->num_frames_in_flight) - 1) : 0;
}

static void dk3d_hw_wait_sync_index(void *handle)
{
   dk3d_t *dk3d = (dk3d_t*)handle;
   if (!dk3d) return;
   {
      dk3d_frame_t *f = &dk3d->frames[dk3d->current_frame];
      if (f->done_fence_valid)
         dkFenceWait(&f->done_fence, -1);
   }
}

static void dk3d_hw_lock_queue(void *handle)
{
   /* Single-thread frontend usage — no-op for now. */
   (void)handle;
}

static void dk3d_hw_unlock_queue(void *handle)
{
   (void)handle;
}

static void dk3d_init_hw_interface(dk3d_t *dk3d)
{
   struct retro_hw_render_interface_deko3d *iface = &dk3d->hw_iface;
   memset(iface, 0, sizeof(*iface));
   iface->interface_type      = RETRO_HW_RENDER_INTERFACE_DEKO3D;
   iface->interface_version   = RETRO_HW_RENDER_INTERFACE_DEKO3D_VERSION;
   iface->handle              = dk3d;
   iface->device              = dk3d->device;
   iface->queue               = dk3d->queue;
   iface->set_image           = dk3d_hw_set_image;
   iface->get_sync_index      = dk3d_hw_get_sync_index;
   iface->get_sync_index_mask = dk3d_hw_get_sync_index_mask;
   iface->wait_sync_index     = dk3d_hw_wait_sync_index;
   iface->lock_queue          = dk3d_hw_lock_queue;
   iface->unlock_queue        = dk3d_hw_unlock_queue;
}

/* ====================================================================== *
 *  gfx_display menu pipeline — shaders, descriptor heaps, blank texture
 * ====================================================================== */

/* DKSH blobs emitted by uam + objcopy via the Makefile.common pattern rule.
 * objcopy -I binary synthesizes _binary_<input>_start/end/size symbols from
 * the input filename — we consume those names directly rather than renaming
 * them, because objcopy --redefine-sym is silently ignored when combined
 * with -I binary in one invocation. */
extern const uint8_t _binary_deko3d_menu_vsh_dksh_start[];
extern const uint8_t _binary_deko3d_menu_vsh_dksh_end[];
extern const uint8_t _binary_deko3d_menu_fsh_dksh_start[];
extern const uint8_t _binary_deko3d_menu_fsh_dksh_end[];
#define deko3d_menu_vsh_dksh      _binary_deko3d_menu_vsh_dksh_start
#define deko3d_menu_vsh_dksh_end  _binary_deko3d_menu_vsh_dksh_end
#define deko3d_menu_fsh_dksh      _binary_deko3d_menu_fsh_dksh_start
#define deko3d_menu_fsh_dksh_end  _binary_deko3d_menu_fsh_dksh_end

/* DKSH blobs for the 3D-engine blit path. */
extern const uint8_t _binary_deko3d_blit_vsh_dksh_start[];
extern const uint8_t _binary_deko3d_blit_vsh_dksh_end[];
extern const uint8_t _binary_deko3d_blit_fsh_dksh_start[];
extern const uint8_t _binary_deko3d_blit_fsh_dksh_end[];
#define deko3d_blit_vsh_dksh      _binary_deko3d_blit_vsh_dksh_start
#define deko3d_blit_vsh_dksh_end  _binary_deko3d_blit_vsh_dksh_end
#define deko3d_blit_fsh_dksh      _binary_deko3d_blit_fsh_dksh_start
#define deko3d_blit_fsh_dksh_end  _binary_deko3d_blit_fsh_dksh_end

/* DKSH header (from deko3d Primer). Each blob is { header, control, code }.
 * dkShaderInitialize takes a pointer to the control section (must outlive
 * the DkShader — we point at .rodata, lifetime is process-wide) and a code
 * offset into a DkMemBlock created with DkMemBlockFlags_Code. */
typedef struct
{
   uint32_t magic;          /* 'DKSH' = 0x48534B44 */
   uint32_t header_sz;
   uint32_t control_sz;
   uint32_t code_sz;
   uint32_t programs_off;
   uint32_t num_programs;
} dk3d_dksh_header_t;
#define DK3D_DKSH_MAGIC 0x48534B44u

static bool dk3d_load_shader_blob(const uint8_t *blob, size_t blob_sz,
      DkShader *out, DkMemBlock code_mem, uint32_t *io_off)
{
   const dk3d_dksh_header_t *hdr;
   uint32_t      off;
   void         *code_cpu;
   DkShaderMaker sm;

   if (blob_sz < sizeof(*hdr))
      return false;
   hdr = (const dk3d_dksh_header_t*)blob;
   if (hdr->magic != DK3D_DKSH_MAGIC)
      return false;

   off = (*io_off + DK_SHADER_CODE_ALIGNMENT - 1)
       & ~(DK_SHADER_CODE_ALIGNMENT - 1);
   if (off + hdr->code_sz > dkMemBlockGetSize(code_mem))
      return false;

   code_cpu = (uint8_t*)dkMemBlockGetCpuAddr(code_mem) + off;
   memcpy(code_cpu, blob + hdr->control_sz, hdr->code_sz);

   dkShaderMakerDefaults(&sm, code_mem, off);
   sm.control = blob;
   dkShaderInitialize(out, &sm);

   *io_off = off + hdr->code_sz;
   return true;
}

/* Eager-upload a 1x1 white pixel to mp->blank_tex via a transient cmdbuf,
 * submitted and waited-on inline so the blank tex is GPU-ready before any
 * menu draw runs. Keeps the hot frame path branch-free. */
static bool dk3d_menu_upload_blank_tex(dk3d_t *dk3d)
{
   dk3d_menu_pipeline_t *mp = &dk3d->menu_pipe;
   DkMemBlockMaker mm;
   DkMemBlock      stage_mem = NULL;
   DkMemBlock      cmd_mem   = NULL;
   DkCmdBuf        cmd       = NULL;
   DkCmdBufMaker   cbm;
   DkCmdList       list;
   DkCopyBuf       src;
   DkImageView     dst_view;
   DkImageRect     dst_rect  = { 0, 0, 0, 1, 1, 1 };
   uint32_t        white     = 0xFFFFFFFFu;

   dkMemBlockMakerDefaults(&mm, dk3d->device, 0x1000);
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   stage_mem = dkMemBlockCreate(&mm);
   if (!stage_mem)
      goto fail;
   memcpy(dkMemBlockGetCpuAddr(stage_mem), &white, sizeof(white));

   dkMemBlockMakerDefaults(&mm, dk3d->device, 0x1000);
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   cmd_mem = dkMemBlockCreate(&mm);
   if (!cmd_mem)
      goto fail;

   dkCmdBufMakerDefaults(&cbm, dk3d->device);
   cmd = dkCmdBufCreate(&cbm);
   if (!cmd)
      goto fail;
   dkCmdBufAddMemory(cmd, cmd_mem, 0, 0x1000);

   src.addr        = dkMemBlockGetGpuAddr(stage_mem);
   src.rowLength   = 4;
   src.imageHeight = 0;
   dkImageViewDefaults(&dst_view, &mp->blank_tex.image);
   dkCmdBufCopyBufferToImage(cmd, &src, &dst_view, &dst_rect, 0);

   list = dkCmdBufFinishList(cmd);
   dkQueueSubmitCommands(dk3d->queue, list);
   dkQueueWaitIdle(dk3d->queue);

   dkCmdBufDestroy(cmd);
   dkMemBlockDestroy(cmd_mem);
   dkMemBlockDestroy(stage_mem);
   return true;

fail:
   if (cmd)       dkCmdBufDestroy(cmd);
   if (cmd_mem)   dkMemBlockDestroy(cmd_mem);
   if (stage_mem) dkMemBlockDestroy(stage_mem);
   return false;
}

static bool dk3d_menu_pipeline_init(dk3d_t *dk3d)
{
   dk3d_menu_pipeline_t *mp = &dk3d->menu_pipe;
   DkMemBlockMaker mm;
   uint32_t        shader_total;
   uint32_t        code_off = 0;

   shader_total = dk3d_align_up(
         (uint32_t)(deko3d_menu_vsh_dksh_end - deko3d_menu_vsh_dksh) +
         (uint32_t)(deko3d_menu_fsh_dksh_end - deko3d_menu_fsh_dksh) +
         DK_SHADER_CODE_ALIGNMENT * 2,
         0x1000);

   dkMemBlockMakerDefaults(&mm, dk3d->device, shader_total);
   mm.flags = DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached
            | DkMemBlockFlags_Code;
   mp->shader_mem = dkMemBlockCreate(&mm);
   if (!mp->shader_mem)
   {
      RARCH_ERR("[deko3d] menu_pipeline: shader memblock alloc failed\n");
      return false;
   }

   if (!dk3d_load_shader_blob(deko3d_menu_vsh_dksh,
            (size_t)(deko3d_menu_vsh_dksh_end - deko3d_menu_vsh_dksh),
            &mp->vsh, mp->shader_mem, &code_off))
   {
      RARCH_ERR("[deko3d] menu_pipeline: VSH DKSH load failed\n");
      return false;
   }
   if (!dk3d_load_shader_blob(deko3d_menu_fsh_dksh,
            (size_t)(deko3d_menu_fsh_dksh_end - deko3d_menu_fsh_dksh),
            &mp->fsh, mp->shader_mem, &code_off))
   {
      RARCH_ERR("[deko3d] menu_pipeline: FSH DKSH load failed\n");
      return false;
   }
   dkMemBlockFlushCpuCache(mp->shader_mem, 0, shader_total);

   /* Immutable sampler descriptor heap — one entry, linear + clamp-to-edge.
    * Never written during rendering so no per-frame copy is required. */
   dkMemBlockMakerDefaults(&mm, dk3d->device,
         dk3d_align_up(sizeof(DkSamplerDescriptor), 0x1000));
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   mp->sampler_desc_mem = dkMemBlockCreate(&mm);
   if (!mp->sampler_desc_mem)
   {
      RARCH_ERR("[deko3d] menu_pipeline: sampler heap alloc failed\n");
      return false;
   }
   mp->sampler_desc_gpu = dkMemBlockGetGpuAddr(mp->sampler_desc_mem);
   {
      DkSampler             sampler;
      DkSamplerDescriptor  *desc = (DkSamplerDescriptor*)
         dkMemBlockGetCpuAddr(mp->sampler_desc_mem);
      dkSamplerDefaults(&sampler);
      sampler.minFilter   = DkFilter_Linear;
      sampler.magFilter   = DkFilter_Linear;
      sampler.wrapMode[0] = DkWrapMode_ClampToEdge;
      sampler.wrapMode[1] = DkWrapMode_ClampToEdge;
      sampler.wrapMode[2] = DkWrapMode_ClampToEdge;
      dkSamplerDescriptorInitialize(desc, &sampler);
   }

   /* 1x1 white fallback texture. */
   if (!dk3d_create_image_2d(dk3d->device, 1, 1, DkImageFormat_RGBA8_Unorm,
            DkImageFlags_Usage2DEngine | DkImageFlags_UsageLoadStore,
            &mp->blank_tex))
   {
      RARCH_ERR("[deko3d] menu_pipeline: blank tex alloc failed\n");
      return false;
   }
   if (!dk3d_menu_upload_blank_tex(dk3d))
   {
      RARCH_ERR("[deko3d] menu_pipeline: blank tex upload failed\n");
      return false;
   }

   /* Default MVP. gfx_display_draw_quad flips Y for us
    * (`draw.y = height - y - h`), so by the time vertices reach the draw
    * path their Y is already bottom-up; with OriginLowerLeft on the device,
    * vanilla ortho(0,1, 0,1, -1,1) lands geometry the right way up. */
   matrix_4x4_ortho(mp->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   mp->blend_on   = false;
   mp->scissor_on = false;
   return true;
}

static void dk3d_menu_pipeline_free(dk3d_t *dk3d)
{
   dk3d_menu_pipeline_t *mp = &dk3d->menu_pipe;
   dk3d_destroy_image(&mp->blank_tex);
   if (mp->sampler_desc_mem)
   {
      dkMemBlockDestroy(mp->sampler_desc_mem);
      mp->sampler_desc_mem = NULL;
   }
   if (mp->shader_mem)
   {
      dkMemBlockDestroy(mp->shader_mem);
      mp->shader_mem = NULL;
   }
}

/* ====================================================================== *
 *  3D-engine blit pipeline — textured full-screen quad via shaders.
 *  Replaces dkCmdBufBlitImage (2D engine) for the HW libretro path.
 *  Uses gl_VertexIndex so no VBO is required.
 * ====================================================================== */

static bool dk3d_blit_pipeline_init(dk3d_t *dk3d)
{
   DkMemBlockMaker mm;
   uint32_t        shader_total;
   uint32_t        code_off = 0;

   shader_total = dk3d_align_up(
         (uint32_t)(deko3d_blit_vsh_dksh_end - deko3d_blit_vsh_dksh) +
         (uint32_t)(deko3d_blit_fsh_dksh_end - deko3d_blit_fsh_dksh) +
         DK_SHADER_CODE_ALIGNMENT * 2,
         0x1000);

   dkMemBlockMakerDefaults(&mm, dk3d->device, shader_total);
   mm.flags = DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached
            | DkMemBlockFlags_Code;
   dk3d->blit_shader_mem = dkMemBlockCreate(&mm);
   if (!dk3d->blit_shader_mem)
   {
      RARCH_ERR("[deko3d] blit_pipeline: shader memblock alloc failed\n");
      return false;
   }

   if (!dk3d_load_shader_blob(deko3d_blit_vsh_dksh,
            (size_t)(deko3d_blit_vsh_dksh_end - deko3d_blit_vsh_dksh),
            &dk3d->blit_vsh, dk3d->blit_shader_mem, &code_off))
   {
      RARCH_ERR("[deko3d] blit_pipeline: VSH DKSH load failed\n");
      return false;
   }
   if (!dk3d_load_shader_blob(deko3d_blit_fsh_dksh,
            (size_t)(deko3d_blit_fsh_dksh_end - deko3d_blit_fsh_dksh),
            &dk3d->blit_fsh, dk3d->blit_shader_mem, &code_off))
   {
      RARCH_ERR("[deko3d] blit_pipeline: FSH DKSH load failed\n");
      return false;
   }
   dkMemBlockFlushCpuCache(dk3d->blit_shader_mem, 0, shader_total);

   /* Single-entry sampler descriptor (bilinear, clamp-to-edge). */
   dkMemBlockMakerDefaults(&mm, dk3d->device,
         dk3d_align_up(sizeof(DkSamplerDescriptor), 0x1000));
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   dk3d->blit_sampler_desc_mem = dkMemBlockCreate(&mm);
   if (!dk3d->blit_sampler_desc_mem)
   {
      RARCH_ERR("[deko3d] blit_pipeline: sampler heap alloc failed\n");
      return false;
   }
   dk3d->blit_sampler_desc_gpu = dkMemBlockGetGpuAddr(dk3d->blit_sampler_desc_mem);
   {
      DkSampler             sampler;
      DkSamplerDescriptor  *desc = (DkSamplerDescriptor*)
         dkMemBlockGetCpuAddr(dk3d->blit_sampler_desc_mem);
      dkSamplerDefaults(&sampler);
      sampler.minFilter   = DkFilter_Linear;
      sampler.magFilter   = DkFilter_Linear;
      sampler.wrapMode[0] = DkWrapMode_ClampToEdge;
      sampler.wrapMode[1] = DkWrapMode_ClampToEdge;
      sampler.wrapMode[2] = DkWrapMode_ClampToEdge;
      dkSamplerDescriptorInitialize(desc, &sampler);
   }

   return true;
}

static void dk3d_blit_pipeline_free(dk3d_t *dk3d)
{
   if (dk3d->blit_sampler_desc_mem)
   {
      dkMemBlockDestroy(dk3d->blit_sampler_desc_mem);
      dk3d->blit_sampler_desc_mem = NULL;
   }
   if (dk3d->blit_shader_mem)
   {
      dkMemBlockDestroy(dk3d->blit_shader_mem);
      dk3d->blit_shader_mem = NULL;
   }
}

/* ====================================================================== *
 *  Initialization
 * ====================================================================== */

static bool dk3d_create_swapchain(dk3d_t *dk3d)
{
   DkSwapchainMaker sm;
   size_t i;

   for (i = 0; i < dk3d->num_swapchain_images; i++)
   {
      /* Usage2DEngine REQUIRED on the swapchain image — we use the 2D
       * engine (dkCmdBufBlitImage) to composite both the core's HW image
       * and font glyphs onto it. Without this flag deko3d aborts on the
       * first blit (dk_image.cpp:144). */
      if (!dk3d_create_image_2d(dk3d->device,
               dk3d->surface_width, dk3d->surface_height,
               DkImageFormat_RGBA8_Unorm,
               DkImageFlags_UsageRender | DkImageFlags_UsagePresent
               | DkImageFlags_Usage2DEngine | DkImageFlags_HwCompression,
               &dk3d->sc_images[i]))
      {
         RARCH_ERR("[deko3d] Failed to allocate swapchain image %u\n",
               (unsigned)i);
         return false;
      }
      dk3d->sc_image_ptrs[i] = &dk3d->sc_images[i].image;
   }

   dkSwapchainMakerDefaults(&sm, dk3d->device, dk3d->win,
         dk3d->sc_image_ptrs, dk3d->num_swapchain_images);
   dk3d->swapchain = dkSwapchainCreate(&sm);
   if (!dk3d->swapchain)
   {
      RARCH_ERR("[deko3d] dkSwapchainCreate failed\n");
      return false;
   }

   /* Default: vsync on. */
   dkSwapchainSetSwapInterval(dk3d->swapchain, dk3d->vsync ? dk3d->swap_interval : 0);
   return true;
}

static bool dk3d_create_frames(dk3d_t *dk3d)
{
   uint32_t i;
   for (i = 0; i < dk3d->num_frames_in_flight; i++)
   {
      DkCmdBufMaker   cm;
      DkMemBlockMaker mm;
      dk3d_frame_t   *f = &dk3d->frames[i];

      dkMemBlockMakerDefaults(&mm, dk3d->device, DK3D_CMDBUF_SIZE);
      mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
      f->cmd_memblock = dkMemBlockCreate(&mm);
      if (!f->cmd_memblock) return false;

      dkCmdBufMakerDefaults(&cm, dk3d->device);
      f->cmdbuf = dkCmdBufCreate(&cm);
      if (!f->cmdbuf) return false;
      dkCmdBufAddMemory(f->cmdbuf, f->cmd_memblock, 0, DK3D_CMDBUF_SIZE);
      f->done_fence_valid = false;

      /* Per-frame menu VBO ring. */
      dkMemBlockMakerDefaults(&mm, dk3d->device,
            dk3d_align_up(DK3D_MENU_VBO_SIZE, 0x1000));
      mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
      f->menu_vbo_mem = dkMemBlockCreate(&mm);
      if (!f->menu_vbo_mem) return false;
      f->menu_vbo_ptr = dkMemBlockGetCpuAddr(f->menu_vbo_mem);
      f->menu_vbo_gpu = dkMemBlockGetGpuAddr(f->menu_vbo_mem);
      f->menu_vbo_off = 0;
      f->menu_vbo_cap = mm.size;

      /* Per-frame menu UBO ring. */
      dkMemBlockMakerDefaults(&mm, dk3d->device,
            dk3d_align_up(DK3D_MENU_UBO_SIZE, 0x1000));
      mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
      f->menu_ubo_mem = dkMemBlockCreate(&mm);
      if (!f->menu_ubo_mem) return false;
      f->menu_ubo_ptr = dkMemBlockGetCpuAddr(f->menu_ubo_mem);
      f->menu_ubo_gpu = dkMemBlockGetGpuAddr(f->menu_ubo_mem);
      f->menu_ubo_off = 0;
      f->menu_ubo_cap = mm.size;

      /* Per-frame image descriptor heap. */
      dkMemBlockMakerDefaults(&mm, dk3d->device,
            dk3d_align_up((uint32_t)sizeof(DkImageDescriptor)
               * DK3D_MENU_IMAGE_DESCS, 0x1000));
      mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
      f->menu_img_desc_mem = dkMemBlockCreate(&mm);
      if (!f->menu_img_desc_mem) return false;
      f->menu_img_desc_ptr  = dkMemBlockGetCpuAddr(f->menu_img_desc_mem);
      f->menu_img_desc_gpu  = dkMemBlockGetGpuAddr(f->menu_img_desc_mem);
      f->menu_img_desc_next = 0;
   }
   return true;
}

static void dk3d_destroy_frames(dk3d_t *dk3d)
{
   uint32_t i;
   for (i = 0; i < dk3d->num_frames_in_flight; i++)
   {
      dk3d_frame_t *f = &dk3d->frames[i];
      if (f->done_fence_valid)
         dkFenceWait(&f->done_fence, -1);
      if (f->menu_img_desc_mem) { dkMemBlockDestroy(f->menu_img_desc_mem); f->menu_img_desc_mem = NULL; }
      if (f->menu_ubo_mem)      { dkMemBlockDestroy(f->menu_ubo_mem);      f->menu_ubo_mem      = NULL; }
      if (f->menu_vbo_mem)      { dkMemBlockDestroy(f->menu_vbo_mem);      f->menu_vbo_mem      = NULL; }
      if (f->cmdbuf)            { dkCmdBufDestroy(f->cmdbuf);              f->cmdbuf            = NULL; }
      if (f->cmd_memblock)      { dkMemBlockDestroy(f->cmd_memblock);      f->cmd_memblock      = NULL; }
      f->done_fence_valid = false;
   }
}

/* The deko3d video driver manages its own surface (nwindow + swapchain)
 * and doesn't use an external gfx_ctx for GL/EGL setup. RA's higher
 * layers still query a gfx_ctx for display metrics (DPI), so we expose
 * a gfx_ctx_driver_t with the entry points that apply to us. Pattern
 * mirrors gx2_gfx's gx2_fake_context (Wii U) and follows the same
 * conventions switch_ctx uses for Switch hardware DPI values. */
static bool dk3d_ctx_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   (void)data;
   if (type != DISPLAY_METRIC_DPI)
      return false;
   switch (appletGetOperationMode())
   {
      case AppletOperationMode_Console:
         /* Docked: 1920x1080 over an assumed 39" TV diagonal. */
         *value = 56.48480f;
         break;
      case AppletOperationMode_Handheld:
      default:
         /* Handheld: 1280x720 over a 6.2" screen. */
         *value = 236.8717f;
         break;
   }
   return true;
}

static gfx_ctx_driver_t dk3d_gfx_ctx;

static void *dk3d_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   DkDeviceMaker dm;
   DkQueueMaker  qm;
   dk3d_t *dk3d = (dk3d_t*)calloc(1, sizeof(*dk3d));
   if (!dk3d)
      return NULL;

   /* Window — pick docked vs handheld dimensions. */
   dk3d->win = nwindowGetDefault();
   dk3d->applet_op_mode = (int)appletGetOperationMode();
   switch (dk3d->applet_op_mode)
   {
      case AppletOperationMode_Console:
         dk3d->surface_width  = 1920;
         dk3d->surface_height = 1080;
         break;
      case AppletOperationMode_Handheld:
      default:
         dk3d->surface_width  = 1280;
         dk3d->surface_height = 720;
         break;
   }
   nwindowSetDimensions(dk3d->win,
         dk3d->surface_width, dk3d->surface_height);

   /* Tell RA core our output dims so gfx_display_t / menu_drivers
    * (Ozone, XMB, MaterialUI) lay out against the real surface instead of
    * the zero-init defaults. */
   video_driver_set_output_size(dk3d->surface_width, dk3d->surface_height);

   dk3d->vp.full_width  = dk3d->surface_width;
   dk3d->vp.full_height = dk3d->surface_height;
   dk3d->vp.width       = dk3d->surface_width;
   dk3d->vp.height      = dk3d->surface_height;
   dk3d->keep_aspect    = true;
   dk3d->should_resize  = true;
   dk3d->is_threaded    = video->is_threaded;
   dk3d->smooth         = video->smooth;
   dk3d->rgb32          = video->rgb32;
   dk3d->vsync          = video->vsync;

   /* Wire RA settings: swapchain depth, swap interval, derived frame ring,
    * hard sync. Mirrors Vulkan driver's read at vulkan_common.c:2297. */
   {
      settings_t *settings = config_get_ptr();
      unsigned want_sc     = settings ? settings->uints.video_max_swapchain_images : 3;
      unsigned want_si     = settings ? settings->uints.video_swap_interval        : 1;
      bool     want_hs     = settings ? settings->bools.video_hard_sync             : false;
      unsigned want_hsf    = settings ? settings->uints.video_hard_sync_frames     : 0;
      if (want_sc < 2)                        want_sc = 2;
      if (want_sc > DK3D_MAX_SWAPCHAIN_IMAGES) want_sc = DK3D_MAX_SWAPCHAIN_IMAGES;
      if (want_si < 1)                        want_si = 1;
      dk3d->num_swapchain_images = want_sc;
      /* Tie frame ring to swapchain — typical Vulkan convention. */
      dk3d->num_frames_in_flight = want_sc;
      if (dk3d->num_frames_in_flight > DK3D_MAX_FRAMES_IN_FLIGHT)
         dk3d->num_frames_in_flight = DK3D_MAX_FRAMES_IN_FLIGHT;
      dk3d->swap_interval     = want_si;
      dk3d->hard_sync         = want_hs;
      /* Clamp hard_sync_frames to ring depth - 1; equals "may not get ahead". */
      dk3d->hard_sync_frames  = (want_hsf < dk3d->num_frames_in_flight)
                                   ? want_hsf
                                   : (dk3d->num_frames_in_flight - 1);
      RARCH_LOG("[deko3d] swapchain_images=%u frames_in_flight=%u swap_interval=%u vsync=%d hard_sync=%d hard_sync_frames=%u\n",
            dk3d->num_swapchain_images, dk3d->num_frames_in_flight,
            dk3d->swap_interval, (int)dk3d->vsync,
            (int)dk3d->hard_sync, dk3d->hard_sync_frames);
   }

   dkDeviceMakerDefaults(&dm);
   dm.flags    = DkDeviceFlags_DepthZeroToOne | DkDeviceFlags_OriginLowerLeft;
   dm.cbDebug  = dk3d_debug_cb;
   dk3d->device = dkDeviceCreate(&dm);
   if (!dk3d->device)
   {
      RARCH_ERR("[deko3d] dkDeviceCreate failed\n");
      goto fail;
   }

   /* Queue — graphics + 2D engine for blits. */
   dkQueueMakerDefaults(&qm, dk3d->device);
   qm.flags = DkQueueFlags_Graphics;
   dk3d->queue = dkQueueCreate(&qm);
   if (!dk3d->queue)
   {
      RARCH_ERR("[deko3d] dkQueueCreate failed\n");
      goto fail;
   }

   if (!dk3d_create_swapchain(dk3d))
      goto fail;
   if (!dk3d_create_frames(dk3d))
      goto fail;

   /* Pre-allocate SW staging — sized for up to 1024x1024 RGBA8.
    * RA never feeds us larger frames in practice. */
   if (!dk3d_stage_create(dk3d->device, DK3D_STAGING_SIZE,
            1024, 1024, DkImageFormat_RGBA8_Unorm, &dk3d->sw_stage))
      goto fail;

   /* Menu staging — set_texture_frame frames are tiny (typically 320x240). */
   if (!dk3d_stage_create(dk3d->device, DK3D_MENU_STAGING_SIZE,
            512, 512, DkImageFormat_RGBA8_Unorm, &dk3d->menu_stage))
      goto fail;

   /* gfx_display menu pipeline — shaders, descriptor heaps, blank tex.
    * Eager init (transient cmdbuf + wait-idle inside) so menu draws can
    * run from the very first frame. */
   if (!dk3d_menu_pipeline_init(dk3d))
      goto fail;

   if (!dk3d_blit_pipeline_init(dk3d))
      goto fail;

   dk3d_init_hw_interface(dk3d);
   {
      struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
      dk3d->hw_bottom_left = hwr ? hwr->bottom_left_origin : true;
   }
   dk3d->menu_alpha   = 1.0f;
   dk3d->acquired_slot = -1;

   if (input && input_data)
   {
      *input      = NULL;
      *input_data = NULL;
   }

   /* OSD/widgets text is rendered by deko3d_font, registered here. The
    * font driver caches an RGBA8 atlas mirror in a tile-mode DkImage and
    * draws each glyph via dkCmdBufBlitImage with alpha blend. */
   font_driver_init_osd(dk3d, video, false, video->is_threaded,
         FONT_DRIVER_RENDER_DEKO3D_API);

   /* Register the gfx_ctx so RA's display layer can query our DPI
    * for ozone/xmb scale calculation. */
   dk3d_gfx_ctx.get_metrics = dk3d_ctx_get_metrics;
   video_context_driver_set(&dk3d_gfx_ctx);

   /* Tell RA that texture_image loaders should hand us RGBA-byte-order
    * pixels (deko3d has no BGRA8 format, so a BGRA upload would
    * channel-swap on sample). gl2.c:4100 sets this the same way. */
   video_driver_set_disp_flags(
         video_driver_get_disp_flags() | VIDEO_FLAG_USE_RGBA);

   RARCH_LOG("[deko3d] driver initialized (%ux%u)\n",
         dk3d->surface_width, dk3d->surface_height);
   return dk3d;

fail:
   dk3d_menu_pipeline_free(dk3d);
   dk3d_blit_pipeline_free(dk3d);
   if (dk3d->menu_stage.cpu_memblock) dk3d_stage_destroy(&dk3d->menu_stage);
   if (dk3d->sw_stage.cpu_memblock)   dk3d_stage_destroy(&dk3d->sw_stage);
   dk3d_destroy_frames(dk3d);
   if (dk3d->swapchain) dkSwapchainDestroy(dk3d->swapchain);
   {
      size_t i;
      for (i = 0; i < dk3d->num_swapchain_images; i++)
         dk3d_destroy_image(&dk3d->sc_images[i]);
   }
   if (dk3d->queue)  dkQueueDestroy(dk3d->queue);
   if (dk3d->device) dkDeviceDestroy(dk3d->device);
   free(dk3d);
   return NULL;
}

static void dk3d_free(void *data)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   size_t i;
   if (!dk3d)
      return;

   if (dk3d->queue)
      dkQueueWaitIdle(dk3d->queue);

   font_driver_free_osd();

   dk3d_menu_pipeline_free(dk3d);
   dk3d_blit_pipeline_free(dk3d);
   dk3d_stage_destroy(&dk3d->menu_stage);
   dk3d_stage_destroy(&dk3d->sw_stage);
   dk3d_destroy_frames(dk3d);
   if (dk3d->swapchain) { dkSwapchainDestroy(dk3d->swapchain); dk3d->swapchain = NULL; }
   for (i = 0; i < dk3d->num_swapchain_images; i++)
      dk3d_destroy_image(&dk3d->sc_images[i]);
   if (dk3d->queue)  { dkQueueDestroy(dk3d->queue);   dk3d->queue  = NULL; }
   if (dk3d->device) { dkDeviceDestroy(dk3d->device); dk3d->device = NULL; }
   free(dk3d);
}

/* ====================================================================== *
 *  Dock/handheld resize
 * ====================================================================== */

/* Tear down the current swapchain + its backing images. Caller must have
 * already drained the queue (dkQueueWaitIdle) so no GPU work references
 * any of these objects. */
static void dk3d_destroy_swapchain(dk3d_t *dk3d)
{
   size_t i;
   if (dk3d->swapchain)
   {
      dkSwapchainDestroy(dk3d->swapchain);
      dk3d->swapchain = NULL;
   }
   for (i = 0; i < dk3d->num_swapchain_images; i++)
   {
      dk3d_destroy_image(&dk3d->sc_images[i]);
      dk3d->sc_image_ptrs[i] = NULL;
   }
}

/* Poll AppletOperationMode and rebuild the swapchain when the dock state
 * changes. Called once per frame from dk3d_frame; cheap when nothing
 * changed (single integer compare). When op mode differs we drain the
 * queue, free in-flight per-frame fences, and recreate everything at the
 * new resolution. */
static void dk3d_check_resize(dk3d_t *dk3d)
{
   const int  op_mode = (int)appletGetOperationMode();
   unsigned   new_w, new_h;
   uint32_t   i;

   if (op_mode == dk3d->applet_op_mode && dk3d->swapchain)
      return;

   switch (op_mode)
   {
      case AppletOperationMode_Console:
         new_w = 1920; new_h = 1080;
         break;
      case AppletOperationMode_Handheld:
      default:
         new_w = 1280; new_h = 720;
         break;
   }

   if (new_w == dk3d->surface_width && new_h == dk3d->surface_height
         && dk3d->swapchain)
   {
      dk3d->applet_op_mode = op_mode;
      return;
   }

   RARCH_LOG("[deko3d] op mode %d -> resize swapchain %ux%u -> %ux%u\n",
         op_mode, dk3d->surface_width, dk3d->surface_height, new_w, new_h);

   /* Drain GPU work that may reference the old swapchain images. */
   dkQueueWaitIdle(dk3d->queue);
   for (i = 0; i < dk3d->num_frames_in_flight; i++)
      dk3d->frames[i].done_fence_valid = false;

   dk3d_destroy_swapchain(dk3d);

   dk3d->surface_width  = new_w;
   dk3d->surface_height = new_h;
   nwindowSetDimensions(dk3d->win, new_w, new_h);
   video_driver_set_output_size(new_w, new_h);

   if (!dk3d_create_swapchain(dk3d))
      RARCH_ERR("[deko3d] swapchain recreate failed at %ux%u\n", new_w, new_h);

   dk3d->vp.full_width  = new_w;
   dk3d->vp.full_height = new_h;
   dk3d->vp.width       = new_w;
   dk3d->vp.height      = new_h;
   dk3d->should_resize  = true;
   dk3d->applet_op_mode = op_mode;
}

/* ====================================================================== *
 *  Per-frame helpers: SW upload, blit, present
 * ====================================================================== */

/* Mark the stage contents as dirty so dk3d_frame issues a
 * CopyBufferToImage on the next command buffer.  Every path that
 * writes fresh pixels into stage->cpu_ptr (menu or core SW frame)
 * calls this to share the small amount of stage state management. */
static void dk3d_stage_mark_dirty(dk3d_stage_t *stage,
      unsigned width, unsigned height)
{
   stage->used_width       = width;
   stage->used_height      = height;
   stage->valid            = true;
   stage->needs_gpu_upload = true;
}

/* GPU half: record the CopyBufferToImage onto the supplied cmdbuf and
 * clear needs_gpu_upload. Must be called from inside dk3d_frame after
 * dkCmdBufClear so the command isn't discarded. */
static void dk3d_stage_upload_record(dk3d_stage_t *stage, DkCmdBuf cmd)
{
   DkImageView dst_view;
   DkImageRect dst_rect;
   DkCopyBuf   src_buf;

   if (!stage->needs_gpu_upload)
      return;

   /* DkCopyBuf.rowLength is BYTES not pixels.
    * Source buffer is packed RGBA8 = 4 bytes per pixel. */
   src_buf.addr        = dkMemBlockGetGpuAddr(stage->cpu_memblock);
   src_buf.rowLength   = stage->used_width * 4;
   src_buf.imageHeight = 0; /* 0 = tightly packed in height */

   dkImageViewDefaults(&dst_view, &stage->image.image);

   dst_rect.x = dst_rect.y = dst_rect.z = 0;
   dst_rect.width  = stage->used_width;
   dst_rect.height = stage->used_height;
   dst_rect.depth  = 1;

   dkCmdBufCopyBufferToImage(cmd, &src_buf, &dst_view, &dst_rect, 0);
   dkCmdBufBarrier(cmd, DkBarrier_Fragments, DkInvalidateFlags_Image);
   stage->needs_gpu_upload = false;
}

/* Convenience wrapper: CPU format-convert + immediate GPU record for the
 * core SW-frame path.  Caller must already be inside dk3d_frame's command-
 * recording window (no dkCmdBufClear after this).  The stage image is
 * always RGBA8 (4 bytes/pixel) regardless of the source format.
 * Cores emit RGB565 when !rgb32; the menu path (set_texture_frame) emits
 * RGBA4444 and is handled separately. */
static bool dk3d_stage_upload(dk3d_t *dk3d, dk3d_stage_t *stage,
      const void *pixels, unsigned width, unsigned height,
      unsigned src_pitch, bool rgb32)
{
   dk3d_frame_t *f = &dk3d->frames[dk3d->current_frame];
   uint32_t needed = width * height * 4;
   if (needed > stage->cpu_capacity)
      return false;

   if (rgb32)
   {
      const uint32_t *src;
      uint32_t       *dst;
      const uint8_t  *src_row = (const uint8_t*)pixels;
      uint32_t       *dst_row = (uint32_t*)stage->cpu_ptr;
      unsigned y, x;
      for (y = 0; y < height;
            y++, src_row += src_pitch, dst_row += width)
      {
         src = (const uint32_t*)src_row;
         dst = dst_row;
         for (x = 0; x < width; x++)
         {
            uint32_t p = src[x];
            dst[x] = (p & 0xff00ff00u)
                    | ((p & 0x00ff0000u) >> 16)
                    | ((p & 0x000000ffu) << 16);
         }
      }
   }
   else
   {
      /* RGB565 -> RGBA8.  Cores output RGB565 when rgb32=false. */
      uint32_t *dst = (uint32_t*)stage->cpu_ptr;
      const uint8_t *src_row = (const uint8_t*)pixels;
      unsigned y, x;
      for (y = 0; y < height; y++, src_row += src_pitch)
      {
         const uint16_t *s16 = (const uint16_t*)src_row;
         uint32_t *drow = dst + y * width;
         for (x = 0; x < width; x++)
         {
            uint16_t p = s16[x];
            uint32_t r = (p >> 11) & 0x1f;
            uint32_t g = (p >> 5)  & 0x3f;
            uint32_t b =  p        & 0x1f;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            drow[x] = 0xff000000u | (r << 16) | (g << 8) | b;
         }
      }
   }

   dk3d_stage_mark_dirty(stage, width, height);
   dk3d_stage_upload_record(stage, f->cmdbuf);
   return true;
}

/* Forward decl: dk3d_set_viewport is defined later but called from dk3d_frame. */
static void dk3d_set_viewport(void *data, unsigned vp_width,
      unsigned vp_height, bool force_full, bool allow_rotate);

static void dk3d_blit_sub(dk3d_t *dk3d, dk3d_frame_t *f,
      const DkImage *src, unsigned src_x, unsigned src_y,
      unsigned src_w, unsigned src_h,
      const DkImage *dst, const DkImageRect *dst_rect, uint32_t opts)
{
   DkImageView src_view, dst_view;
   DkImageRect src_rect;
   uint32_t flags;

   dk3d_make_image_view(&src_view, src);
   dk3d_make_image_view(&dst_view, dst);

   src_rect.x = src_x;
   src_rect.y = src_y;
   src_rect.z = 0;
   src_rect.width  = src_w;
   src_rect.height = src_h;
   src_rect.depth  = 1;

   flags  = (opts & DK3D_BLIT_LINEAR)        ? DkBlitFlag_FilterLinear  : DkBlitFlag_FilterNearest;
   flags |= (opts & DK3D_BLIT_PREMULT_BLEND) ? DkBlitFlag_ModePremultBlend : DkBlitFlag_ModeBlit;
   if (opts & DK3D_BLIT_FLIP_Y)
      flags |= DkBlitFlag_FlipY;

   dkCmdBufBlitImage(f->cmdbuf, &src_view, &src_rect,
         &dst_view, dst_rect, flags, 0);
}

static void dk3d_blit(dk3d_t *dk3d, dk3d_frame_t *f,
      const DkImage *src, unsigned src_w, unsigned src_h,
      const DkImage *dst, const DkImageRect *dst_rect, uint32_t opts)
{
   dk3d_blit_sub(dk3d, f, src, 0, 0, src_w, src_h, dst, dst_rect, opts);
}

/* ====================================================================== *
 *  frame()
 * ====================================================================== */

/* Forward decl — defined in the gfx_display ctx section below. */
static uint32_t dk3d_menu_bump(uint32_t *off, uint32_t cap,
      uint32_t size, uint32_t align);

static bool dk3d_frame(void *data, const void *frame,
      unsigned width, unsigned height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   dk3d_t        *dk3d = (dk3d_t*)data;
   dk3d_frame_t  *f;
   const float    clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
   DkCmdList      cmds;
   DkImageView    sc_view;
   const DkImage *swap_img;
   unsigned       sw, sh;
#ifdef HAVE_MENU
   bool menu_is_alive   = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE)
      ? true : false;
#endif

   if (!dk3d)
      return false;

   /* Pick up dock <-> handheld transitions before we use the surface dims. */
   dk3d_check_resize(dk3d);

   sw = dk3d->surface_width;
   sh = dk3d->surface_height;

   /* Wait the frame slot we are about to reuse. */
   f = &dk3d->frames[dk3d->current_frame];
   if (f->done_fence_valid)
   {
      dkFenceWait(&f->done_fence, -1);
      f->done_fence_valid = false;
   }
   dkCmdBufClear(f->cmdbuf);
   /* Reset per-frame menu bump allocators. Safe here: the cmdbuf for this
    * frame slot was waited above (done_fence), so the GPU is no longer
    * reading from these memblocks. */
   f->menu_vbo_off       = 0;
   f->menu_ubo_off       = 0;
   f->menu_img_desc_next = 0;

   /* Acquire next swapchain image. */
   dk3d->acquired_slot = dkQueueAcquireImage(dk3d->queue, dk3d->swapchain);
   if (dk3d->acquired_slot < 0)
      return false;
   swap_img = &dk3d->sc_images[dk3d->acquired_slot].image;
   /* Bind the swapchain image as render target so the clear lands on it.
    * Content blit (below) uses the 3D engine (shader pipeline) for the HW
    * path, or the 2D engine (dkCmdBufBlitImage) for the SW path. A single
    * barrier after the content section transitions between the two engines
    * for the 2D-engine menu overlay. */
   dk3d_make_image_view(&sc_view, swap_img);
   {
      const DkImageView *targets[1] = { &sc_view };
      dkCmdBufBindRenderTargets(f->cmdbuf, targets, 1, NULL);
      dkCmdBufClearColor(f->cmdbuf, 0,
            DkColorMask_RGBA, clear_color);
   }

   /* Decide what to draw: HW image (preferred) > SW frame > nothing. */
   {
      const DkImage *src     = NULL;
      unsigned       src_w   = 0;
      unsigned       src_h   = 0;
      float          aspect  = 0.0f;

      if (dk3d->hw_image && frame == RETRO_HW_FRAME_BUFFER_VALID)
      {
         src    = dk3d->hw_image;
         src_w  = dk3d->hw_image_width  ? dk3d->hw_image_width  : width;
         src_h  = dk3d->hw_image_height ? dk3d->hw_image_height : height;
         aspect = dk3d->hw_aspect;

         if (dk3d->hw_acquire_fence)
            dkQueueWaitFence(dk3d->queue, dk3d->hw_acquire_fence);
      }
      else if (frame && frame != RETRO_HW_FRAME_BUFFER_VALID
            && width > 0 && height > 0)
      {
         if (dk3d_stage_upload(dk3d, &dk3d->sw_stage,
                  frame, width, height, pitch, dk3d->rgb32))
         {
            src   = &dk3d->sw_stage.image.image;
            src_w = width;
            src_h = height;
         }
      }

      if (src)
      {
         DkImageRect dst_rect;
         unsigned blit_src_x = 0, blit_src_y = 0;

         /* Refresh vp from RA's current settings stack (aspect ratio,
          * integer scale, custom vp) whenever they change. video_st->
          * scale_width/height also updates inside, so the stats panel
          * gets meaningful numbers. */
         if (dk3d->should_resize
             || dk3d->vp.full_width  != sw
             || dk3d->vp.full_height != sh)
         {
            dk3d_set_viewport(dk3d, sw, sh, false, true);
            dk3d->should_resize = false;
         }

         if (src == dk3d->hw_image)
         {
            /* Sub-rect within the core's working texture. v2 interface fields. */
            blit_src_x = dk3d->hw_src_x;
            blit_src_y = dk3d->hw_src_y;
         }

         /* Use RA's settings-aware viewport as the blit destination. */
         dst_rect.x      = dk3d->vp.x;
         dst_rect.y      = dk3d->vp.y;
         dst_rect.z      = 0;
         dst_rect.width  = dk3d->vp.width  ? dk3d->vp.width  : sw;
         dst_rect.height = dk3d->vp.height ? dk3d->vp.height : sh;
         dst_rect.depth  = 1;
         (void)aspect; /* aspect handled by RA's update_viewport now */

         dk3d_blit_sub(dk3d, f, src, blit_src_x, blit_src_y, src_w, src_h,
               swap_img, &dst_rect,
               (dk3d->smooth ? DK3D_BLIT_LINEAR : 0)
               | (dk3d->hw_bottom_left ? 0 : DK3D_BLIT_FLIP_Y));
      }
   }

   /* Barrier: transition from 3D engine (clear + optional HW blit) to
    * 2D engine (menu overlay and font blits). Needed whenever the swapchain
    * image was touched by the 3D engine and will be read by the 2D engine. */
   dkCmdBufBarrier(f->cmdbuf, DkBarrier_Fragments,
         DkInvalidateFlags_Image);
   /* Menu overlay (rgui set_texture_frame). */
#ifdef HAVE_MENU
   if (dk3d->menu_enable && dk3d->menu_stage.valid)
   {
      DkImageRect dst_rect;
      unsigned mw = dk3d->menu_stage.used_width;
      unsigned mh = dk3d->menu_stage.used_height;
      /* If set_texture_frame loaded fresh pixels since last frame,
       * record the GPU upload now (cmdbuf is past dkCmdBufClear). */
      dk3d_stage_upload_record(&dk3d->menu_stage, f->cmdbuf);
      if (dk3d->menu_full_screen)
      {
         dst_rect.x = dst_rect.y = dst_rect.z = 0;
         dst_rect.width  = sw;
         dst_rect.height = sh;
         dst_rect.depth  = 1;
      }
      else
         dk3d_compute_dst_rect(sw, sh, mw, mh, 0.0f, &dst_rect);
      dk3d_blit(dk3d, f, &dk3d->menu_stage.image.image, mw, mh,
            swap_img, &dst_rect,
            DK3D_BLIT_FLIP_Y);
   }

   if (menu_is_alive)
      menu_driver_frame(menu_is_alive, video_info);
   else if (video_info->statistics_show)
   {
      const struct font_params *osd_params =
         (const struct font_params*)&video_info->osd_stat_params;
      if (osd_params)
         font_driver_render_msg(dk3d, video_info->stat_text,
               osd_params, NULL);
   }
#endif

   if (msg)
      font_driver_render_msg(dk3d, msg, NULL, NULL);

#ifdef HAVE_GFX_WIDGETS
   if (video_info->widgets_active)
      gfx_widgets_frame(video_info);
#endif
   /* Signal frame fence (also acts as release_fence carrier for hw path). */
   if (dk3d->hw_release_fence)
      dkCmdBufSignalFence(f->cmdbuf, dk3d->hw_release_fence, true);

   dkCmdBufSignalFence(f->cmdbuf, &f->done_fence, true);
   f->done_fence_valid = true;

   cmds = dkCmdBufFinishList(f->cmdbuf);
   dkQueueSubmitCommands(dk3d->queue, cmds);
   dkQueuePresentImage(dk3d->queue, dk3d->swapchain, dk3d->acquired_slot);
   dk3d->acquired_slot = -1;

   /* Reset hw fences — they are valid only for the current refresh. */
   dk3d->hw_acquire_fence = NULL;
   dk3d->hw_release_fence = NULL;

   /* video_hard_sync: CPU waits until the GPU is at most hard_sync_frames
    * behind. Equivalent to GL's glFinish-after-swap (frames=0) or vkWaitForFences
    * on a fence N frames back. Slot N back == (current - N) mod ring; current
    * still points at the just-submitted slot (advance happens below). */
   if (dk3d->hard_sync)
   {
      unsigned back  = dk3d->hard_sync_frames;
      unsigned ring  = dk3d->num_frames_in_flight;
      unsigned idx   = (dk3d->current_frame + ring - back) % ring;
      dk3d_frame_t *wait_f = &dk3d->frames[idx];
      if (wait_f->done_fence_valid)
         dkFenceWait(&wait_f->done_fence, -1);
   }

   /* Advance frame ring. */
   dk3d->current_frame = (dk3d->current_frame + 1) % dk3d->num_frames_in_flight;
   dk3d->frame_count++;
   return true;
}

/* ====================================================================== *
 *  Trivial driver hooks
 * ====================================================================== */

/* Forward decl — defined in the gfx_display ctx section below. */
static uint32_t dk3d_menu_bump(uint32_t *off, uint32_t cap,
      uint32_t size, uint32_t align);

/* ====================================================================== *
 *  Font driver — 3D-pipeline textured-quad text
 *
 *  RA's font_driver layer hands us a CPU-rasterized 8-bit alpha atlas via
 *  font_renderer (stb_truetype on this build). We mirror that atlas into
 *  an RGBA8 tile-mode DkImage (white * alpha) and draw glyphs as
 *  per-character dkCmdBufBlitImage calls into the current swapchain image
 *  with mode=AlphaBlend. No shaders are required — the 2D engine handles
 *  format conversion + blend.
 *
 *  Limitations:
 *   - Color is fixed to white (atlas is pre-multiplied white). Tinted
 *     text (red/yellow stat overlays) will appear white. A shader-based
 *     font driver is needed for proper color tinting.
 *   - Drop shadow (params->drop_x/drop_y) is ignored.
 *   - One blit per glyph — fine for the OSD, suboptimal for big menus.
 * ====================================================================== */

#define DK3D_FONT_ATLAS_MAX_W  1024
#define DK3D_FONT_ATLAS_MAX_H  1024

typedef struct dk3d_font
{
   dk3d_t                       *dk3d;     /* set by font_renderer init data param */
   const font_renderer_driver_t *font_driver;
   void                         *font_data;
   struct font_atlas            *atlas;

   /* Atlas mirror on GPU */
   dk3d_image_t                  atlas_img;
   DkMemBlock                    cpu_buf;     /* CPU-visible staging memblock */
   void                         *cpu_ptr;
   uint32_t                      cpu_size;
   uint32_t                      atlas_w;
   uint32_t                      atlas_h;
   bool                          atlas_uploaded;
} dk3d_font_t;

static bool dk3d_font_alloc_atlas(dk3d_font_t *f, DkDevice device,
      uint32_t w, uint32_t h)
{
   DkMemBlockMaker mm;
   uint32_t cpu_bytes = w * h * 4;

   if (!dk3d_create_image_2d(device, w, h, DkImageFormat_RGBA8_Unorm,
            DkImageFlags_Usage2DEngine | DkImageFlags_UsageLoadStore,
            &f->atlas_img))
      return false;

   dkMemBlockMakerDefaults(&mm, device, dk3d_align_up(cpu_bytes, 0x1000));
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   f->cpu_buf  = dkMemBlockCreate(&mm);
   if (!f->cpu_buf)
   {
      dk3d_destroy_image(&f->atlas_img);
      return false;
   }
   f->cpu_ptr  = dkMemBlockGetCpuAddr(f->cpu_buf);
   f->cpu_size = mm.size;
   f->atlas_w  = w;
   f->atlas_h  = h;
   return true;
}

static void *dk3d_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   dk3d_font_t *f = (dk3d_font_t*)calloc(1, sizeof(*f));
   if (!f)
      return NULL;

   /* `data` is the dk3d_t pointer the video driver passed via
    * font_driver_init_osd. We need it to access the device + the active
    * swapchain image when rendering. */
   f->dk3d = (dk3d_t*)data;
   if (!f->dk3d || !f->dk3d->device)
   {
      free(f);
      return NULL;
   }

   if (!font_renderer_create_default(&f->font_driver,
            &f->font_data, font_path, font_size))
   {
      free(f);
      return NULL;
   }
   f->atlas = f->font_driver->get_atlas(f->font_data);

   if (!dk3d_font_alloc_atlas(f, f->dk3d->device,
            DK3D_FONT_ATLAS_MAX_W, DK3D_FONT_ATLAS_MAX_H))
   {
      f->font_driver->free(f->font_data);
      free(f);
      return NULL;
   }
   return f;
}

static void dk3d_font_free(void *data, bool is_threaded)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   if (!f)
      return;
   if (f->cpu_buf)        dkMemBlockDestroy(f->cpu_buf);
   dk3d_destroy_image(&f->atlas_img);
   if (f->font_driver && f->font_data)
      f->font_driver->free(f->font_data);
   free(f);
}

/* Inflate the alpha-only CPU atlas into an RGBA8 (white,white,white,a)
 * staging buffer and copy it into the GPU atlas image. Called only when
 * the atlas dirty flag is set or the GPU mirror has never been
 * populated. */
static void dk3d_font_upload_atlas(dk3d_font_t *f, DkCmdBuf cmd)
{
   const struct font_atlas *a = f->atlas;
   uint32_t y, x;
   uint32_t need;
   DkImageView dst_view;
   DkImageRect dst_rect;
   DkCopyBuf   src_buf;

   if (!a || !a->buffer)
      return;
   if (a->width  > f->atlas_w) return;
   if (a->height > f->atlas_h) return;

   need = a->width * a->height * 4;
   if (need > f->cpu_size)
      return;

   {
      uint32_t *dst = (uint32_t*)f->cpu_ptr;
      const uint8_t *src = a->buffer;
      for (y = 0; y < a->height; y++)
      {
         for (x = 0; x < a->width; x++)
         {
            uint32_t alpha = src[y * a->width + x];
            dst[y * a->width + x] = (alpha << 24) | (alpha << 16)
                                  | (alpha <<  8) |  alpha;
         }
      }
   }

   /* rowLength is bytes per source row, not pixels. Atlas staging is
    * packed RGBA8 = 4 bytes per pixel. */
   src_buf.addr        = dkMemBlockGetGpuAddr(f->cpu_buf);
   src_buf.rowLength   = a->width * 4;
   src_buf.imageHeight = 0;

   dkImageViewDefaults(&dst_view, &f->atlas_img.image);
   dst_rect.x = dst_rect.y = dst_rect.z = 0;
   dst_rect.width  = a->width;
   dst_rect.height = a->height;
   dst_rect.depth  = 1;

   dkCmdBufCopyBufferToImage(cmd, &src_buf, &dst_view, &dst_rect, 0);
   dkCmdBufBarrier(cmd, DkBarrier_Fragments, DkInvalidateFlags_Image);

   f->atlas_uploaded   = true;
   f->atlas->dirty     = false;
}

/* Sum the advance_x of every glyph in a string. Mirrors the lightweight
 * width pass in switch_font_render_line. */
static int dk3d_font_message_width_chars(dk3d_font_t *f,
      const char *msg, size_t msg_len)
{
   const char *scan      = msg;
   const char *scan_end  = msg + msg_len;
   const struct font_glyph *fallback =
      f->font_driver->get_glyph(f->font_data, '?');
   int total = 0;
   while (scan < scan_end)
   {
      const struct font_glyph *g;
      uint32_t code = utf8_walk(&scan);
      if (!(g = f->font_driver->get_glyph(f->font_data, code)))
         if (!(g = fallback))
            continue;
      total += g->advance_x;
   }
   return total;
}

static int dk3d_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   if (!f) return 0;
   return (int)(dk3d_font_message_width_chars(f, msg, msg_len) * scale);
}

static const struct font_glyph *dk3d_font_get_glyph(void *data,
      uint32_t code)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   if (!f || !f->font_driver) return NULL;
   return f->font_driver->get_glyph(f->font_data, code);
}

static bool dk3d_font_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   if (!f || !f->font_driver || !f->font_driver->get_line_metrics)
      return false;
   f->font_driver->get_line_metrics(f->font_data, metrics);
   return true;
}

/* Walk text and emit per-glyph textured quads through the menu 3D
 * pipeline. Uses the same shaders as gfx_display_dk3d_draw (the atlas
 * acts as the bound texture, the per-vertex color is the requested text
 * color); blend state is overridden to premultiplied alpha to match the
 * (a,a,a,a) atlas content. */
static void dk3d_font_render_line(dk3d_font_t *f, dk3d_t *dk3d,
      const char *msg, size_t msg_len, float scale,
      float pos_x, float pos_y, unsigned text_align,
      const float color[4])
{
   dk3d_frame_t          *fr   = &dk3d->frames[dk3d->current_frame];
   dk3d_menu_pipeline_t  *mp   = &dk3d->menu_pipe;
   DkCmdBuf               cmd  = fr->cmdbuf;
   const struct font_glyph *fallback;
   const char            *scan, *scan_end;
   int                    base_x, base_y, delta_x = 0, delta_y = 0;
   uint32_t               fb_w = dk3d->surface_width;
   uint32_t               fb_h = dk3d->surface_height;
   uint32_t               atlas_w, atlas_h;
   uint32_t               vstride, uoff, tex_idx;
   uint32_t               glyph_count = 0;
   uint32_t               vbo_base_off;
   float                 *vbo_base_ptr;
   DkImageView            view;
   DkImageDescriptor     *img_heap;
   DkResHandle            handle;
   DkShader const        *shaders[2];
   DkVtxBufferState       vbuf_state;
   DkRasterizerState      rs;
   DkDepthStencilState    ds;
   DkColorState           cs;
   DkColorWriteState      cws;
   DkBlendState           bs;
   DkViewport             vp;
   DkScissor              sc;
   const DkVtxAttribState attribs[3] = {
      { .bufferId = 0, .offset =  0, .size = DkVtxAttribSize_2x32, .type = DkVtxAttribType_Float },
      { .bufferId = 0, .offset =  8, .size = DkVtxAttribSize_2x32, .type = DkVtxAttribType_Float },
      { .bufferId = 0, .offset = 16, .size = DkVtxAttribSize_4x32, .type = DkVtxAttribType_Float },
   };

   if (dk3d->acquired_slot < 0)
      return;
   if (!f->atlas || f->atlas->width == 0 || f->atlas->height == 0)
      return;

   /* Make sure atlas is up-to-date before we sample it. */
   if (!f->atlas_uploaded || f->atlas->dirty)
      dk3d_font_upload_atlas(f, cmd);

   /* Normalize UVs against the GPU image dimensions (f->atlas_w/_h, set
    * to MAX_W/H in dk3d_font_alloc_atlas), NOT f->atlas->width/height
    * which is RA's currently-packed sub-region inside the larger image. */
   atlas_w = f->atlas_w;
   atlas_h = f->atlas_h;

   base_x = (int)(pos_x * fb_w);
   base_y = (int)((1.0f - pos_y) * fb_h);

   if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
   {
      int width_px = (int)(dk3d_font_message_width_chars(f, msg, msg_len) * scale);
      if (text_align == TEXT_ALIGN_RIGHT)
         base_x -= width_px;
      else
         base_x -= width_px / 2;
   }

   /* Bump-allocate worst-case VBO slice (6 verts per character). The
    * exact glyph count after clipping is unknown ahead of time; we'll
    * use only what we fill and let the rest go unused. */
   vstride      = 8 * sizeof(float);
   {
      uint32_t worst_bytes = (uint32_t)msg_len * 6 * vstride;
      vbo_base_off = dk3d_menu_bump(&fr->menu_vbo_off, fr->menu_vbo_cap,
            worst_bytes, 16);
   }
   if (vbo_base_off == UINT32_MAX)
      return;
   vbo_base_ptr = (float*)((uint8_t*)fr->menu_vbo_ptr + vbo_base_off);

   fallback = f->font_driver->get_glyph(f->font_data, '?');
   scan     = msg;
   scan_end = msg + msg_len;
   while (scan < scan_end)
   {
      const struct font_glyph *g;
      int    gx, gy, gw, gh;
      int    sx, sy;
      float  x0, y0, x1, y1, u0, v0, u1, v1;
      float *v;
      uint32_t code = utf8_walk(&scan);

      if (!(g = f->font_driver->get_glyph(f->font_data, code)))
         if (!(g = fallback))
            continue;

      /* Scale glyph quad dimensions and offsets per `scale` (matches
       * gl2.c's GL_RASTER_FONT_EMIT macro). Atlas content stays at
       * native size; the linear sampler interpolates when scale > 1.0,
       * giving smooth scaling instead of integer-pixel reads. */
      gw = (int)(g->width  * scale);
      gh = (int)(g->height * scale);
      if (gw <= 0 || gh <= 0)
      {
         delta_x += g->advance_x;
         delta_y += g->advance_y;
         continue;
      }

      gx = base_x + (int)(g->draw_offset_x * scale) + (int)(delta_x * scale);
      gy = base_y + (int)(g->draw_offset_y * scale) + (int)(delta_y * scale);
      if (gx >= (int)fb_w || gy >= (int)fb_h
            || gx + gw <= 0 || gy + gh <= 0)
      {
         delta_x += g->advance_x;
         delta_y += g->advance_y;
         continue;
      }

      sx = g->atlas_offset_x;
      sy = g->atlas_offset_y;

      /* Clip-space coords. Menu MVP is ortho(0,1, 0,1, -1,1) over the
       * fb-sized viewport, and the device has OriginLowerLeft — so the
       * MVP's NDC [0..1] maps with y=1 at the *top* of the display. The
       * glyph layout above is in display-y-from-top; flip Y here so
       * (y0 = top of glyph) gets the higher clip y value.
       *
       * x0,y0 is the display-top-left corner of the glyph;
       * x1,y1 is the display-bottom-right. After the flip,
       * y0 > y1 in the value sense (top has higher clip y).
       *
       * UVs are top-left origin (atlas natural convention). */
      x0 = (float)gx        / (float)fb_w;
      x1 = (float)(gx + gw) / (float)fb_w;
      y0 = 1.0f - (float)gy        / (float)fb_h;
      y1 = 1.0f - (float)(gy + gh) / (float)fb_h;

      /* UVs sample the atlas at its NATIVE glyph size (g->width/height),
       * not the scaled-up quad dimensions, so the linear sampler can
       * interpolate atlas pixels into the larger quad. */
      u0 = (float)sx              / (float)atlas_w;
      u1 = (float)(sx + g->width) / (float)atlas_w;
      v0 = (float)sy               / (float)atlas_h;
      v1 = (float)(sy + g->height) / (float)atlas_h;

      v = vbo_base_ptr + glyph_count * 6 * 8;
      /* TL */
      v[ 0] = x0; v[ 1] = y0; v[ 2] = u0; v[ 3] = v0;
      v[ 4] = color[0]; v[ 5] = color[1]; v[ 6] = color[2]; v[ 7] = color[3];
      /* TR */
      v[ 8] = x1; v[ 9] = y0; v[10] = u1; v[11] = v0;
      v[12] = color[0]; v[13] = color[1]; v[14] = color[2]; v[15] = color[3];
      /* BL */
      v[16] = x0; v[17] = y1; v[18] = u0; v[19] = v1;
      v[20] = color[0]; v[21] = color[1]; v[22] = color[2]; v[23] = color[3];
      /* BL again */
      v[24] = x0; v[25] = y1; v[26] = u0; v[27] = v1;
      v[28] = color[0]; v[29] = color[1]; v[30] = color[2]; v[31] = color[3];
      /* TR again */
      v[32] = x1; v[33] = y0; v[34] = u1; v[35] = v0;
      v[36] = color[0]; v[37] = color[1]; v[38] = color[2]; v[39] = color[3];
      /* BR */
      v[40] = x1; v[41] = y1; v[42] = u1; v[43] = v1;
      v[44] = color[0]; v[45] = color[1]; v[46] = color[2]; v[47] = color[3];
      glyph_count++;

      delta_x += g->advance_x;
      delta_y += g->advance_y;
   }

   if (glyph_count == 0)
      return;

   /* Bump-allocate UBO slice for the MVP — reuse mp->mvp_no_rot. */
   uoff = dk3d_menu_bump(&fr->menu_ubo_off, fr->menu_ubo_cap,
         (uint32_t)sizeof(math_matrix_4x4), DK_UNIFORM_BUF_ALIGNMENT);
   if (uoff == UINT32_MAX)
      return;
   memcpy((uint8_t*)fr->menu_ubo_ptr + uoff, &mp->mvp_no_rot,
         sizeof(math_matrix_4x4));

   /* Per-frame image descriptor slot for the atlas. */
   if (fr->menu_img_desc_next >= DK3D_MENU_IMAGE_DESCS)
      return;
   img_heap = (DkImageDescriptor*)fr->menu_img_desc_ptr;
   dkImageViewDefaults(&view, &f->atlas_img.image);
   dkImageDescriptorInitialize(&img_heap[fr->menu_img_desc_next], &view, false, false);
   tex_idx = fr->menu_img_desc_next++;
   handle  = dkMakeTextureHandle(tex_idx, 0);

   /* Pipeline state. Mirror gfx_display_dk3d_draw, but with premult
    * blend factors so an (a,a,a,a) atlas multiplied by vColor composes
    * correctly into the swapchain. */
   shaders[0] = &mp->vsh;
   shaders[1] = &mp->fsh;
   dkCmdBufBindShaders(cmd, DkStageFlag_GraphicsMask, shaders, 2);

   dkCmdBufBindVtxAttribState(cmd, attribs, 3);
   vbuf_state.stride  = vstride;
   vbuf_state.divisor = 0;
   dkCmdBufBindVtxBufferState(cmd, &vbuf_state, 1);
   dkCmdBufBindVtxBuffer(cmd, 0, fr->menu_vbo_gpu + vbo_base_off,
         glyph_count * 6 * vstride);
   dkCmdBufBindUniformBuffer(cmd, DkStage_Vertex, 0,
         fr->menu_ubo_gpu + uoff, sizeof(math_matrix_4x4));

   dkCmdBufBindImageDescriptorSet(cmd, fr->menu_img_desc_gpu, DK3D_MENU_IMAGE_DESCS);
   dkCmdBufBindSamplerDescriptorSet(cmd, mp->sampler_desc_gpu, 1);
   dkCmdBufBindTextures(cmd, DkStage_Fragment, 0, &handle, 1);

   dkRasterizerStateDefaults(&rs);
   rs.cullMode = DkFace_None;
   dkCmdBufBindRasterizerState(cmd, &rs);

   dkDepthStencilStateDefaults(&ds);
   ds.depthTestEnable  = false;
   ds.depthWriteEnable = false;
   dkCmdBufBindDepthStencilState(cmd, &ds);

   dkColorStateDefaults(&cs);
   dkColorStateSetBlendEnable(&cs, 0, true);
   dkCmdBufBindColorState(cmd, &cs);

   dkColorWriteStateDefaults(&cws);
   dkColorWriteStateSetMask(&cws, 0, DkColorMask_RGBA);
   dkCmdBufBindColorWriteState(cmd, &cws);

   dkBlendStateDefaults(&bs);
   /* Premultiplied: src is (color.rgb * atlas_alpha, color.a * atlas_alpha). */
   bs.colorBlendOp        = DkBlendOp_Add;
   bs.alphaBlendOp        = DkBlendOp_Add;
   bs.srcColorBlendFactor = DkBlendFactor_One;
   bs.dstColorBlendFactor = DkBlendFactor_InvSrcAlpha;
   bs.srcAlphaBlendFactor = DkBlendFactor_One;
   bs.dstAlphaBlendFactor = DkBlendFactor_InvSrcAlpha;
   dkCmdBufBindBlendStates(cmd, 0, &bs, 1);

   /* Viewport = whole framebuffer (we already projected into NDC). */
   vp.x = 0.0f; vp.y = 0.0f;
   vp.width  = (float)fb_w;
   vp.height = (float)fb_h;
   vp.near   = 0.0f;
   vp.far    = 1.0f;
   dkCmdBufSetViewports(cmd, 0, &vp, 1);

   /* Respect ozone's current scissor (set via gfx_display_scissor_begin
    * around its item list) and flip Y to deko3d's bottom-origin under
    * OriginLowerLeft. dkCmdBufSetScissors is stateful, so we always set
    * it here to either ozone's rect or full-fb to avoid carrying over a
    * previous draw's scissor. */
   if (mp->scissor_on)
   {
      sc.x      = mp->scissor_x;
      sc.y      = (mp->scissor_y + mp->scissor_h <= fb_h)
                ? (fb_h - mp->scissor_y - mp->scissor_h)
                : 0;
      sc.width  = mp->scissor_w;
      sc.height = mp->scissor_h;
   }
   else
   {
      sc.x = 0; sc.y = 0;
      sc.width  = fb_w;
      sc.height = fb_h;
   }
   dkCmdBufSetScissors(cmd, 0, &sc, 1);

   dkCmdBufDraw(cmd, DkPrimitive_Triangles, glyph_count * 6, 1, 0, 0);
}

static void dk3d_font_render_msg(void *userdata, void *data,
      const char *msg, const struct font_params *params)
{
   dk3d_t      *dk3d = (dk3d_t*)userdata;
   dk3d_font_t *f    = (dk3d_font_t*)data;
   float pos_x, pos_y, scale;
   float color[4];
   unsigned align;

   if (!f || !dk3d || !msg || !*msg)
      return;
   if (dk3d->acquired_slot < 0)
      return;

   if (params)
   {
      uint32_t c = params->color;
      pos_x = params->x;
      pos_y = params->y;
      scale = params->scale;
      align = params->text_align;
      /* font_params::color packs as `(R<<24)|(G<<16)|(B<<8)|A` per the
       * FONT_COLOR_RGBA macro in video_defines.h — R is the HIGH byte,
       * A is the LOW byte. (The struct comment "ABGR" is misleading.) */
      color[0] = (float)((c >> 24) & 0xFF) / 255.0f;
      color[1] = (float)((c >> 16) & 0xFF) / 255.0f;
      color[2] = (float)((c >>  8) & 0xFF) / 255.0f;
      color[3] = (float)((c      ) & 0xFF) / 255.0f;
      /* color == 0 (caller didn't set) → opaque white. */
      if (c == 0)
      {
         color[0] = color[1] = color[2] = color[3] = 1.0f;
      }
   }
   else
   {
      pos_x = 0.0f;
      pos_y = 0.0f;
      scale = 1.0f;
      align = TEXT_ALIGN_LEFT;
      color[0] = color[1] = color[2] = color[3] = 1.0f;
   }

   /* Split on '\n' so multi-line messages (e.g. statistics_show's
    * CORE AV_INFO block) render with each line offset by the font's
    * line height. Mirrors switch_font_render_msg's loop. pos_y is in
    * bottom-origin normalized coords, so subsequent lines decrease pos_y. */
   {
      struct font_line_metrics *lm = NULL;
      float line_height = 0.0f;
      const char *start = msg;
      const char *scan  = msg;
      int lines = 0;

      if (f->font_driver->get_line_metrics)
         f->font_driver->get_line_metrics(f->font_data, &lm);
      if (lm && lm->height > 0.0f)
         line_height = scale / lm->height;

      for (;;)
      {
         if (*scan == '\n' || *scan == '\0')
         {
            size_t line_len = (size_t)(scan - start);
            if (line_len > 0)
               dk3d_font_render_line(f, dk3d, start, line_len, scale,
                     pos_x, pos_y - (float)lines * line_height,
                     align, color);
            if (*scan == '\0')
               break;
            start = ++scan;
            lines++;
         }
         else
            scan++;
      }
   }
}

font_renderer_t deko3d_font = {
   dk3d_font_init,
   dk3d_font_free,
   dk3d_font_render_msg,
   "deko3d",
   dk3d_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   dk3d_font_get_message_width,
   dk3d_font_get_line_metrics
};

/* ====================================================================== *
 *  gfx_display ctx — wires Ozone / XMB / MaterialUI to deko3d.
 *
 *  Pipeline: per-frame bump-allocate an interleaved (pos,uv,color) VBO
 *  slice + a 64B UBO slice + an image descriptor slot, then issue a
 *  TriangleStrip draw using the textured-color-quad shaders compiled at
 *  build time from deko3d_menu_{vsh,fsh}.glsl.
 * ====================================================================== */

/* Defaults used when the caller doesn't supply per-vertex arrays. Sized
 * for 4 vertices (unit quad); the draw() function fills extra vertices
 * with constants (matches gfx_display_vk_draw's behavior). */
static const float dk3d_menu_default_vertices[] = {
   0.0f, 0.0f,
   1.0f, 0.0f,
   0.0f, 1.0f,
   1.0f, 1.0f,
};

static const float dk3d_menu_default_tex_coords[] = {
   0.0f, 0.0f,
   1.0f, 0.0f,
   0.0f, 1.0f,
   1.0f, 1.0f,
};

static const float dk3d_menu_default_colors[] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *gfx_display_dk3d_get_default_mvp(void *data)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d)
      return NULL;
   return &dk3d->menu_pipe.mvp_no_rot;
}

static const float *gfx_display_dk3d_get_default_vertices(void)
{
   return dk3d_menu_default_vertices;
}

static const float *gfx_display_dk3d_get_default_tex_coords(void)
{
   return dk3d_menu_default_tex_coords;
}

static void gfx_display_dk3d_blend_begin(void *data)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (dk3d) dk3d->menu_pipe.blend_on = true;
}

static void gfx_display_dk3d_blend_end(void *data)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (dk3d) dk3d->menu_pipe.blend_on = false;
}

static void gfx_display_dk3d_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d) return;
   dk3d->menu_pipe.scissor_on = true;
   dk3d->menu_pipe.scissor_x  = (x < 0) ? 0 : x;
   dk3d->menu_pipe.scissor_y  = (y < 0) ? 0 : y;
   dk3d->menu_pipe.scissor_w  = width;
   dk3d->menu_pipe.scissor_h  = height;
}

static void gfx_display_dk3d_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (dk3d) dk3d->menu_pipe.scissor_on = false;
}

/* HAVE_SHADERPIPELINE menu effects (XMB ribbons / snow). Not implemented
 * for deko3d yet — those pipelines require additional shader compilation
 * and per-pipeline UBO layouts. Stubbed to no-op so Ozone (which never
 * uses these) and XMB-with-no-effects still render. */
static void gfx_display_dk3d_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   (void)draw; (void)p_disp; (void)data;
   (void)video_width; (void)video_height;
}

/* Bump-allocate `size` bytes from a per-frame ring memblock with `align`
 * boundary. Returns offset on success, UINT32_MAX on overflow. Caller
 * advances the bump pointer on success. */
static uint32_t dk3d_menu_bump(uint32_t *off, uint32_t cap,
      uint32_t size, uint32_t align)
{
   uint32_t aligned = (*off + align - 1) & ~(align - 1);
   if (aligned + size > cap)
      return UINT32_MAX;
   *off = aligned + size;
   return aligned;
}

static void gfx_display_dk3d_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   dk3d_t                *dk3d;
   dk3d_frame_t          *fr;
   dk3d_menu_pipeline_t  *mp;
   DkCmdBuf               cmd;
   const float           *vert_src;
   const float           *tc_src;
   const float           *color_src;
   bool                   use_default_tc;
   bool                   use_default_color;
   size_t                 n_verts;
   uint32_t               vstride, vsize, voff;
   uint32_t               uoff;
   uint32_t               tex_idx;
   float                 *vbo;
   const DkImage         *tex;
   DkImageView            view;
   DkImageDescriptor     *img_heap;
   DkResHandle            handle;
   DkShader const *shaders[2];
   const DkVtxAttribState attribs[3] = {
      { .bufferId = 0, .offset =  0, .size = DkVtxAttribSize_2x32, .type = DkVtxAttribType_Float },
      { .bufferId = 0, .offset =  8, .size = DkVtxAttribSize_2x32, .type = DkVtxAttribType_Float },
      { .bufferId = 0, .offset = 16, .size = DkVtxAttribSize_4x32, .type = DkVtxAttribType_Float },
   };
   DkVtxBufferState       vbuf_state;
   DkRasterizerState      rs;
   DkDepthStencilState    ds;
   DkColorState           cs;
   DkColorWriteState      cws;
   DkBlendState           bs;
   DkViewport             vp;
   DkScissor              sc;

   dk3d = (dk3d_t*)data;
   if (!dk3d || !draw || !draw->coords)
      return;
   if (dk3d->acquired_slot < 0)
      return;
   n_verts = draw->coords->vertices;
   if (n_verts == 0)
      return;

   fr  = &dk3d->frames[dk3d->current_frame];
   mp  = &dk3d->menu_pipe;
   cmd = fr->cmdbuf;

   /* Vertex source arrays with fallback to defaults. */
   vert_src          = draw->coords->vertex    ? draw->coords->vertex    : dk3d_menu_default_vertices;
   tc_src            = draw->coords->tex_coord ? draw->coords->tex_coord : dk3d_menu_default_tex_coords;
   color_src         = draw->coords->color     ? draw->coords->color     : dk3d_menu_default_colors;
   use_default_tc    = (tc_src    == dk3d_menu_default_tex_coords);
   use_default_color = (color_src == dk3d_menu_default_colors);

   /* Bump-allocate VBO slice. 32B per vertex (vec2 pos + vec2 uv + vec4 color). */
   vstride = 8 * sizeof(float);
   vsize   = (uint32_t)n_verts * vstride;
   voff    = dk3d_menu_bump(&fr->menu_vbo_off, fr->menu_vbo_cap, vsize, 16);
   if (voff == UINT32_MAX)
      return;
   vbo = (float*)((uint8_t*)fr->menu_vbo_ptr + voff);
   {
      size_t i;
      for (i = 0; i < n_verts; i++)
      {
         vbo[i*8 + 0] = *vert_src++;
         vbo[i*8 + 1] = *vert_src++;
         /* Flip V on the way into the VBO: ozone passes UVs in standard
          * top-origin convention (v=0 = atlas top), but our ortho MVP +
          * OriginLowerLeft puts display-bottom at the v=0 vertex. */
         if (use_default_tc && i >= 4)
         {
            vbo[i*8 + 2] = 0.0f; vbo[i*8 + 3] = 1.0f;
         }
         else
         {
            vbo[i*8 + 2] = *tc_src++;
            vbo[i*8 + 3] = 1.0f - *tc_src++;
         }
         if (use_default_color && i >= 4)
         {
            vbo[i*8 + 4] = vbo[i*8 + 5] = vbo[i*8 + 6] = vbo[i*8 + 7] = 1.0f;
         }
         else
         {
            vbo[i*8 + 4] = *color_src++;
            vbo[i*8 + 5] = *color_src++;
            vbo[i*8 + 6] = *color_src++;
            vbo[i*8 + 7] = *color_src++;
         }
      }
   }

   /* Bump-allocate UBO slice for mvp matrix. */
   uoff = dk3d_menu_bump(&fr->menu_ubo_off, fr->menu_ubo_cap,
         (uint32_t)sizeof(math_matrix_4x4), DK_UNIFORM_BUF_ALIGNMENT);
   if (uoff == UINT32_MAX)
      return;
   memcpy((uint8_t*)fr->menu_ubo_ptr + uoff,
         draw->matrix_data ? draw->matrix_data : &mp->mvp_no_rot,
         sizeof(math_matrix_4x4));

   /* Resolve texture: handle is a dk3d_image_t* allocated by
    * dk3d_load_texture; fall back to the blank 1x1 white if none. */
   tex = (draw->texture != 0)
       ? &((const dk3d_image_t*)(uintptr_t)draw->texture)->image
       : &mp->blank_tex.image;

   /* Write image descriptor into the per-frame heap. */
   if (fr->menu_img_desc_next >= DK3D_MENU_IMAGE_DESCS)
      return;
   img_heap = (DkImageDescriptor*)fr->menu_img_desc_ptr;
   dkImageViewDefaults(&view, tex);
   dkImageDescriptorInitialize(&img_heap[fr->menu_img_desc_next], &view, false, false);
   tex_idx = fr->menu_img_desc_next++;
   handle  = dkMakeTextureHandle(tex_idx, 0);

   /* State setup. dkBlendStateDefaults already emits (SRC_ALPHA,
    * INV_SRC_ALPHA, Add) — the RA-standard blend equation — so blend_on
    * only needs to toggle the per-attachment enable mask. */
   shaders[0] = &mp->vsh;
   shaders[1] = &mp->fsh;
   dkCmdBufBindShaders(cmd, DkStageFlag_GraphicsMask, shaders, 2);

   dkCmdBufBindVtxAttribState(cmd, attribs, 3);
   vbuf_state.stride  = vstride;
   vbuf_state.divisor = 0;
   dkCmdBufBindVtxBufferState(cmd, &vbuf_state, 1);
   dkCmdBufBindVtxBuffer(cmd, 0, fr->menu_vbo_gpu + voff, vsize);
   dkCmdBufBindUniformBuffer(cmd, DkStage_Vertex, 0,
         fr->menu_ubo_gpu + uoff, sizeof(math_matrix_4x4));

   dkCmdBufBindImageDescriptorSet(cmd, fr->menu_img_desc_gpu, DK3D_MENU_IMAGE_DESCS);
   dkCmdBufBindSamplerDescriptorSet(cmd, mp->sampler_desc_gpu, 1);
   dkCmdBufBindTextures(cmd, DkStage_Fragment, 0, &handle, 1);

   dkRasterizerStateDefaults(&rs);
   rs.cullMode = DkFace_None;
   dkCmdBufBindRasterizerState(cmd, &rs);

   dkDepthStencilStateDefaults(&ds);
   ds.depthTestEnable  = false;
   ds.depthWriteEnable = false;
   dkCmdBufBindDepthStencilState(cmd, &ds);

   dkColorStateDefaults(&cs);
   dkColorStateSetBlendEnable(&cs, 0, mp->blend_on);
   dkCmdBufBindColorState(cmd, &cs);

   dkColorWriteStateDefaults(&cws);
   dkColorWriteStateSetMask(&cws, 0, DkColorMask_RGBA);
   dkCmdBufBindColorWriteState(cmd, &cws);

   dkBlendStateDefaults(&bs);
   dkCmdBufBindBlendStates(cmd, 0, &bs, 1);

   /* Viewport — the gfx_display draw region within the swapchain. */
   vp.x        = (float)draw->x;
   vp.y        = (float)draw->y;
   vp.width    = (float)draw->width;
   vp.height   = (float)draw->height;
   vp.near     = 0.0f;
   vp.far      = 1.0f;
   dkCmdBufSetViewports(cmd, 0, &vp, 1);

   /* Flip scissor Y from RA's top-origin to deko3d's bottom-origin
    * under OriginLowerLeft (same as the font path). */
   if (mp->scissor_on)
   {
      sc.x      = mp->scissor_x;
      sc.y      = (mp->scissor_y + mp->scissor_h <= dk3d->surface_height)
                ? (dk3d->surface_height - mp->scissor_y - mp->scissor_h)
                : 0;
      sc.width  = mp->scissor_w;
      sc.height = mp->scissor_h;
   }
   else
   {
      sc.x      = 0;
      sc.y      = 0;
      sc.width  = dk3d->surface_width;
      sc.height = dk3d->surface_height;
   }
   dkCmdBufSetScissors(cmd, 0, &sc, 1);

   /* RA's gfx_display emits TRIANGLE_STRIP quads for all menu draws. */
   dkCmdBufDraw(cmd, DkPrimitive_TriangleStrip,
         (uint32_t)n_verts, 1, 0, 0);
}

gfx_display_ctx_driver_t gfx_display_ctx_deko3d = {
   gfx_display_dk3d_draw,
   gfx_display_dk3d_draw_pipeline,
   gfx_display_dk3d_blend_begin,
   gfx_display_dk3d_blend_end,
   gfx_display_dk3d_get_default_mvp,
   gfx_display_dk3d_get_default_vertices,
   gfx_display_dk3d_get_default_tex_coords,
   FONT_DRIVER_RENDER_DEKO3D_API,
   GFX_VIDEO_DRIVER_DEKO3D,
   "deko3d",
   false,
   gfx_display_dk3d_scissor_begin,
   gfx_display_dk3d_scissor_end
};

/* ====================================================================== *
 *  Trivial driver hooks
 * ====================================================================== */

static void dk3d_set_nonblock_state(void *data, bool toggle, bool c, unsigned d)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d)
      return;
   dk3d->vsync = !toggle;
   if (dk3d->swapchain)
      dkSwapchainSetSwapInterval(dk3d->swapchain, dk3d->vsync ? dk3d->swap_interval : 0);
}

static bool dk3d_alive(void *data)              { (void)data; return true; }
static bool dk3d_focus(void *data)              { (void)data; return true; }
static bool dk3d_suppress_screensaver(void *data, bool e) { (void)data; (void)e; return false; }
static bool dk3d_has_windowed(void *data)       { (void)data; return false; }
static bool dk3d_set_shader(void *data,
      enum rarch_shader_type t, const char *p)  { (void)data; (void)t; (void)p; return false; }
static void dk3d_set_rotation(void *data, unsigned r)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (dk3d) dk3d->rotation = r;
}
static void dk3d_viewport_info(void *data, struct video_viewport *vp)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (dk3d && vp) *vp = dk3d->vp;
}

/* RA invokes this when aspect ratio / integer scale / custom viewport /
 * surface size changes. video_driver_update_viewport honors all those
 * settings AND writes video_st->scale_width/height (what the stats panel
 * reads). Without this callback, vp stays full-surface and stats read 0. */
static void dk3d_set_viewport(void *data, unsigned vp_width,
      unsigned vp_height, bool force_full, bool allow_rotate)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d) return;
   dk3d->vp.full_width  = vp_width;
   dk3d->vp.full_height = vp_height;
   video_driver_update_viewport(&dk3d->vp, force_full,
         dk3d->keep_aspect, true);
   if (dk3d->vp.x < 0) dk3d->vp.x = 0;
   if (dk3d->vp.y < 0) dk3d->vp.y = 0;
   (void)allow_rotate;
}

/* ====================================================================== *
 *  Poke interface
 * ====================================================================== */

static void dk3d_set_aspect_ratio(void *data, unsigned aspect_idx)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d) return;
   dk3d->keep_aspect   = true;
   dk3d->should_resize = true;
   (void)aspect_idx;
}

static void dk3d_apply_state_changes(void *data) { (void)data; }

static void dk3d_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   dk3d_stage_t *stage;
   uint32_t needed;

   if (!dk3d || !frame || !width || !height)
      return;

   /* RGUI always sends RGBA4444 via the pixel format map entry. */
   if (rgb32)
      return;

   dk3d->menu_alpha = alpha;

   /* RGUI sends RGBA4444 (16 bpp).  The 2D engine maps RGBA4_Unorm to the
    * BGR5A1 surface format, so blitting it directly would reinterpret
    * the 4:4:4:4 bit fields as 5:5:5:1.  Convert to RGBA8 on the CPU
    * instead.  The GPU CopyBufferToImage is deferred to dk3d_frame
    * after dkCmdBufClear, so recording it outside the frame window
    * is safe. */
   stage  = &dk3d->menu_stage;
   needed = width * height * 4;
   if (needed > stage->cpu_capacity)
      return;

   {
      const uint16_t *src = (const uint16_t*)frame;
      uint32_t *dst       = (uint32_t*)stage->cpu_ptr;
      unsigned i;
      for (i = 0; i < width * height; i++)
      {
         uint16_t p = src[i];
         uint8_t r = ((p >> 12) & 0xf) * 17;
         uint8_t g = ((p >> 8)  & 0xf) * 17;
         uint8_t b = ((p >> 4)  & 0xf) * 17;
         uint8_t a = ( p        & 0xf) * 17;
         dst[i] = ((uint32_t)a << 24)
                | ((uint32_t)r << 16)
                | ((uint32_t)g << 8)
                |  b;
      }
   }

   dk3d_stage_mark_dirty(stage, width, height);
}

static void dk3d_set_texture_enable(void *data, bool enable, bool full_screen)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d) return;
   dk3d->menu_enable      = enable;
   dk3d->menu_full_screen = full_screen;
}

static bool dk3d_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   dk3d_t *dk3d = (dk3d_t*)data;
   if (!dk3d) return false;
   *iface = (const struct retro_hw_render_interface*)&dk3d->hw_iface;
   return true;
}

static uint32_t dk3d_get_flags(void *data)
{
   uint32_t flags = 0;
   (void)data;
   /* GFX_CTX_FLAGS_* are enum indices — BIT32_SET sets the bit at that index.
    * VIDEO_FLAG_USE_RGBA is opted in separately via video_driver_set_disp_flags
    * during init (see dk3d_init). */
   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   return flags;
}

/* Eager pixel upload via a one-shot cmdbuf, modeled on
 * dk3d_menu_upload_blank_tex. Blocks until the GPU has copied the data;
 * caller may use the image immediately after. */
static bool dk3d_upload_pixels(dk3d_t *dk3d, dk3d_image_t *tex,
      const void *pixels, uint32_t w, uint32_t h)
{
   DkMemBlockMaker mm;
   DkMemBlock      stage_mem = NULL;
   DkMemBlock      cmd_mem   = NULL;
   DkCmdBuf        cmd       = NULL;
   DkCmdBufMaker   cbm;
   DkCmdList       list;
   DkCopyBuf       src;
   DkImageView     dst_view;
   DkImageRect     dst_rect  = { 0, 0, 0, w, h, 1 };
   uint32_t        bytes     = w * h * 4;
   uint32_t        stage_size = dk3d_align_up(bytes, 0x1000);

   dkMemBlockMakerDefaults(&mm, dk3d->device, stage_size);
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   stage_mem = dkMemBlockCreate(&mm);
   if (!stage_mem)
      goto fail;
   memcpy(dkMemBlockGetCpuAddr(stage_mem), pixels, bytes);

   dkMemBlockMakerDefaults(&mm, dk3d->device, 0x1000);
   mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   cmd_mem = dkMemBlockCreate(&mm);
   if (!cmd_mem)
      goto fail;

   dkCmdBufMakerDefaults(&cbm, dk3d->device);
   cmd = dkCmdBufCreate(&cbm);
   if (!cmd)
      goto fail;
   dkCmdBufAddMemory(cmd, cmd_mem, 0, 0x1000);

   src.addr        = dkMemBlockGetGpuAddr(stage_mem);
   src.rowLength   = w * 4;
   src.imageHeight = 0;
   dkImageViewDefaults(&dst_view, &tex->image);
   dkCmdBufCopyBufferToImage(cmd, &src, &dst_view, &dst_rect, 0);

   list = dkCmdBufFinishList(cmd);
   dkQueueSubmitCommands(dk3d->queue, list);
   dkQueueWaitIdle(dk3d->queue);

   dkCmdBufDestroy(cmd);
   dkMemBlockDestroy(cmd_mem);
   dkMemBlockDestroy(stage_mem);
   return true;

fail:
   if (cmd)       dkCmdBufDestroy(cmd);
   if (cmd_mem)   dkMemBlockDestroy(cmd_mem);
   if (stage_mem) dkMemBlockDestroy(stage_mem);
   return false;
}

static uintptr_t dk3d_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   dk3d_t               *dk3d  = (dk3d_t*)video_data;
   struct texture_image *image = (struct texture_image*)data;
   dk3d_image_t         *tex;
   (void)threaded;
   (void)filter_type;

   if (!dk3d || !image || !image->pixels || !image->width || !image->height)
      return 0;

   tex = (dk3d_image_t*)calloc(1, sizeof(*tex));
   if (!tex)
      return 0;

   if (!dk3d_create_image_2d(dk3d->device,
            image->width, image->height,
            DkImageFormat_RGBA8_Unorm,
            DkImageFlags_Usage2DEngine | DkImageFlags_UsageLoadStore,
            tex))
   {
      free(tex);
      return 0;
   }

   if (!dk3d_upload_pixels(dk3d, tex, image->pixels,
            image->width, image->height))
   {
      dk3d_destroy_image(tex);
      free(tex);
      return 0;
   }

   return (uintptr_t)tex;
}

static void dk3d_unload_texture(void *data, bool threaded, uintptr_t handle)
{
   dk3d_t       *dk3d = (dk3d_t*)data;
   dk3d_image_t *tex  = (dk3d_image_t*)handle;
   (void)threaded;

   if (!dk3d || !tex)
      return;

   /* Wait for the queue to drain so we don't free the image while
    * cmdbufs that reference it are still in flight. */
   dkQueueWaitIdle(dk3d->queue);
   dk3d_destroy_image(tex);
   free(tex);
}

static const video_poke_interface_t dk3d_poke_interface = {
   dk3d_get_flags,
   dk3d_load_texture,
   dk3d_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   dk3d_set_aspect_ratio,
   dk3d_apply_state_changes,
   dk3d_set_texture_frame,
   dk3d_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   dk3d_get_hw_render_interface,
   NULL, NULL, NULL, NULL, NULL  /* HDR */
};

static void dk3d_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &dk3d_poke_interface;
}

/* ====================================================================== *
 *  video_driver_t
 * ====================================================================== */

#ifdef HAVE_GFX_WIDGETS
static bool dk3d_gfx_widgets_enabled(void *data) { (void)data; return true; }
#endif

video_driver_t video_deko3d = {
   dk3d_init,
   dk3d_frame,
   dk3d_set_nonblock_state,
   dk3d_alive,
   dk3d_focus,
   dk3d_suppress_screensaver,
   dk3d_has_windowed,
   dk3d_set_shader,
   dk3d_free,
   "deko3d",
   dk3d_set_viewport,
   dk3d_set_rotation,
   dk3d_viewport_info,
   NULL,                  /* read_viewport */
   NULL,                  /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,                  /* get_overlay_interface */
#endif
   dk3d_get_poke_interface,
   NULL,                  /* wrap_type_to_enum */
   NULL,                  /* shader_load_begin */
   NULL,                  /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   dk3d_gfx_widgets_enabled
#endif
};
