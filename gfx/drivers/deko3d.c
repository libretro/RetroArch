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

/* deko3d: the Switch's native GPU API.
 *
 * One queue, a ring of frame slots each owning a command buffer, a fence
 * and per-frame memory (vertices, the menu's matrix, upload staging, and
 * what it retires). A slot is waited right after the frame before it is
 * submitted, so the slot being recorded is always free: uploads made
 * between frames are appended to its command buffer ahead of the frame
 * that follows, and nothing on the frame path waits for the GPU but that
 * one fence.
 *
 * Software frames are blitted by the 2D engine straight out of the
 * memory they were written to - a pitch-linear image in the core's own
 * pixel format, lent to the core, to the threaded wrapper as its frame
 * memory, or a ring slot a frame is copied into once. Hardware cores
 * hand over a DkImage blitted the same way. The menu, widgets and text
 * draw with one 3D pipeline whose state is bound once a frame. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <deko3d.h>
#include <switch.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <encodings/utf.h>
#include <gfx/math/matrix_4x4.h>
#include <libretro_deko3d.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"
#include "../video_driver.h"
#include "../gfx_display.h"
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#define DK3D_MAX_SWAPCHAIN  4
#define DK3D_MAX_FRAMES     3
/* One more than can be in flight, and one lent: a free one always */
#define DK3D_SW_SLOTS       (DK3D_MAX_FRAMES + 2)
#define DK3D_FRAME_MEM_MAX  8
#define DK3D_CMDMEM_SIZE    (256 * 1024)
#define DK3D_VBO_SIZE       (1024 * 1024)
#define DK3D_STAGE_SIZE     (4 * 1024 * 1024)
#define DK3D_HEAP_BLOCK     (16 * 1024 * 1024)
#define DK3D_HEAP_BLOCKS    16
#define DK3D_DESC_COUNT     1024
#define DK3D_VTX_FLOATS     8

/* Image descriptors with a fixed owner; the rest are textures' */
#define DK3D_DESC_BLANK     0
#define DK3D_DESC_RGUI      1
#define DK3D_DESC_FIRST_TEX (DK3D_DESC_RGUI + DK3D_SW_SLOTS)

#define DK3D_SAMPLER_LINEAR  0
#define DK3D_SAMPLER_NEAREST 1

#define DK3D_ALIGN(v, a) (((v) + ((a) - 1)) & ~((a) - 1))

enum dk3d_blend
{
   DK3D_BLEND_UNSET = 0,
   DK3D_BLEND_OFF,
   DK3D_BLEND_ALPHA,
   DK3D_BLEND_PREMULT
};

enum dk3d_src_kind
{
   DK3D_SRC_NONE = 0,
   DK3D_SRC_LINEAR,
   DK3D_SRC_HW
};

/* A free range of a heap block */
typedef struct dk3d_range
{
   struct dk3d_range *next;
   uint32_t off;
   uint32_t size;
} dk3d_range_t;

typedef struct dk3d_heap_block
{
   DkMemBlock    mem;
   dk3d_range_t *free;
} dk3d_heap_block_t;

/* A texture: its image in a heap block (or a block of its own when it
 * is larger than one), its descriptor and its sampler */
typedef struct dk3d_tex
{
   DkImage    image;
   DkMemBlock own;
   uint32_t   off;
   uint32_t   size;
   uint16_t   block;
   uint16_t   desc;
   uint8_t    sampler;
} dk3d_tex_t;

/* What a frame slot lets go of once its fence has passed */
typedef struct dk3d_retire
{
   DkMemBlock  mem;
   dk3d_tex_t *tex;
} dk3d_retire_t;

typedef struct dk3d_frame
{
   DkFence        done;
   uint64_t       serial;
   DkCmdBuf       cmdbuf;
   DkMemBlock     cmd_mem;
   DkMemBlock     vbo_mem;
   DkMemBlock     ubo_mem;
   DkMemBlock     stage_mem;
   dk3d_retire_t *retire;
   uint8_t       *vbo_cpu;
   uint8_t       *stage_cpu;
   DkGpuAddr      vbo_gpu;
   DkGpuAddr      ubo_gpu;
   DkGpuAddr      stage_gpu;
   uint32_t       vbo_off;
   uint32_t       stage_off;
   unsigned       retire_count;
   unsigned       retire_cap;
   bool           done_valid;
} dk3d_frame_t;

/* CPU-written memory the GPU reads in place: a software frame ring
 * slot, a RGUI slot, or the threaded wrapper's frame memory */
typedef struct dk3d_linear
{
   uint64_t   use_serial;
   DkImage    image;          /* RGUI slots: the image their descriptor names */
   DkMemBlock mem;
   uint8_t   *cpu;
   uint32_t   size;
   uint32_t   pitch;
   unsigned   dims;
   uint8_t    bpp;
   bool       used;
} dk3d_linear_t;

/* A pitch-linear blit source */
typedef struct dk3d_src
{
   DkMemBlock mem;
   uint32_t   offset;
   uint32_t   pitch;
   unsigned   dims;
   unsigned   x;
} dk3d_src_t;

typedef struct dk3d
{
   dk3d_frame_t      frames[DK3D_MAX_FRAMES];
   dk3d_linear_t     sw[DK3D_SW_SLOTS];
   dk3d_linear_t     rgui[DK3D_SW_SLOTS];
   dk3d_linear_t     frame_mem[DK3D_FRAME_MEM_MAX];
   dk3d_heap_block_t heap[DK3D_HEAP_BLOCKS];
   DkImage           sc_images[DK3D_MAX_SWAPCHAIN];
   DkMemBlock        sc_mem[DK3D_MAX_SWAPCHAIN];
   const DkImage    *sc_ptrs[DK3D_MAX_SWAPCHAIN];
   DkFence           hw_acquire;
   DkFence           last_done;
   DkImage           hw_img;
   DkShader          vsh;
   DkShader          fsh;
   struct retro_hw_render_interface_deko3d hw_iface;
   struct video_viewport vp;
   math_matrix_4x4   mvp_default;
   float             last_mvp[16];
   dk3d_src_t        last_src;
   dk3d_tex_t        blank;
   uint16_t          desc_free[DK3D_DESC_COUNT];
   NWindow          *win;
   DkDevice          device;
   DkQueue           queue;
   DkSwapchain       swapchain;
   DkMemBlock        shader_mem;
   DkMemBlock        desc_mem;
   DkMemBlock        sampler_mem;
   DkImageDescriptor *desc_cpu;
   uint64_t          serial;
   uint64_t          done_serial;
   DkGpuAddr         desc_gpu;
   DkGpuAddr         sampler_gpu;
   unsigned          surface_dims;
   unsigned          last_vp_pos;
   unsigned          last_vp_dims;
   unsigned          last_sc_pos;
   unsigned          last_sc_dims;
   unsigned          scissor_pos;
   unsigned          scissor_dims;
   unsigned          hw_pos;
   unsigned          hw_dims;
   unsigned          num_swapchain;
   unsigned          num_frames;
   unsigned          cur;
   unsigned          swap_interval;
   unsigned          hard_sync_frames;
   unsigned          desc_free_count;
   int               op_mode;
   int               sw_last;
   int               sw_lent;
   int               rgui_last;
   int               blend;
   int               src_kind;
   float             menu_alpha;
   bool              vsync;
   bool              smooth;
   bool              rgb32;
   bool              keep_aspect;
   bool              should_resize;
   bool              hard_sync;
   bool              hw_bottom_left;
   bool              hw_has_image;
   bool              hw_has_acquire;
   bool              menu_enable;
   bool              menu_full_screen;
   bool              scissor_on;
   bool              draw_bound;
   bool              mvp_valid;
   bool              pending_barrier;
   bool              recording;
} dk3d_t;

typedef struct dk3d_font
{
   const font_renderer_driver_t *drv;
   void                         *data;
   struct font_atlas            *atlas;
   dk3d_t                       *dk;
   dk3d_tex_t                   *tex;
   unsigned                      tex_dims;
} dk3d_font_t;

/* The menu shaders, GLSL compiled by uam and embedded by bin2s */
extern const uint8_t deko3d_menu_vsh_dksh[];
extern const uint8_t deko3d_menu_vsh_dksh_end[];
extern const uint8_t deko3d_menu_fsh_dksh[];
extern const uint8_t deko3d_menu_fsh_dksh_end[];

/* (pos, uv, color) as the menu vertex shader reads them */
static const DkVtxAttribState dk3d_menu_attribs[3] = {
   { 0, 0,  0, DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0 },
   { 0, 0,  8, DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0 },
   { 0, 0, 16, DkVtxAttribSize_4x32, DkVtxAttribType_Float, 0 }
};

static const float dk3d_default_vertices[8] = {
   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
};
static const float dk3d_default_tex_coords[8] = {
   0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f
};
static const float dk3d_default_colors[16] = {
   1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

static gfx_ctx_driver_t dk3d_gfx_ctx;
static font_renderer_t  deko3d_font;

/* ---- memory ----------------------------------------------------------- */

static void dk3d_debug_cb(void *user, const char *context,
      DkResult result, const char *message)
{
   (void)user;
   RARCH_ERR("[deko3d] %s: %d %s\n", context ? context : "",
         (int)result, message ? message : "");
}

static DkMemBlock dk3d_mem_new(DkDevice device, uint32_t size,
      uint32_t flags)
{
   DkMemBlockMaker mm;
   dkMemBlockMakerDefaults(&mm, device, DK3D_ALIGN(size, DK_MEMBLOCK_ALIGNMENT));
   mm.flags = flags;
   return dkMemBlockCreate(&mm);
}

/* Parks @mem or @tex on the slot being recorded: let go of once its
 * fence has passed, by which time no frame can read them */
static void dk3d_retire(dk3d_t *dk, DkMemBlock mem, dk3d_tex_t *tex)
{
   dk3d_frame_t *f = &dk->frames[dk->cur];
   if (f->retire_count == f->retire_cap)
   {
      unsigned       cap = f->retire_cap ? f->retire_cap * 2 : 16;
      dk3d_retire_t *r   = (dk3d_retire_t*)realloc(f->retire,
            cap * sizeof(*r));
      if (!r)
      {
         /* Nowhere to park it: the queue is drained instead */
         dkQueueWaitIdle(dk->queue);
         if (mem)
            dkMemBlockDestroy(mem);
         if (tex)
            free(tex);
         return;
      }
      f->retire     = r;
      f->retire_cap = cap;
   }
   f->retire[f->retire_count].mem = mem;
   f->retire[f->retire_count].tex = tex;
   f->retire_count++;
}

/* First fit in the heap's blocks; a block is made when none has room */
static bool dk3d_heap_alloc(dk3d_t *dk, uint32_t size, uint32_t align,
      uint16_t *block, uint32_t *off)
{
   unsigned i;
   size = DK3D_ALIGN(size, 512);
   for (i = 0; i < DK3D_HEAP_BLOCKS; i++)
   {
      dk3d_heap_block_t *b = &dk->heap[i];
      dk3d_range_t **pp;
      if (!b->mem)
      {
         if (!(b->mem = dk3d_mem_new(dk->device, DK3D_HEAP_BLOCK,
                     DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)))
            return false;
         if (!(b->free = (dk3d_range_t*)malloc(sizeof(*b->free))))
            return false;
         b->free->next = NULL;
         b->free->off  = 0;
         b->free->size = DK3D_HEAP_BLOCK;
      }
      for (pp = &b->free; *pp; pp = &(*pp)->next)
      {
         dk3d_range_t *r = *pp;
         uint32_t start  = DK3D_ALIGN(r->off, align);
         uint32_t lead   = start - r->off;
         if (lead + size > r->size)
            continue;
         if (lead)
         {
            dk3d_range_t *n = (dk3d_range_t*)malloc(sizeof(*n));
            if (!n)
               return false;
            n->off    = start;
            n->size   = r->size - lead;
            n->next   = r->next;
            r->size   = lead;
            r->next   = n;
            pp        = &r->next;
            r         = n;
         }
         r->off  += size;
         r->size -= size;
         if (!r->size)
         {
            *pp = r->next;
            free(r);
         }
         *block = (uint16_t)i;
         *off   = start;
         return true;
      }
   }
   return false;
}

static void dk3d_heap_free(dk3d_t *dk, uint16_t block, uint32_t off,
      uint32_t size)
{
   dk3d_heap_block_t *b = &dk->heap[block];
   dk3d_range_t **pp    = &b->free;
   dk3d_range_t *prev   = NULL;
   dk3d_range_t *n;
   size = DK3D_ALIGN(size, 512);
   while (*pp && (*pp)->off < off)
   {
      prev = *pp;
      pp   = &(*pp)->next;
   }
   /* Joined to its neighbours where they touch */
   if (prev && prev->off + prev->size == off)
   {
      prev->size += size;
      if (prev->next && prev->off + prev->size == prev->next->off)
      {
         n           = prev->next;
         prev->size += n->size;
         prev->next  = n->next;
         free(n);
      }
      return;
   }
   if (*pp && off + size == (*pp)->off)
   {
      (*pp)->off   = off;
      (*pp)->size += size;
      return;
   }
   if (!(n = (dk3d_range_t*)malloc(sizeof(*n))))
      return;
   n->off  = off;
   n->size = size;
   n->next = *pp;
   *pp     = n;
}

static void dk3d_tex_destroy(dk3d_t *dk, dk3d_tex_t *t)
{
   if (t->own)
      dkMemBlockDestroy(t->own);
   else
      dk3d_heap_free(dk, t->block, t->off, t->size);
   if (t->desc >= DK3D_DESC_FIRST_TEX)
      dk->desc_free[dk->desc_free_count++] = t->desc;
   free(t);
}

/* @size bytes of the recording slot's staging the GPU copies from;
 * past its end, a block of its own retired with the slot */
static uint8_t *dk3d_stage_alloc(dk3d_t *dk, uint32_t size,
      DkGpuAddr *gpu)
{
   dk3d_frame_t *f = &dk->frames[dk->cur];
   uint32_t off    = DK3D_ALIGN(f->stage_off, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
   DkMemBlock mem;
   if (off + size <= DK3D_STAGE_SIZE)
   {
      f->stage_off = off + size;
      *gpu         = f->stage_gpu + off;
      return f->stage_cpu + off;
   }
   if (!(mem = dk3d_mem_new(dk->device, size,
               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
      return NULL;
   dk3d_retire(dk, mem, NULL);
   *gpu = dkMemBlockGetGpuAddr(mem);
   return (uint8_t*)dkMemBlockGetCpuAddr(mem);
}

/* Copies @rows of @row_bytes at @src_pitch into @dst_view's @rect,
 * through the recording slot's staging */
static bool dk3d_upload(dk3d_t *dk, const DkImageView *dst_view,
      const DkImageRect *rect, const uint8_t *src, size_t src_pitch,
      uint32_t row_bytes)
{
   uint32_t pitch = DK3D_ALIGN(row_bytes, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
   DkCopyBuf cb;
   DkGpuAddr gpu;
   unsigned y;
   uint8_t *dst   = dk3d_stage_alloc(dk, pitch * rect->height, &gpu);
   if (!dst)
      return false;
   if (pitch == src_pitch)
      memcpy(dst, src, (size_t)pitch * rect->height);
   else
      for (y = 0; y < rect->height; y++)
         memcpy(dst + (size_t)y * pitch, src + y * src_pitch, row_bytes);
   cb.addr        = gpu;
   cb.rowLength   = pitch;
   cb.imageHeight = 0;
   dkCmdBufCopyBufferToImage(dk->frames[dk->cur].cmdbuf, &cb, dst_view,
         rect, 0);
   dk->pending_barrier = true;
   return true;
}

/* ---- frame slots ------------------------------------------------------ */

static void dk3d_poll_done(dk3d_t *dk)
{
   unsigned i;
   for (i = 0; i < dk->num_frames; i++)
   {
      dk3d_frame_t *f = &dk->frames[i];
      if (     f->done_valid
            && f->serial > dk->done_serial
            && dkFenceWait(&f->done, 0) == DkResult_Success)
         dk->done_serial = f->serial;
   }
}

/* Waits slot @i out and makes it ready to record into: the one wait on
 * the frame path */
static void dk3d_slot_recycle(dk3d_t *dk, unsigned i)
{
   dk3d_frame_t *f = &dk->frames[i];
   unsigned j;
   if (f->done_valid)
   {
      dkFenceWait(&f->done, -1);
      if (f->serial > dk->done_serial)
         dk->done_serial = f->serial;
   }
   for (j = 0; j < f->retire_count; j++)
   {
      if (f->retire[j].mem)
         dkMemBlockDestroy(f->retire[j].mem);
      if (f->retire[j].tex)
         dk3d_tex_destroy(dk, f->retire[j].tex);
   }
   f->retire_count = 0;
   dkCmdBufClear(f->cmdbuf);
   f->vbo_off      = 0;
   f->stage_off    = 0;
}

static bool dk3d_frames_init(dk3d_t *dk)
{
   unsigned i;
   for (i = 0; i < dk->num_frames; i++)
   {
      DkCmdBufMaker cm;
      dk3d_frame_t *f = &dk->frames[i];
      if (!(f->cmd_mem = dk3d_mem_new(dk->device, DK3D_CMDMEM_SIZE,
                  DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
         return false;
      if (!(f->vbo_mem = dk3d_mem_new(dk->device, DK3D_VBO_SIZE,
                  DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
         return false;
      if (!(f->ubo_mem = dk3d_mem_new(dk->device, DK_UNIFORM_BUF_ALIGNMENT,
                  DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
         return false;
      if (!(f->stage_mem = dk3d_mem_new(dk->device, DK3D_STAGE_SIZE,
                  DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
         return false;
      dkCmdBufMakerDefaults(&cm, dk->device);
      if (!(f->cmdbuf = dkCmdBufCreate(&cm)))
         return false;
      dkCmdBufAddMemory(f->cmdbuf, f->cmd_mem, 0, DK3D_CMDMEM_SIZE);
      f->vbo_cpu   = (uint8_t*)dkMemBlockGetCpuAddr(f->vbo_mem);
      f->vbo_gpu   = dkMemBlockGetGpuAddr(f->vbo_mem);
      f->ubo_gpu   = dkMemBlockGetGpuAddr(f->ubo_mem);
      f->stage_cpu = (uint8_t*)dkMemBlockGetCpuAddr(f->stage_mem);
      f->stage_gpu = dkMemBlockGetGpuAddr(f->stage_mem);
   }
   return true;
}

static void dk3d_frames_free(dk3d_t *dk)
{
   unsigned i, j;
   for (i = 0; i < dk->num_frames; i++)
   {
      dk3d_frame_t *f = &dk->frames[i];
      for (j = 0; j < f->retire_count; j++)
      {
         if (f->retire[j].mem)
            dkMemBlockDestroy(f->retire[j].mem);
         if (f->retire[j].tex)
            dk3d_tex_destroy(dk, f->retire[j].tex);
      }
      free(f->retire);
      if (f->cmdbuf)
         dkCmdBufDestroy(f->cmdbuf);
      if (f->stage_mem)
         dkMemBlockDestroy(f->stage_mem);
      if (f->ubo_mem)
         dkMemBlockDestroy(f->ubo_mem);
      if (f->vbo_mem)
         dkMemBlockDestroy(f->vbo_mem);
      if (f->cmd_mem)
         dkMemBlockDestroy(f->cmd_mem);
   }
}

/* ---- swapchain -------------------------------------------------------- */

static unsigned dk3d_mode_dims(int op_mode)
{
   return op_mode == AppletOperationMode_Console
      ? VIDEO_SCALE_PACK(1920, 1080) : VIDEO_SCALE_PACK(1280, 720);
}

static void dk3d_swapchain_free(dk3d_t *dk)
{
   unsigned i;
   if (dk->swapchain)
   {
      dkSwapchainDestroy(dk->swapchain);
      dk->swapchain = NULL;
   }
   for (i = 0; i < dk->num_swapchain; i++)
      if (dk->sc_mem[i])
      {
         dkMemBlockDestroy(dk->sc_mem[i]);
         dk->sc_mem[i] = NULL;
      }
}

static bool dk3d_swapchain_init(dk3d_t *dk)
{
   DkImageLayoutMaker lm;
   DkImageLayout      layout;
   DkSwapchainMaker   sm;
   unsigned i;

   dkImageLayoutMakerDefaults(&lm, dk->device);
   lm.flags         = DkImageFlags_UsageRender | DkImageFlags_UsagePresent
                    | DkImageFlags_Usage2DEngine | DkImageFlags_HwCompression;
   lm.format        = DkImageFormat_RGBA8_Unorm;
   lm.dimensions[0] = VIDEO_SCALE_W(dk->surface_dims);
   lm.dimensions[1] = VIDEO_SCALE_H(dk->surface_dims);
   dkImageLayoutInitialize(&layout, &lm);

   for (i = 0; i < dk->num_swapchain; i++)
   {
      if (!(dk->sc_mem[i] = dk3d_mem_new(dk->device,
                  (uint32_t)dkImageLayoutGetSize(&layout),
                  DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)))
         return false;
      dkImageInitialize(&dk->sc_images[i], &layout, dk->sc_mem[i], 0);
      dk->sc_ptrs[i] = &dk->sc_images[i];
   }
   nwindowSetDimensions(dk->win, VIDEO_SCALE_W(dk->surface_dims),
         VIDEO_SCALE_H(dk->surface_dims));
   dkSwapchainMakerDefaults(&sm, dk->device, dk->win, dk->sc_ptrs,
         dk->num_swapchain);
   if (!(dk->swapchain = dkSwapchainCreate(&sm)))
      return false;
   dkSwapchainSetSwapInterval(dk->swapchain,
         dk->vsync ? dk->swap_interval : 0);
   return true;
}

/* Dock and handheld have their own sizes; a change remakes the
 * swapchain, the one drain outside teardown */
static void dk3d_check_resize(dk3d_t *dk)
{
   int op_mode = (int)appletGetOperationMode();
   if (op_mode == dk->op_mode && dk->swapchain)
      return;
   dk->op_mode = op_mode;
   if (dk3d_mode_dims(op_mode) == dk->surface_dims && dk->swapchain)
      return;
   dkQueueWaitIdle(dk->queue);
   dk3d_swapchain_free(dk);
   dk->surface_dims  = dk3d_mode_dims(op_mode);
   video_driver_set_output_dims(dk->surface_dims);
   if (!dk3d_swapchain_init(dk))
      RARCH_ERR("[deko3d] Swapchain remake failed.\n");
   dk->vp.pos        = VIDEO_POS_PACK(0, 0);
   dk->vp.full_dims  = dk->surface_dims;
   dk->vp.dims       = dk->surface_dims;
   dk->should_resize = true;
}

/* ---- linear memory ---------------------------------------------------- */

/* A slot of @ring none in flight reads, not @skip0 nor @skip1; -1 when
 * every one is busy */
static int dk3d_linear_pick(dk3d_t *dk, dk3d_linear_t *ring, int skip0,
      int skip1)
{
   int i;
   dk3d_poll_done(dk);
   for (i = 0; i < DK3D_SW_SLOTS; i++)
      if (     i != skip0 && i != skip1
            && ring[i].use_serial <= dk->done_serial)
         return i;
   return -1;
}

/* Room in @l for @dims rows of @pitch: a smaller block is retired */
static bool dk3d_linear_fit(dk3d_t *dk, dk3d_linear_t *l, unsigned dims,
      uint32_t pitch)
{
   uint32_t need = pitch * VIDEO_SCALE_H(dims);
   if (!l->mem || l->size < need)
   {
      DkMemBlock mem = dk3d_mem_new(dk->device, need,
            DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached);
      if (!mem)
         return false;
      if (l->mem)
         dk3d_retire(dk, l->mem, NULL);
      l->mem  = mem;
      l->cpu  = (uint8_t*)dkMemBlockGetCpuAddr(mem);
      l->size = DK3D_ALIGN(need, DK_MEMBLOCK_ALIGNMENT);
   }
   l->pitch = pitch;
   l->dims  = dims;
   return true;
}

static void dk3d_linear_free(dk3d_linear_t *ring, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      if (ring[i].mem)
      {
         dkMemBlockDestroy(ring[i].mem);
         ring[i].mem = NULL;
      }
}

static void dk3d_linear_image(DkDevice device, DkImage *img,
      DkMemBlock mem, uint32_t offset, uint32_t pitch, unsigned dims,
      DkImageFormat fmt, unsigned bpp, uint32_t flags)
{
   DkImageLayoutMaker lm;
   DkImageLayout      layout;
   dkImageLayoutMakerDefaults(&lm, device);
   lm.flags         = DkImageFlags_PitchLinear | flags;
   lm.format        = fmt;
   lm.dimensions[0] = pitch / bpp;
   lm.dimensions[1] = VIDEO_SCALE_H(dims);
   lm.pitchStride   = pitch;
   dkImageLayoutInitialize(&layout, &lm);
   dkImageInitialize(img, &layout, mem, offset);
}

/* ---- software frames -------------------------------------------------- */

static DkImageFormat dk3d_sw_format(const dk3d_t *dk)
{
   /* libretro's RGB565 has red high, XRGB8888 is B,G,R,X in memory */
   return dk->rgb32 ? DkImageFormat_BGRX8_Unorm : DkImageFormat_BGR565_Unorm;
}

/* Where @frame lies when the GPU can read it in place: in the slot lent
 * to the core or in the wrapper's frame memory, whole rows on a pitch
 * the 2D engine takes */
static bool dk3d_sw_find(dk3d_t *dk, const void *frame, unsigned dims,
      unsigned pitch, dk3d_src_t *src, dk3d_linear_t **owner)
{
   uintptr_t p   = (uintptr_t)frame;
   unsigned bpp  = dk->rgb32 ? 4 : 2;
   unsigned i;
   if (pitch % DK_IMAGE_LINEAR_STRIDE_ALIGNMENT)
      return false;
   for (i = 0; i < DK3D_SW_SLOTS + DK3D_FRAME_MEM_MAX; i++)
   {
      dk3d_linear_t *l = (i < DK3D_SW_SLOTS)
         ? &dk->sw[i] : &dk->frame_mem[i - DK3D_SW_SLOTS];
      uintptr_t base   = (uintptr_t)l->cpu;
      uintptr_t off;
      if (!l->mem || p < base || p >= base + l->size)
         continue;
      off = p - base;
      if (off + (uintptr_t)pitch * VIDEO_SCALE_H(dims) > l->size)
         return false;
      src->mem    = l->mem;
      src->offset = (uint32_t)(off - off % pitch);
      src->pitch  = pitch;
      src->dims   = dims;
      src->x      = (unsigned)((off % pitch) / bpp);
      if (     (off % pitch) % bpp
            || src->x + VIDEO_SCALE_W(dims) > pitch / bpp)
         return false;
      dkMemBlockFlushCpuCache(l->mem, src->offset,
            pitch * VIDEO_SCALE_H(dims));
      *owner      = l;
      return true;
   }
   return false;
}

/* The software frame's source: in place where it lies, else copied once
 * into a free ring slot */
static bool dk3d_sw_source(dk3d_t *dk, const void *frame, unsigned dims,
      unsigned pitch, dk3d_src_t *src, dk3d_linear_t **owner)
{
   unsigned bpp  = dk->rgb32 ? 4 : 2;
   unsigned w    = VIDEO_SCALE_W(dims);
   unsigned h    = VIDEO_SCALE_H(dims);
   uint32_t pt   = DK3D_ALIGN(w * bpp, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
   dk3d_linear_t *l;
   unsigned y;
   int s;

   if (dk3d_sw_find(dk, frame, dims, pitch, src, owner))
   {
      if (*owner >= dk->sw && *owner < dk->sw + DK3D_SW_SLOTS)
         dk->sw_last = (int)(*owner - dk->sw);
      dk->sw_lent = -1;
      return true;
   }
   if ((s = dk3d_linear_pick(dk, dk->sw, dk->sw_last, dk->sw_lent)) < 0)
      return false;
   l = &dk->sw[s];
   if (!dk3d_linear_fit(dk, l, dims, pt))
      return false;
   if (pitch == pt)
      memcpy(l->cpu, frame, (size_t)pt * h);
   else
      for (y = 0; y < h; y++)
         memcpy(l->cpu + (size_t)y * pt,
               (const uint8_t*)frame + (size_t)y * pitch, w * bpp);
   dkMemBlockFlushCpuCache(l->mem, 0, pt * h);
   src->mem    = l->mem;
   src->offset = 0;
   src->pitch  = pt;
   src->dims   = dims;
   src->x      = 0;
   *owner      = l;
   dk->sw_last = s;
   return true;
}

/* The next free ring slot, for the core to render into */
static bool dk3d_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   dk3d_t *dk   = (dk3d_t*)data;
   unsigned bpp;
   uint32_t pt;
   int s;
   if (!dk || !fb->width || !fb->height)
      return false;
   bpp = dk->rgb32 ? 4 : 2;
   pt  = DK3D_ALIGN(fb->width * bpp, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
   if ((s = dk3d_linear_pick(dk, dk->sw, dk->sw_last, -1)) < 0)
      return false;
   if (     !dk->sw[s].mem
         || dk->sw[s].size < pt * fb->height)
   {
      /* A cached frame may point into the block being let go of */
      video_driver_cached_frame_retire();
      if (!dk3d_linear_fit(dk, &dk->sw[s],
               VIDEO_SCALE_PACK(fb->width, fb->height), pt))
         return false;
   }
   dk->sw[s].pitch  = pt;
   dk->sw[s].dims   = VIDEO_SCALE_PACK(fb->width, fb->height);
   dk->sw_lent      = s;
   fb->data         = dk->sw[s].cpu;
   fb->pitch        = pt;
   fb->format       = dk->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   fb->memory_flags = RETRO_MEMORY_TYPE_CACHED;
   return true;
}

/* The threaded wrapper's frame buffers, read by the GPU where they lie */
static void *dk3d_frame_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *pitch_align)
{
   dk3d_t *dk = (dk3d_t*)data;
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   unsigned i;
   /* A hardware core's frames are images, not memory */
   if (     !dk || size > 0xffffffffu
         || (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE))
      return NULL;
   for (i = 0; i < DK3D_FRAME_MEM_MAX; i++)
   {
      dk3d_linear_t *l = &dk->frame_mem[i];
      if (l->used)
         continue;
      if (!(l->mem = dk3d_mem_new(dk->device, (uint32_t)size,
                  DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached)))
         return NULL;
      l->cpu        = (uint8_t*)dkMemBlockGetCpuAddr(l->mem);
      l->size       = DK3D_ALIGN((uint32_t)size, DK_MEMBLOCK_ALIGNMENT);
      l->use_serial = 0;
      l->used       = true;
      *id           = i + 1;
      *cached       = true;
      *pitch_align  = DK_IMAGE_LINEAR_STRIDE_ALIGNMENT;
      return l->cpu;
   }
   return NULL;
}

static bool dk3d_frame_mem_ready(void *data, uintptr_t id)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (!dk || !id || id > DK3D_FRAME_MEM_MAX)
      return true;
   if (dk->frame_mem[id - 1].use_serial <= dk->done_serial)
      return true;
   dk3d_poll_done(dk);
   return dk->frame_mem[id - 1].use_serial <= dk->done_serial;
}

static void dk3d_frame_mem_free(void *data, uintptr_t id)
{
   dk3d_t *dk = (dk3d_t*)data;
   dk3d_linear_t *l;
   if (!dk || !id || id > DK3D_FRAME_MEM_MAX)
      return;
   l = &dk->frame_mem[id - 1];
   if (l->mem)
      dk3d_retire(dk, l->mem, NULL);
   l->mem  = NULL;
   l->cpu  = NULL;
   l->size = 0;
   l->used = false;
}

/* ---- 3D drawing ------------------------------------------------------- */

/* Orders what was copied or drawn before against what samples it next */
static void dk3d_barrier(dk3d_t *dk)
{
   if (!dk->pending_barrier)
      return;
   dkCmdBufBarrier(dk->frames[dk->cur].cmdbuf, DkBarrier_Full,
         DkInvalidateFlags_Image | DkInvalidateFlags_Descriptors);
   dk->pending_barrier = false;
}

/* The menu pipeline's state that holds for the whole frame, bound the
 * first time anything draws with it */
static void dk3d_draw_bind(dk3d_t *dk)
{
   DkCmdBuf cmd        = dk->frames[dk->cur].cmdbuf;
   DkShader const *shaders[2];
   DkVtxBufferState vb;
   DkRasterizerState rs;
   DkDepthStencilState ds;
   DkColorWriteState cws;

   dk3d_barrier(dk);
   if (dk->draw_bound)
      return;
   shaders[0] = &dk->vsh;
   shaders[1] = &dk->fsh;
   dkCmdBufBindShaders(cmd, DkStageFlag_GraphicsMask, shaders, 2);
   dkCmdBufBindVtxAttribState(cmd, dk3d_menu_attribs, 3);
   vb.stride  = DK3D_VTX_FLOATS * sizeof(float);
   vb.divisor = 0;
   dkCmdBufBindVtxBufferState(cmd, &vb, 1);
   dkCmdBufBindUniformBuffer(cmd, DkStage_Vertex, 0,
         dk->frames[dk->cur].ubo_gpu, DK_UNIFORM_BUF_ALIGNMENT);
   dkCmdBufBindImageDescriptorSet(cmd, dk->desc_gpu, DK3D_DESC_COUNT);
   dkCmdBufBindSamplerDescriptorSet(cmd, dk->sampler_gpu, 2);
   dkRasterizerStateDefaults(&rs);
   rs.cullMode = DkFace_None;
   dkCmdBufBindRasterizerState(cmd, &rs);
   dkDepthStencilStateDefaults(&ds);
   ds.depthTestEnable  = false;
   ds.depthWriteEnable = false;
   dkCmdBufBindDepthStencilState(cmd, &ds);
   dkColorWriteStateDefaults(&cws);
   dkColorWriteStateSetMask(&cws, 0, DkColorMask_RGBA);
   dkCmdBufBindColorWriteState(cmd, &cws);
   dk->blend        = DK3D_BLEND_UNSET;
   dk->mvp_valid    = false;
   dk->draw_bound   = true;
}

static void dk3d_draw_blend(dk3d_t *dk, int mode)
{
   DkCmdBuf cmd = dk->frames[dk->cur].cmdbuf;
   DkColorState cs;
   DkBlendState bs;
   if (dk->blend == mode)
      return;
   dkColorStateDefaults(&cs);
   dkColorStateSetBlendEnable(&cs, 0, mode != DK3D_BLEND_OFF);
   dkCmdBufBindColorState(cmd, &cs);
   if (mode != DK3D_BLEND_OFF)
   {
      dkBlendStateDefaults(&bs);
      if (mode == DK3D_BLEND_PREMULT)
      {
         bs.srcColorBlendFactor = DkBlendFactor_One;
         bs.srcAlphaBlendFactor = DkBlendFactor_One;
      }
      dkCmdBufBindBlendStates(cmd, 0, &bs, 1);
   }
   dk->blend = mode;
}

/* @mvp pushed into the frame's matrix only when it differs */
static void dk3d_draw_mvp(dk3d_t *dk, const float *mvp)
{
   dk3d_frame_t *f = &dk->frames[dk->cur];
   if (dk->mvp_valid && !memcmp(dk->last_mvp, mvp, sizeof(dk->last_mvp)))
      return;
   memcpy(dk->last_mvp, mvp, sizeof(dk->last_mvp));
   dkCmdBufPushConstants(f->cmdbuf, f->ubo_gpu, DK_UNIFORM_BUF_ALIGNMENT,
         0, sizeof(dk->last_mvp), dk->last_mvp);
   dk->mvp_valid = true;
}

/* Viewport @pos/@dims bottom-up, and the scissor when one is set */
static void dk3d_draw_rect(dk3d_t *dk, unsigned pos, unsigned dims)
{
   DkCmdBuf cmd = dk->frames[dk->cur].cmdbuf;
   unsigned sw  = VIDEO_SCALE_W(dk->surface_dims);
   unsigned sh  = VIDEO_SCALE_H(dk->surface_dims);
   unsigned spos, sdims;
   if (pos != dk->last_vp_pos || dims != dk->last_vp_dims)
   {
      DkViewport vp;
      vp.x      = (float)VIDEO_POS_X(pos);
      vp.y      = (float)VIDEO_POS_Y(pos);
      vp.width  = (float)VIDEO_SCALE_W(dims);
      vp.height = (float)VIDEO_SCALE_H(dims);
      vp.near   = 0.0f;
      vp.far    = 1.0f;
      dkCmdBufSetViewports(cmd, 0, &vp, 1);
      dk->last_vp_pos  = pos;
      dk->last_vp_dims = dims;
   }
   if (dk->scissor_on)
   {
      /* Given top-down, and clamped to the surface */
      int x  = VIDEO_POS_X(dk->scissor_pos);
      int y  = VIDEO_POS_Y(dk->scissor_pos);
      int x1 = x + (int)VIDEO_SCALE_W(dk->scissor_dims);
      int y1 = y + (int)VIDEO_SCALE_H(dk->scissor_dims);
      if (x < 0)
         x = 0;
      if (y < 0)
         y = 0;
      if (x1 > (int)sw)
         x1 = (int)sw;
      if (y1 > (int)sh)
         y1 = (int)sh;
      if (x1 < x)
         x1 = x;
      if (y1 < y)
         y1 = y;
      spos  = VIDEO_POS_PACK(x, (int)sh - y1);
      sdims = VIDEO_SCALE_PACK(x1 - x, y1 - y);
   }
   else
   {
      spos  = VIDEO_POS_PACK(0, 0);
      sdims = dk->surface_dims;
   }
   if (spos != dk->last_sc_pos || sdims != dk->last_sc_dims)
   {
      DkScissor sc;
      sc.x      = (uint32_t)VIDEO_POS_X(spos);
      sc.y      = (uint32_t)VIDEO_POS_Y(spos);
      sc.width  = VIDEO_SCALE_W(sdims);
      sc.height = VIDEO_SCALE_H(sdims);
      dkCmdBufSetScissors(cmd, 0, &sc, 1);
      dk->last_sc_pos  = spos;
      dk->last_sc_dims = sdims;
   }
}

/* Room for @n vertices in the frame's vertex memory */
static float *dk3d_vtx_alloc(dk3d_t *dk, unsigned n, DkGpuAddr *gpu,
      uint32_t *bytes)
{
   dk3d_frame_t *f = &dk->frames[dk->cur];
   uint32_t size   = n * DK3D_VTX_FLOATS * sizeof(float);
   uint32_t off    = DK3D_ALIGN(f->vbo_off, 16);
   if (off + size > DK3D_VBO_SIZE)
      return NULL;
   f->vbo_off = off + size;
   *gpu       = f->vbo_gpu + off;
   *bytes     = size;
   return (float*)(f->vbo_cpu + off);
}

static void dk3d_draw_vertices(dk3d_t *dk, DkPrimitive prim,
      DkGpuAddr gpu, uint32_t bytes, unsigned n, unsigned desc,
      unsigned sampler)
{
   DkCmdBuf cmd       = dk->frames[dk->cur].cmdbuf;
   DkResHandle handle = dkMakeTextureHandle(desc, sampler);
   dkCmdBufBindVtxBuffer(cmd, 0, gpu, bytes);
   dkCmdBufBindTexture(cmd, DkStage_Fragment, 0, handle);
   dkCmdBufDraw(cmd, prim, n, 1, 0, 0);
}

/* ---- textures --------------------------------------------------------- */

static dk3d_tex_t *dk3d_tex_new(dk3d_t *dk, unsigned dims,
      DkImageFormat fmt, unsigned desc)
{
   DkImageLayoutMaker lm;
   DkImageLayout      layout;
   DkImageView        view;
   uint32_t size, align;
   dk3d_tex_t *t = (dk3d_tex_t*)calloc(1, sizeof(*t));
   if (!t)
      return NULL;
   dkImageLayoutMakerDefaults(&lm, dk->device);
   lm.format        = fmt;
   lm.dimensions[0] = VIDEO_SCALE_W(dims);
   lm.dimensions[1] = VIDEO_SCALE_H(dims);
   dkImageLayoutInitialize(&layout, &lm);
   size  = (uint32_t)dkImageLayoutGetSize(&layout);
   align = dkImageLayoutGetAlignment(&layout);
   if (size > DK3D_HEAP_BLOCK / 2)
   {
      if (!(t->own = dk3d_mem_new(dk->device, size,
                  DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image)))
      {
         free(t);
         return NULL;
      }
      dkImageInitialize(&t->image, &layout, t->own, 0);
   }
   else
   {
      if (!dk3d_heap_alloc(dk, size, align, &t->block, &t->off))
      {
         free(t);
         return NULL;
      }
      t->size = size;
      dkImageInitialize(&t->image, &layout, dk->heap[t->block].mem, t->off);
   }
   t->desc = (uint16_t)desc;
   dkImageViewDefaults(&view, &t->image);
   if (fmt == DkImageFormat_R8_Unorm)
   {
      /* Coverage only: sampled as premultiplied white */
      view.swizzle[0] = DkImageSwizzle_Red;
      view.swizzle[1] = DkImageSwizzle_Red;
      view.swizzle[2] = DkImageSwizzle_Red;
      view.swizzle[3] = DkImageSwizzle_Red;
   }
   dkImageDescriptorInitialize(&dk->desc_cpu[desc], &view, false, false);
   dk->pending_barrier = true;
   return t;
}

static unsigned dk3d_desc_take(dk3d_t *dk)
{
   return dk->desc_free_count ? dk->desc_free[--dk->desc_free_count] : 0;
}

static uintptr_t dk3d_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   dk3d_t *dk                 = (dk3d_t*)video_data;
   struct texture_image *img  = (struct texture_image*)data;
   DkImageView view;
   DkImageRect rect;
   dk3d_tex_t *t;
   unsigned desc;
   (void)threaded;
   if (!dk || !img || !img->pixels || !img->width || !img->height)
      return 0;
   if (!(desc = dk3d_desc_take(dk)))
      return 0;
   /* Loaders' ARGB words are B,G,R,A in memory */
   if (!(t = dk3d_tex_new(dk, VIDEO_SCALE_PACK(img->width, img->height),
               DkImageFormat_BGRA8_Unorm, desc)))
   {
      dk->desc_free[dk->desc_free_count++] = (uint16_t)desc;
      return 0;
   }
   t->sampler = (     filter_type == TEXTURE_FILTER_NEAREST
                   || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
      ? DK3D_SAMPLER_NEAREST : DK3D_SAMPLER_LINEAR;
   dkImageViewDefaults(&view, &t->image);
   rect.x      = 0;
   rect.y      = 0;
   rect.z      = 0;
   rect.width  = img->width;
   rect.height = img->height;
   rect.depth  = 1;
   if (!dk3d_upload(dk, &view, &rect, (const uint8_t*)img->pixels,
            img->width * 4, img->width * 4))
   {
      dk3d_tex_destroy(dk, t);
      return 0;
   }
   return (uintptr_t)t;
}

static void dk3d_unload_texture(void *data, bool threaded, uintptr_t handle)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)threaded;
   if (dk && handle)
      dk3d_retire(dk, NULL, (dk3d_tex_t*)handle);
}

/* ---- menu (gfx_display) ----------------------------------------------- */

static void *gfx_display_dk3d_get_default_mvp(void *data)
{
   dk3d_t *dk = (dk3d_t*)data;
   return dk ? &dk->mvp_default : NULL;
}

static const float *gfx_display_dk3d_get_default_vertices(void)
{
   return dk3d_default_vertices;
}

static const float *gfx_display_dk3d_get_default_tex_coords(void)
{
   return dk3d_default_tex_coords;
}

static void gfx_display_dk3d_blend_begin(void *data)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (dk && dk->recording)
   {
      dk3d_draw_bind(dk);
      dk3d_draw_blend(dk, DK3D_BLEND_ALPHA);
   }
}

static void gfx_display_dk3d_blend_end(void *data)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (dk && dk->recording)
   {
      dk3d_draw_bind(dk);
      dk3d_draw_blend(dk, DK3D_BLEND_OFF);
   }
}

static void gfx_display_dk3d_scissor_begin(void *data, unsigned video_dims,
      int x, int y, unsigned dims)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)video_dims;
   if (!dk)
      return;
   dk->scissor_pos  = VIDEO_POS_PACK(x, y);
   dk->scissor_dims = dims;
   dk->scissor_on   = true;
}

static void gfx_display_dk3d_scissor_end(void *data, unsigned video_dims)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)video_dims;
   if (dk)
      dk->scissor_on = false;
}

static void gfx_display_dk3d_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_dims)
{
   dk3d_t *dk         = (dk3d_t*)data;
   const float *vert, *tc, *col;
   const dk3d_tex_t *t;
   unsigned n, i;
   DkGpuAddr gpu;
   uint32_t bytes;
   float *v;
   (void)video_dims;

   if (!dk || !dk->recording || !draw || !draw->coords)
      return;
   if (!(n = draw->coords->vertices))
      return;
   vert = draw->coords->vertex    ? draw->coords->vertex    : dk3d_default_vertices;
   tc   = draw->coords->tex_coord ? draw->coords->tex_coord : dk3d_default_tex_coords;
   col  = draw->coords->color     ? draw->coords->color     : dk3d_default_colors;
   /* The defaults are one quad's: past it, a strip needs its own */
   if (n > 4 && (   vert == dk3d_default_vertices
                 || tc   == dk3d_default_tex_coords
                 || col  == dk3d_default_colors))
      return;
   if (!(v = dk3d_vtx_alloc(dk, n, &gpu, &bytes)))
      return;
   for (i = 0; i < n; i++, v += DK3D_VTX_FLOATS)
   {
      v[0] = vert[2 * i + 0];
      v[1] = vert[2 * i + 1];
      /* Texture rows run top-down, the menu's coordinates bottom-up */
      v[2] = tc[2 * i + 0];
      v[3] = 1.0f - tc[2 * i + 1];
      v[4] = col[4 * i + 0];
      v[5] = col[4 * i + 1];
      v[6] = col[4 * i + 2];
      v[7] = col[4 * i + 3];
   }
   t = (const dk3d_tex_t*)draw->texture;
   dk3d_draw_bind(dk);
   if (dk->blend == DK3D_BLEND_UNSET)
      dk3d_draw_blend(dk, DK3D_BLEND_OFF);
   dk3d_draw_mvp(dk, draw->matrix_data
         ? (const float*)draw->matrix_data : dk->mvp_default.data);
   dk3d_draw_rect(dk, draw->pos, draw->dims);
   dk3d_draw_vertices(dk, DkPrimitive_TriangleStrip, gpu, bytes, n,
         t ? t->desc : DK3D_DESC_BLANK,
         t ? t->sampler : DK3D_SAMPLER_LINEAR);
}

static void gfx_display_dk3d_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp, void *data, unsigned video_dims)
{
   (void)draw;
   (void)p_disp;
   (void)data;
   (void)video_dims;
}

gfx_display_ctx_driver_t gfx_display_ctx_deko3d = {
   gfx_display_dk3d_draw,
   gfx_display_dk3d_draw_pipeline,
   gfx_display_dk3d_blend_begin,
   gfx_display_dk3d_blend_end,
   gfx_display_dk3d_get_default_mvp,
   gfx_display_dk3d_get_default_vertices,
   gfx_display_dk3d_get_default_tex_coords,
   &deko3d_font,
   GFX_VIDEO_DRIVER_DEKO3D,
   "deko3d",
   false,
   true,
   gfx_display_dk3d_scissor_begin,
   gfx_display_dk3d_scissor_end
};

/* ---- RGUI ------------------------------------------------------------- */

static void dk3d_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned dims, float alpha)
{
   dk3d_t *dk   = (dk3d_t*)data;
   unsigned bpp = rgb32 ? 4 : 2;
   unsigned w   = VIDEO_SCALE_W(dims);
   unsigned h   = VIDEO_SCALE_H(dims);
   uint32_t pt  = DK3D_ALIGN(w * bpp, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
   DkImageView view;
   dk3d_linear_t *l;
   unsigned y;
   int s;

   if (!dk || !frame || !w || !h)
      return;
   dk->menu_alpha = alpha;
   if ((s = dk3d_linear_pick(dk, dk->rgui, dk->rgui_last, -1)) < 0)
      return;
   l = &dk->rgui[s];
   if (!dk3d_linear_fit(dk, l, dims, pt))
      return;
   for (y = 0; y < h; y++)
      memcpy(l->cpu + (size_t)y * pt,
            (const uint8_t*)frame + (size_t)y * w * bpp, w * bpp);
   dkMemBlockFlushCpuCache(l->mem, 0, pt * h);
   l->bpp = (uint8_t)bpp;
   /* Sampled where it lies: RGUI's RGBA4444 has red in the high nibble,
    * the format the low one */
   dk3d_linear_image(dk->device, &l->image, l->mem, 0, pt, dims,
         rgb32 ? DkImageFormat_BGRA8_Unorm : DkImageFormat_RGBA4_Unorm,
         bpp, 0);
   dkImageViewDefaults(&view, &l->image);
   if (!rgb32)
   {
      view.swizzle[0] = DkImageSwizzle_Alpha;
      view.swizzle[1] = DkImageSwizzle_Blue;
      view.swizzle[2] = DkImageSwizzle_Green;
      view.swizzle[3] = DkImageSwizzle_Red;
   }
   dkImageDescriptorInitialize(&dk->desc_cpu[DK3D_DESC_RGUI + s], &view,
         false, false);
   dk->pending_barrier = true;
   dk->rgui_last       = s;
}

static void dk3d_set_texture_enable(void *data, bool enable,
      bool full_screen)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (!dk)
      return;
   dk->menu_enable      = enable;
   dk->menu_full_screen = full_screen;
}

/* RGUI as a quad over the frame, at its own aspect unless full screen */
static void dk3d_draw_rgui(dk3d_t *dk)
{
   dk3d_linear_t *l = &dk->rgui[dk->rgui_last];
   unsigned sw      = VIDEO_SCALE_W(dk->surface_dims);
   unsigned sh      = VIDEO_SCALE_H(dk->surface_dims);
   unsigned mw      = VIDEO_SCALE_W(l->dims);
   unsigned mh      = VIDEO_SCALE_H(l->dims);
   float x0 = 0.0f, y0 = 0.0f, x1 = 1.0f, y1 = 1.0f;
   float u1;
   DkGpuAddr gpu;
   uint32_t bytes;
   float *v;
   unsigned i;

   if (!dk->menu_full_screen && mw && mh)
   {
      float r = ((float)mw / (float)mh) / ((float)sw / (float)sh);
      if (r < 1.0f)
      {
         x0 = 0.5f - r * 0.5f;
         x1 = 0.5f + r * 0.5f;
      }
      else
      {
         y0 = 0.5f - 0.5f / r;
         y1 = 0.5f + 0.5f / r;
      }
   }
   if (!(v = dk3d_vtx_alloc(dk, 4, &gpu, &bytes)))
      return;
   /* The image is the pitch wide, the menu its width */
   u1 = (float)mw / (float)(l->pitch / l->bpp);
   for (i = 0; i < 4; i++)
   {
      float *p = v + i * DK3D_VTX_FLOATS;
      p[0] = (i & 1) ? x1 : x0;
      p[1] = (i & 2) ? y1 : y0;
      p[2] = (i & 1) ? u1 : 0.0f;
      p[3] = (i & 2) ? 0.0f : 1.0f;
      p[4] = 1.0f;
      p[5] = 1.0f;
      p[6] = 1.0f;
      p[7] = dk->menu_alpha;
   }
   dk3d_draw_bind(dk);
   dk3d_draw_blend(dk, DK3D_BLEND_ALPHA);
   dk3d_draw_mvp(dk, dk->mvp_default.data);
   dk3d_draw_rect(dk, VIDEO_POS_PACK(0, 0), dk->surface_dims);
   dk3d_draw_vertices(dk, DkPrimitive_TriangleStrip, gpu, bytes, 4,
         DK3D_DESC_RGUI + dk->rgui_last,
         dk->smooth ? DK3D_SAMPLER_LINEAR : DK3D_SAMPLER_NEAREST);
}

/* ---- font ------------------------------------------------------------- */

static void *dk3d_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   dk3d_font_t *f = (dk3d_font_t*)calloc(1, sizeof(*f));
   (void)is_threaded;
   if (!f)
      return NULL;
   if (     !(f->dk = (dk3d_t*)data)
         || !font_renderer_create_default(&f->drv, &f->data, font_path,
            font_size, FONT_ATLAS_FORMAT_A8))
   {
      free(f);
      return NULL;
   }
   f->atlas = f->drv->get_atlas(f->data);
   return f;
}

static void dk3d_font_free(void *data, bool is_threaded)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   (void)is_threaded;
   if (!f)
      return;
   if (f->tex)
      dk3d_retire(f->dk, NULL, f->tex);
   if (f->drv && f->data)
      f->drv->free(f->data);
   free(f);
}

/* The atlas's dirty rectangles sent to its R8 texture, made again at a
 * new size when it has grown */
static bool dk3d_font_upload(dk3d_font_t *f)
{
   dk3d_t *dk               = f->dk;
   struct font_atlas *a     = f->drv->get_atlas(f->data);
   unsigned dims, i, n;
   DkImageView view;
   f->atlas = a;
   if (!a || !a->buffer || !a->width || !a->height)
      return false;
   dims = VIDEO_SCALE_PACK(a->width, a->height);
   if (!f->tex || f->tex_dims != dims)
   {
      /* A descriptor of its own: frames in flight still sample the old
       * atlas through theirs, retired with it */
      unsigned desc = dk3d_desc_take(dk);
      dk3d_tex_t *t;
      if (!desc)
         return false;
      if (f->tex)
         dk3d_retire(dk, NULL, f->tex);
      if (!(t = dk3d_tex_new(dk, dims, DkImageFormat_R8_Unorm, desc)))
      {
         f->tex = NULL;
         return false;
      }
      f->tex      = t;
      f->tex_dims = dims;
      a->dirty    = true;
      a->dirty_rects = 0;
      a->dirty_xy0   = VIDEO_SCALE_PACK(0, 0);
      a->dirty_xy1   = dims;
   }
   if (!a->dirty)
      return true;
   dkImageViewDefaults(&view, &f->tex->image);
   n = font_atlas_dirty_rects(a);
   for (i = 0; i < n; i++)
   {
      unsigned xy0, xy1;
      DkImageRect rect;
      font_atlas_dirty_rect(a, i, &xy0, &xy1);
      if (     VIDEO_SCALE_W(xy1) <= VIDEO_SCALE_W(xy0)
            || VIDEO_SCALE_H(xy1) <= VIDEO_SCALE_H(xy0))
         continue;
      rect.x      = VIDEO_SCALE_W(xy0);
      rect.y      = VIDEO_SCALE_H(xy0);
      rect.z      = 0;
      rect.width  = VIDEO_SCALE_W(xy1) - rect.x;
      rect.height = VIDEO_SCALE_H(xy1) - rect.y;
      rect.depth  = 1;
      dk3d_upload(dk, &view, &rect,
            a->buffer + (size_t)rect.y * a->width + rect.x,
            a->width, rect.width);
   }
   a->dirty       = false;
   a->dirty_rects = 0;
   return true;
}

/* One line's glyphs as triangles at (@x, @y) pixels top-down, in
 * premultiplied @c */
static unsigned dk3d_font_emit(dk3d_font_t *f, float *v, const char *msg,
      size_t len, float scale, int x, int y, const float *c)
{
   const struct font_glyph *fallback = f->drv->get_glyph(f->data, '?');
   const char *end = msg + len;
   unsigned sw     = VIDEO_SCALE_W(f->dk->surface_dims);
   unsigned sh     = VIDEO_SCALE_H(f->dk->surface_dims);
   float iw        = 1.0f / (float)VIDEO_SCALE_W(f->tex_dims);
   float ih        = 1.0f / (float)VIDEO_SCALE_H(f->tex_dims);
   int dx = 0, dy  = 0;
   unsigned n      = 0;
   while (msg < end)
   {
      const struct font_glyph *g;
      uint32_t code = utf8_walk(&msg);
      float gx0, gy0, gx1, gy1, u0, v0, u1, v1;
      unsigned gw, gh, k;
      if (!(g = f->drv->get_glyph(f->data, code)) && !(g = fallback))
         continue;
      gw = VIDEO_SCALE_W(g->dims);
      gh = VIDEO_SCALE_H(g->dims);
      if (gw && gh)
      {
         float px = (float)x + (float)(g->draw_offset_x + dx) * scale;
         float py = (float)y + (float)(g->draw_offset_y + dy) * scale;
         gx0 = px / (float)sw;
         gx1 = (px + (float)gw * scale) / (float)sw;
         gy0 = 1.0f - py / (float)sh;
         gy1 = 1.0f - (py + (float)gh * scale) / (float)sh;
         u0  = (float)VIDEO_SCALE_W(g->atlas_pos) * iw;
         v0  = (float)VIDEO_SCALE_H(g->atlas_pos) * ih;
         u1  = u0 + (float)gw * iw;
         v1  = v0 + (float)gh * ih;
         for (k = 0; k < 6; k++)
         {
            /* TL TR BL, BL TR BR */
            static const unsigned char right[6]  = { 0, 1, 0, 0, 1, 1 };
            static const unsigned char bottom[6] = { 0, 0, 1, 1, 0, 1 };
            float *p = v + (n * 6 + k) * DK3D_VTX_FLOATS;
            p[0] = right[k]  ? gx1 : gx0;
            p[1] = bottom[k] ? gy1 : gy0;
            p[2] = right[k]  ? u1  : u0;
            p[3] = bottom[k] ? v1  : v0;
            p[4] = c[0];
            p[5] = c[1];
            p[6] = c[2];
            p[7] = c[3];
         }
         n++;
      }
      dx += g->advance_x;
      dy += g->advance_y;
   }
   return n;
}

static void dk3d_font_render_msg(void *userdata, void *data,
      const char *msg, size_t msg_len, const struct font_params *params)
{
   dk3d_t *dk     = (dk3d_t*)userdata;
   dk3d_font_t *f = (dk3d_font_t*)data;
   font_params_resolved_t rp;
   struct font_line_metrics *lm = NULL;
   float line_h   = 0.0f;
   float c[4], s[4];
   const char *start;
   const char *p;
   size_t worst;
   unsigned n     = 0;
   unsigned line  = 0;
   int bx, by, width;
   DkGpuAddr gpu;
   uint32_t bytes;
   float *v;

   if (!dk || !f || !dk->recording || !msg || !msg_len)
      return;
   if (!dk3d_font_upload(f))
      return;
   font_driver_resolve_params(params, &rp);
   /* Premultiplied, as the coverage is */
   c[0] = rp.color[0] * rp.color[3];
   c[1] = rp.color[1] * rp.color[3];
   c[2] = rp.color[2] * rp.color[3];
   c[3] = rp.color[3];
   s[3] = rp.color[3] * rp.drop_alpha;
   s[0] = rp.color[0] * rp.drop_mod * s[3];
   s[1] = rp.color[1] * rp.drop_mod * s[3];
   s[2] = rp.color[2] * rp.drop_mod * s[3];

   worst = msg_len * 6 * ((rp.drop_x || rp.drop_y) ? 2 : 1);
   if (worst > 0xffff || !(v = dk3d_vtx_alloc(dk, (unsigned)worst,
               &gpu, &bytes)))
      return;
   if (f->drv->get_line_metrics)
      f->drv->get_line_metrics(f->data, &lm);
   if (lm)
      line_h = lm->height * rp.scale;

   start = p = msg;
   for (;;)
   {
      if (p == msg + msg_len || *p == '\n')
      {
         size_t len = (size_t)(p - start);
         if (len)
         {
            width = 0;
            if (rp.text_align != TEXT_ALIGN_LEFT)
            {
               const char *q = start;
               while (q < start + len)
               {
                  const struct font_glyph *g = f->drv->get_glyph(f->data,
                        utf8_walk(&q));
                  if (g)
                     width += g->advance_x;
               }
               width = (int)((float)width * rp.scale);
               if (rp.text_align == TEXT_ALIGN_CENTER)
                  width /= 2;
            }
            bx = (int)(rp.x * (float)VIDEO_SCALE_W(dk->surface_dims)) - width;
            by = (int)((1.0f - rp.y) * (float)VIDEO_SCALE_H(dk->surface_dims)
                  + (float)line * line_h);
            if (rp.drop_x || rp.drop_y)
               n += dk3d_font_emit(f, v + n * 6 * DK3D_VTX_FLOATS, start,
                     len, rp.scale, bx + rp.drop_x, by - rp.drop_y, s);
            n += dk3d_font_emit(f, v + n * 6 * DK3D_VTX_FLOATS, start, len,
                  rp.scale, bx, by, c);
         }
         if (p == msg + msg_len)
            break;
         start = ++p;
         line++;
      }
      else
         p++;
   }
   if (!n)
      return;
   dk3d_draw_bind(dk);
   dk3d_draw_blend(dk, DK3D_BLEND_PREMULT);
   dk3d_draw_mvp(dk, dk->mvp_default.data);
   dk3d_draw_rect(dk, VIDEO_POS_PACK(0, 0), dk->surface_dims);
   dk3d_draw_vertices(dk, DkPrimitive_Triangles, gpu,
         n * 6 * DK3D_VTX_FLOATS * sizeof(float), n * 6, f->tex->desc,
         DK3D_SAMPLER_LINEAR);
   (void)bytes;
}

static int dk3d_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   dk3d_font_t *f  = (dk3d_font_t*)data;
   const char *end = msg + msg_len;
   int width       = 0;
   if (!f)
      return 0;
   while (msg < end)
   {
      const struct font_glyph *g = f->drv->get_glyph(f->data,
            utf8_walk(&msg));
      if (g)
         width += g->advance_x;
   }
   return (int)((float)width * scale);
}

static const struct font_glyph *dk3d_font_get_glyph(void *data,
      uint32_t code)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   return (f && f->drv) ? f->drv->get_glyph(f->data, code) : NULL;
}

static bool dk3d_font_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   dk3d_font_t *f = (dk3d_font_t*)data;
   if (!f || !f->drv || !f->drv->get_line_metrics)
      return false;
   f->drv->get_line_metrics(f->data, metrics);
   return true;
}

static font_renderer_t deko3d_font = {
   dk3d_font_init,
   dk3d_font_free,
   dk3d_font_render_msg,
   "deko3d",
   dk3d_font_get_glyph,
   NULL,
   NULL,
   dk3d_font_get_message_width,
   dk3d_font_get_line_metrics
};

/* ---- hardware cores --------------------------------------------------- */

static void dk3d_hw_take(dk3d_t *dk, const struct retro_deko3d_image *img,
      const DkFence *acquire)
{
   if (!img || !img->image)
   {
      dk->hw_has_image   = false;
      dk->hw_has_acquire = false;
      return;
   }
   dk->hw_img       = *img->image;
   dk->hw_pos       = VIDEO_SCALE_PACK(img->x, img->y);
   dk->hw_dims      = VIDEO_SCALE_PACK(img->width, img->height);
   dk->hw_has_image = true;
   if (acquire)
   {
      dk->hw_acquire     = *acquire;
      dk->hw_has_acquire = true;
   }
}

static void dk3d_hw_set_image(void *handle,
      const struct retro_deko3d_image *image, const DkFence *acquire)
{
   if (handle)
      dk3d_hw_take((dk3d_t*)handle, image, acquire);
}

static uint32_t dk3d_hw_get_sync_index(void *handle)
{
   dk3d_t *dk = (dk3d_t*)handle;
   return dk ? dk->cur : 0;
}

static uint32_t dk3d_hw_get_sync_index_mask(void *handle)
{
   dk3d_t *dk = (dk3d_t*)handle;
   return dk ? (1u << dk->num_frames) - 1 : 0;
}

/* The slot being recorded was waited before retro_run: nothing left */
static void dk3d_hw_wait_sync_index(void *handle)
{
   dk3d_t *dk = (dk3d_t*)handle;
   dk3d_frame_t *f;
   if (!dk)
      return;
   f = &dk->frames[dk->cur];
   if (f->done_valid)
      dkFenceWait(&f->done, -1);
}

static bool dk3d_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (!dk)
      return false;
   *iface = (const struct retro_hw_render_interface*)&dk->hw_iface;
   return true;
}

/* The threaded wrapper's ring: a slot's image and acquire fence copied
 * in from the video thread, and a slot fence that is the done fence of
 * the frame just submitted, which covers every read that frame made */
static bool dk3d_hw_ring_install(void *data, const void *image,
      const void *semaphores, unsigned num_semaphores,
      unsigned src_queue_family, const void *cmd, unsigned num_cmd)
{
   (void)src_queue_family;
   (void)cmd;
   (void)num_cmd;
   if (!data)
      return false;
   dk3d_hw_take((dk3d_t*)data, (const struct retro_deko3d_image*)image,
         num_semaphores ? (const DkFence*)semaphores : NULL);
   return true;
}

static bool dk3d_hw_ring_fence_new(void *data, void **fence)
{
   /* A zeroed DkFence is an empty one: waited, it returns at once */
   DkFence *f = (DkFence*)calloc(1, sizeof(*f));
   (void)data;
   if (!f)
      return false;
   *fence = f;
   return true;
}

static void dk3d_hw_ring_fence_free(void *data, void *fence)
{
   (void)data;
   free(fence);
}

static void dk3d_hw_ring_fence_signal(void *data, void *fence)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (dk && fence)
      *(DkFence*)fence = dk->last_done;
}

static bool dk3d_hw_ring_fence_wait(void *data, void *fence,
      unsigned timeout_us)
{
   (void)data;
   if (!fence)
      return true;
   return dkFenceWait((DkFence*)fence, timeout_us == HW_RING_WAIT_FOREVER
         ? -1 : (int64_t)timeout_us * 1000) == DkResult_Success;
}

/* ---- frame ------------------------------------------------------------ */

static void dk3d_set_viewport(void *data, unsigned dims,
      bool force_full, bool allow_rotate)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)allow_rotate;
   if (!dk)
      return;
   dk->vp.full_dims = dims;
   /* The swapchain is shown flipped: blits land bottom-up */
   video_driver_update_viewport(&dk->vp, force_full, dk->keep_aspect,
         false);
   if (VIDEO_POS_X(dk->vp.pos) < 0)
      VIDEO_POS_PUT_X(dk->vp.pos, 0);
   if (VIDEO_POS_Y(dk->vp.pos) < 0)
      VIDEO_POS_PUT_Y(dk->vp.pos, 0);
}

/* @src, or the hardware image, scaled into the viewport */
static void dk3d_blit_content(dk3d_t *dk, const DkImage *dst,
      const dk3d_src_t *src)
{
   DkCmdBuf cmd = dk->frames[dk->cur].cmdbuf;
   DkImageView sv, dv;
   DkImageRect sr, dr;
   DkImage img;
   uint32_t flags = dk->smooth ? DkBlitFlag_FilterLinear
                               : DkBlitFlag_FilterNearest;

   if (dk->src_kind == DK3D_SRC_HW)
   {
      dkImageViewDefaults(&sv, &dk->hw_img);
      sr.x      = VIDEO_SCALE_W(dk->hw_pos);
      sr.y      = VIDEO_SCALE_H(dk->hw_pos);
      sr.width  = VIDEO_SCALE_W(dk->hw_dims);
      sr.height = VIDEO_SCALE_H(dk->hw_dims);
      if (!dk->hw_bottom_left)
         flags |= DkBlitFlag_FlipY;
   }
   else
   {
      dk3d_linear_image(dk->device, &img, src->mem, src->offset,
            src->pitch, src->dims, dk3d_sw_format(dk), dk->rgb32 ? 4 : 2,
            DkImageFlags_Usage2DEngine);
      dkImageViewDefaults(&sv, &img);
      sr.x      = src->x;
      sr.y      = 0;
      sr.width  = VIDEO_SCALE_W(src->dims);
      sr.height = VIDEO_SCALE_H(src->dims);
      flags    |= DkBlitFlag_FlipY;
   }
   sr.z      = 0;
   sr.depth  = 1;
   dkImageViewDefaults(&dv, dst);
   dr.x      = (uint32_t)VIDEO_POS_X(dk->vp.pos);
   dr.y      = (uint32_t)VIDEO_POS_Y(dk->vp.pos);
   dr.z      = 0;
   dr.width  = VIDEO_SCALE_W(dk->vp.dims);
   dr.height = VIDEO_SCALE_H(dk->vp.dims);
   dr.depth  = 1;
   if (!dr.width || !dr.height)
      return;
   dkCmdBufBlitImage(cmd, &sv, &sr, &dv, &dr, flags | DkBlitFlag_ModeBlit, 0);
}

static bool dk3d_frame(void *data, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   dk3d_t *dk               = (dk3d_t*)data;
   dk3d_linear_t *owner     = NULL;
   dk3d_linear_t *rgui      = NULL;
   dk3d_frame_t *f;
   DkImageView target;
   const DkImageView *targets[1];
   DkCmdList list;
   dk3d_src_t src;
   int slot;
   bool full;
#ifdef HAVE_MENU
   bool menu_alive          = (video_info->menu_st_flags
         & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
   (void)frame_count;

   if (!dk)
      return false;
   dk3d_check_resize(dk);
   if (!dk->swapchain)
      return false;
   f = &dk->frames[dk->cur];

   if (dk->should_resize || dk->vp.full_dims != dk->surface_dims)
   {
      dk3d_set_viewport(dk, dk->surface_dims, false, true);
      dk->should_resize = false;
   }

   /* What the frame shows, settled before the swapchain is touched:
    * a dupe shows the last again */
   if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      dk->src_kind = dk->hw_has_image ? DK3D_SRC_HW : DK3D_SRC_NONE;
   else if (frame && dims)
   {
      if (dk3d_sw_source(dk, frame, dims, pitch, &src, &owner))
      {
         dk->src_kind = DK3D_SRC_LINEAR;
         dk->last_src = src;
      }
   }
   src = dk->last_src;
   if (dk->src_kind == DK3D_SRC_LINEAR && !owner)
   {
      unsigned i;
      for (i = 0; i < DK3D_SW_SLOTS + DK3D_FRAME_MEM_MAX && !owner; i++)
      {
         dk3d_linear_t *l = (i < DK3D_SW_SLOTS)
            ? &dk->sw[i] : &dk->frame_mem[i - DK3D_SW_SLOTS];
         if (l->mem && l->mem == src.mem)
            owner = l;
      }
      if (!owner)
         dk->src_kind = DK3D_SRC_NONE;
   }

   if ((slot = dkQueueAcquireImage(dk->queue, dk->swapchain)) < 0)
      return false;
   dk->recording = true;
   dk3d_barrier(dk);
   dkImageViewDefaults(&target, &dk->sc_images[slot]);
   targets[0] = &target;
   dkCmdBufBindRenderTargets(f->cmdbuf, targets, 1, NULL);
   /* Whole surface: the last frame may have left a menu scissor set */
   dk->last_vp_dims = 0;
   dk->last_sc_dims = 0;
   dk->scissor_on   = false;
   dk3d_draw_rect(dk, VIDEO_POS_PACK(0, 0), dk->surface_dims);

   full = dk->src_kind != DK3D_SRC_NONE
      && dk->vp.pos == VIDEO_POS_PACK(0, 0)
      && dk->vp.dims == dk->surface_dims;
   if (!full)
   {
      dkCmdBufClearColorFloat(f->cmdbuf, 0, DkColorMask_RGBA,
            0.0f, 0.0f, 0.0f, 1.0f);
      if (dk->src_kind != DK3D_SRC_NONE)
         dkCmdBufBarrier(f->cmdbuf, DkBarrier_Fragments,
               DkInvalidateFlags_Image);
   }
   if (dk->src_kind != DK3D_SRC_NONE)
   {
      dk3d_blit_content(dk, &dk->sc_images[slot], &src);
      /* The 3D engine draws over what the 2D one wrote */
      dkCmdBufBarrier(f->cmdbuf, DkBarrier_Fragments,
            DkInvalidateFlags_Image);
   }
   dk->draw_bound = false;

#ifdef HAVE_MENU
   if (dk->menu_enable && dk->rgui_last >= 0)
   {
      rgui = &dk->rgui[dk->rgui_last];
      dk3d_draw_rgui(dk);
   }
   if (menu_alive)
      menu_driver_frame(menu_alive, video_info);
   else
#endif
   if (video_info->statistics_show)
      font_driver_render_msg(dk, video_info->stat_text,
            video_info->stat_text_len,
            (const struct font_params*)&video_info->osd_stat_params, NULL);
#ifdef HAVE_GFX_WIDGETS
   if (video_info->widgets_active)
      gfx_widgets_frame(video_info);
#endif
   if (msg && *msg)
      font_driver_render_msg(dk, msg, strlen(msg), NULL, NULL);
   dk->recording = false;

   if (dk->src_kind == DK3D_SRC_HW && dk->hw_has_acquire)
   {
      dkQueueWaitFence(dk->queue, &dk->hw_acquire);
      dk->hw_has_acquire = false;
   }
   dkCmdBufSignalFence(f->cmdbuf, &f->done, true);
   list = dkCmdBufFinishList(f->cmdbuf);
   dkQueueSubmitCommands(dk->queue, list);
   dkQueuePresentImage(dk->queue, dk->swapchain, slot);
   f->serial     = ++dk->serial;
   f->done_valid = true;
   dk->last_done = f->done;
   if (owner)
      owner->use_serial = dk->serial;
   if (rgui)
      rgui->use_serial  = dk->serial;

   /* Hard sync: no more than hard_sync_frames behind. Waiting the next
    * slot below already holds the GPU to num_frames - 1 */
   if (dk->hard_sync && dk->hard_sync_frames + 1 < dk->num_frames)
   {
      dk3d_frame_t *w = &dk->frames[(dk->cur + dk->num_frames
            - dk->hard_sync_frames) % dk->num_frames];
      if (w->done_valid)
         dkFenceWait(&w->done, -1);
   }

   /* The next slot is waited now, before retro_run renders into what
    * its sync index names */
   dk->cur = (dk->cur + 1) % dk->num_frames;
   dk3d_slot_recycle(dk, dk->cur);
   return true;
}

/* ---- driver ----------------------------------------------------------- */

static bool dk3d_get_metrics(void *data, enum display_metric_types type,
      float *value)
{
   (void)data;
   if (type != DISPLAY_METRIC_DPI)
      return false;
   /* Docked: 1080p over an assumed 39" TV; handheld: the 6.2" 720p panel */
   *value = (appletGetOperationMode() == AppletOperationMode_Console)
      ? 56.48480f : 236.8717f;
   return true;
}

static bool dk3d_shaders_init(dk3d_t *dk)
{
   const uint8_t *blobs[2];
   uint32_t sizes[2];
   uint32_t total = DK_SHADER_CODE_UNUSABLE_SIZE;
   uint32_t off   = 0;
   DkShader *out[2];
   unsigned i;

   blobs[0] = deko3d_menu_vsh_dksh;
   blobs[1] = deko3d_menu_fsh_dksh;
   sizes[0] = (uint32_t)(deko3d_menu_vsh_dksh_end - deko3d_menu_vsh_dksh);
   sizes[1] = (uint32_t)(deko3d_menu_fsh_dksh_end - deko3d_menu_fsh_dksh);
   out[0]   = &dk->vsh;
   out[1]   = &dk->fsh;
   for (i = 0; i < 2; i++)
      total += DK3D_ALIGN(sizes[i], DK_SHADER_CODE_ALIGNMENT);
   if (!(dk->shader_mem = dk3d_mem_new(dk->device, total,
               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached
               | DkMemBlockFlags_Code)))
      return false;
   for (i = 0; i < 2; i++)
   {
      /* DKSH: a header of four words (magic, header, control, code
       * sizes), the control section, then the code */
      const uint32_t *hdr = (const uint32_t*)blobs[i];
      DkShaderMaker sm;
      if (sizes[i] < 16 || hdr[0] != 0x48534B44u
            || hdr[2] + hdr[3] > sizes[i])
         return false;
      memcpy((uint8_t*)dkMemBlockGetCpuAddr(dk->shader_mem) + off,
            blobs[i] + hdr[2], hdr[3]);
      dkShaderMakerDefaults(&sm, dk->shader_mem, off);
      sm.control = blobs[i];
      dkShaderInitialize(out[i], &sm);
      off += DK3D_ALIGN(hdr[3], DK_SHADER_CODE_ALIGNMENT);
   }
   return true;
}

static bool dk3d_descriptors_init(dk3d_t *dk)
{
   DkSamplerDescriptor *sd;
   DkSampler s;
   unsigned i;
   if (!(dk->desc_mem = dk3d_mem_new(dk->device,
               DK3D_DESC_COUNT * sizeof(DkImageDescriptor),
               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
      return false;
   if (!(dk->sampler_mem = dk3d_mem_new(dk->device,
               2 * sizeof(DkSamplerDescriptor),
               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)))
      return false;
   dk->desc_cpu    = (DkImageDescriptor*)dkMemBlockGetCpuAddr(dk->desc_mem);
   dk->desc_gpu    = dkMemBlockGetGpuAddr(dk->desc_mem);
   dk->sampler_gpu = dkMemBlockGetGpuAddr(dk->sampler_mem);
   sd              = (DkSamplerDescriptor*)dkMemBlockGetCpuAddr(dk->sampler_mem);
   for (i = 0; i < 2; i++)
   {
      dkSamplerDefaults(&s);
      s.minFilter   = i == DK3D_SAMPLER_LINEAR ? DkFilter_Linear : DkFilter_Nearest;
      s.magFilter   = s.minFilter;
      s.wrapMode[0] = DkWrapMode_ClampToEdge;
      s.wrapMode[1] = DkWrapMode_ClampToEdge;
      s.wrapMode[2] = DkWrapMode_ClampToEdge;
      dkSamplerDescriptorInitialize(&sd[i], &s);
   }
   for (i = DK3D_DESC_COUNT; i-- > DK3D_DESC_FIRST_TEX;)
      dk->desc_free[dk->desc_free_count++] = (uint16_t)i;
   return true;
}

/* The 1x1 white the menu samples when it names no texture, sent with
 * the first frame */
static bool dk3d_blank_init(dk3d_t *dk)
{
   static const uint32_t white = 0xffffffffu;
   DkImageView view;
   DkImageRect rect;
   dk3d_tex_t *t = dk3d_tex_new(dk, VIDEO_SCALE_PACK(1, 1),
         DkImageFormat_BGRA8_Unorm, DK3D_DESC_BLANK);
   if (!t)
      return false;
   dk->blank = *t;
   free(t);
   dkImageViewDefaults(&view, &dk->blank.image);
   rect.x = rect.y = rect.z = 0;
   rect.width = rect.height = rect.depth = 1;
   return dk3d_upload(dk, &view, &rect, (const uint8_t*)&white, 4, 4);
}

static void dk3d_free(void *data)
{
   dk3d_t *dk = (dk3d_t*)data;
   unsigned i;
   if (!dk)
      return;
   if (dk->queue)
      dkQueueWaitIdle(dk->queue);
   dk3d_frames_free(dk);
   dk3d_linear_free(dk->sw, DK3D_SW_SLOTS);
   dk3d_linear_free(dk->rgui, DK3D_SW_SLOTS);
   dk3d_linear_free(dk->frame_mem, DK3D_FRAME_MEM_MAX);
   for (i = 0; i < DK3D_HEAP_BLOCKS; i++)
   {
      while (dk->heap[i].free)
      {
         dk3d_range_t *n = dk->heap[i].free->next;
         free(dk->heap[i].free);
         dk->heap[i].free = n;
      }
      if (dk->heap[i].mem)
         dkMemBlockDestroy(dk->heap[i].mem);
   }
   if (dk->sampler_mem)
      dkMemBlockDestroy(dk->sampler_mem);
   if (dk->desc_mem)
      dkMemBlockDestroy(dk->desc_mem);
   if (dk->shader_mem)
      dkMemBlockDestroy(dk->shader_mem);
   dk3d_swapchain_free(dk);
   if (dk->queue)
      dkQueueDestroy(dk->queue);
   if (dk->device)
      dkDeviceDestroy(dk->device);
   free(dk);
}

static void *dk3d_init(const video_info_t *video)
{
   settings_t *settings = config_get_ptr();
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   DkDeviceMaker dm;
   DkQueueMaker  qm;
   unsigned sc;
   dk3d_t *dk = (dk3d_t*)calloc(1, sizeof(*dk));
   if (!dk)
      return NULL;

   sc = settings ? settings->uints.video_max_swapchain_images : 3;
   if (sc < 2)
      sc = 2;
   if (sc > DK3D_MAX_SWAPCHAIN)
      sc = DK3D_MAX_SWAPCHAIN;
   dk->num_swapchain    = sc;
   /* One frame fewer in flight than there are images to show */
   dk->num_frames       = sc - 1 > DK3D_MAX_FRAMES ? DK3D_MAX_FRAMES : sc - 1;
   dk->swap_interval    = video->swap_interval > 0
      ? (unsigned)video->swap_interval : 1;
   dk->hard_sync        = settings && settings->bools.video_hard_sync;
   dk->hard_sync_frames = settings ? settings->uints.video_hard_sync_frames : 0;
   dk->vsync            = video->vsync;
   dk->smooth           = video->smooth;
   dk->rgb32            = video->rgb32;
   dk->keep_aspect      = true;
   dk->should_resize    = true;
   dk->sw_last          = -1;
   dk->sw_lent          = -1;
   dk->rgui_last        = -1;
   dk->menu_alpha       = 1.0f;
   dk->hw_bottom_left   = hwr && hwr->bottom_left_origin;
   dk->win              = nwindowGetDefault();
   dk->op_mode          = (int)appletGetOperationMode();
   dk->surface_dims     = dk3d_mode_dims(dk->op_mode);
   dk->vp.pos           = VIDEO_POS_PACK(0, 0);
   dk->vp.full_dims     = dk->surface_dims;
   dk->vp.dims          = dk->surface_dims;
   /* ortho over the unit square, bottom-up as the device's origin is */
   matrix_4x4_ortho(dk->mvp_default, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   dkDeviceMakerDefaults(&dm);
   dm.flags   = DkDeviceFlags_DepthZeroToOne | DkDeviceFlags_OriginLowerLeft;
   dm.cbDebug = dk3d_debug_cb;
   if (!(dk->device = dkDeviceCreate(&dm)))
      goto error;
   dkQueueMakerDefaults(&qm, dk->device);
   qm.flags = DkQueueFlags_Graphics;
   if (!(dk->queue = dkQueueCreate(&qm)))
      goto error;
   if (     !dk3d_swapchain_init(dk)
         || !dk3d_frames_init(dk)
         || !dk3d_shaders_init(dk)
         || !dk3d_descriptors_init(dk))
      goto error;
   dk3d_slot_recycle(dk, 0);
   if (!dk3d_blank_init(dk))
      goto error;

   dk->hw_iface.interface_type      = RETRO_HW_RENDER_INTERFACE_DEKO3D;
   dk->hw_iface.interface_version   = RETRO_HW_RENDER_INTERFACE_DEKO3D_VERSION;
   dk->hw_iface.handle              = dk;
   dk->hw_iface.device              = dk->device;
   dk->hw_iface.set_image           = dk3d_hw_set_image;
   dk->hw_iface.get_sync_index      = dk3d_hw_get_sync_index;
   dk->hw_iface.get_sync_index_mask = dk3d_hw_get_sync_index_mask;
   dk->hw_iface.wait_sync_index     = dk3d_hw_wait_sync_index;

   video_driver_set_output_dims(dk->surface_dims);
   dk3d_gfx_ctx.get_metrics = dk3d_get_metrics;
   video_context_driver_set(&dk3d_gfx_ctx);
   /* no input driver of this driver's own: the frontend starts the
    * platform's */
   input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);
   RARCH_LOG("[deko3d] %ux%u, %u images, %u frames in flight.\n",
         VIDEO_SCALE_W(dk->surface_dims), VIDEO_SCALE_H(dk->surface_dims),
         dk->num_swapchain, dk->num_frames);
   return dk;

error:
   RARCH_ERR("[deko3d] Initialization failed.\n");
   dk3d_free(dk);
   return NULL;
}

static void dk3d_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)adaptive_vsync_enabled;
   if (!dk)
      return;
   dk->vsync = !toggle;
   if (swap_interval)
      dk->swap_interval = swap_interval;
   if (dk->swapchain)
      dkSwapchainSetSwapInterval(dk->swapchain,
            dk->vsync ? dk->swap_interval : 0);
}

static bool dk3d_alive(void *data) { (void)data; return true; }
static bool dk3d_focus(void *data) { (void)data; return true; }
static bool dk3d_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}
static bool dk3d_has_windowed(void *data) { (void)data; return false; }
static bool dk3d_set_shader(void *data, enum rarch_shader_type type,
      const char *path)
{
   (void)data;
   (void)type;
   (void)path;
   return false;
}
static void dk3d_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void dk3d_viewport_info(void *data, struct video_viewport *vp)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (dk && vp)
      *vp = dk->vp;
}

static void dk3d_set_aspect_ratio(void *data, unsigned aspect_idx)
{
   dk3d_t *dk = (dk3d_t*)data;
   (void)aspect_idx;
   if (dk)
      dk->should_resize = true;
}

static void dk3d_apply_state_changes(void *data)
{
   dk3d_t *dk = (dk3d_t*)data;
   if (dk)
      dk->should_resize = true;
}

static uint32_t dk3d_get_flags(void *data)
{
   uint32_t flags = 0;
   (void)data;
   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   return flags;
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
   dk3d_get_current_sw_framebuffer,
   dk3d_get_hw_render_interface,
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL, /* get_last_present_time */
   dk3d_hw_ring_install,
   dk3d_hw_ring_fence_new,
   dk3d_hw_ring_fence_free,
   dk3d_hw_ring_fence_signal,
   dk3d_hw_ring_fence_wait,
   NULL, /* hw_ring_capture */
   NULL, /* hw_ring_present_slot */
   NULL, /* hw_ring_context_new */
   NULL, /* hw_ring_context_free */
   NULL, /* hw_ring_framebuffer */
   NULL, /* update_texture */
   NULL, /* get_swap_interval_cap */
   NULL, /* texture_lend */
   NULL, /* texture_lend_ready */
   NULL, /* get_last_present_wait */
   NULL, /* set_view_count */
   NULL, /* hw_context_destroying */
   NULL, /* get_headset_refresh */
   NULL, /* set_frame_format */
   dk3d_frame_mem_alloc,
   dk3d_frame_mem_ready,
   dk3d_frame_mem_free
};

static void dk3d_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &dk3d_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool dk3d_gfx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
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
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   dk3d_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   dk3d_gfx_widgets_enabled,
#endif
   NULL, /* invalidate_hw_render_cache */
   NULL, /* read_viewport_hdr */
   &deko3d_font
};
