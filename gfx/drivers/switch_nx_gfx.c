/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - misson20000
 *  Copyright (C) 2018      - m4xw
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

/* The Switch's own video driver, with no graphics library under it.
 *
 * On the Switch it drives the GPU's 2D engine directly, through a
 * channel libnx opens: the core's frame is scaled and converted from
 * the memory it was written to straight into the window's block-linear
 * buffer, and the menus, widgets, overlays and text are blended over it
 * by the same engine from premultiplied surfaces. The window is shown
 * behind the channel's fence, so no CPU pass touches a pixel of the
 * screen and nothing waits on the GPU but the oldest frame in flight.
 * What the 2D engine cannot draw - a rotated or scaled quad, a strip, a
 * texture shaded vertex by vertex - the same channel's 3D engine does,
 * with deko3d's menu shaders.
 *
 * Where there is no such channel (the Switch 2, or a channel that
 * cannot be opened) the same drawing runs on the CPU into the libnx
 * framebuffer: every draw is one operation - a source rectangle, a
 * destination rectangle, a premultiplied colour and a blend - executed
 * by the 2D engine or by a CPU loop. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <formats/image.h>
#include <encodings/utf.h>
#include <gfx/math/matrix_4x4.h>

#include <gfx/scaler/scaler.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/video_frame.h>

#include <switch.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../gfx_display.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../common/switch_defines.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

/* The GPU path: a channel to the GPU's 2D engine. On the Switch through
 * libnx's channel interface; on the Switch 2 through GSV, whose channel,
 * memory and display calls libounce provides, as drako3d does it. */
#if defined(HAVE_LIBNX) && !defined(SWITCH_NX_CPU_ONLY)
#define SWITCH_NX_GPU 1
#ifdef __SWITCH2__
#define NXG_S2 1
#endif
#endif

#define SW_ALIGN(v, a) (((v) + ((a) - 1)) & ~((a) - 1))

/* Page-aligned memory for the GPU to map */
#ifndef SW_ALIGNED_ALLOC
#define SW_ALIGNED_ALLOC(align, size) memalign(align, size)
#define SW_ALIGNED_FREE(p)            free(p)
#endif

/* What the 2D engine takes as a pitch-linear source: its base and its
 * pitch. Ampere's is 256 bytes */
#ifdef NXG_S2
#define SW_PITCH_ALIGN 256
#else
#define SW_PITCH_ALIGN 32
#endif

#ifdef SWITCH_NX_GPU
/* CPU memory the GPU maps */
typedef struct nxg_mem
{
#ifdef NXG_S2
   uint32_t range_id;
#else
   NvMap    map;
#endif
   uint8_t *cpu;
   uint64_t iova;
   uint32_t size;
} nxg_mem_t;
#endif

/* An image the driver draws from: premultiplied RGBA8, R in the low
 * byte as the framebuffer has it. With the GPU it lives in memory the
 * channel maps, and pixels is that memory's CPU view. */
typedef struct sw_surf
{
   uint8_t  *pixels;
#ifdef SWITCH_NX_GPU
   /* Its memory: a range of a heap block (heap set), or a mapping of
    * its own */
   nxg_mem_t mem;
   uint16_t  block;
   bool      heap;
#endif
   uint32_t  pitch;
   unsigned  dims;
} sw_surf_t;

/* One draw: a source rectangle in texels (a negative size flips it),
 * a destination rectangle in screen pixels, top-down, and the colour
 * the source is modulated by, premultiplied. With no source the colour
 * fills the rectangle; when gradient is set the four colours are its
 * top left, top right, bottom left and bottom right corners. */
typedef struct sw_op
{
   const sw_surf_t *src;
   float    sx, sy, sw, sh;
   float    dx, dy, dw, dh;
   uint32_t color[4];
   bool     blend;
   bool     filter;
   bool     gradient;
} sw_op_t;

static gfx_ctx_driver_t switch_fake_ctx;
static font_renderer_t  switch_font;

/* Premultiplied RGBA8, R low, from floats 0..1 */
static uint32_t sw_pack_premul(float r, float g, float b, float a)
{
   unsigned ia, ir, ig, ib;
   if (a < 0.0f)
      a = 0.0f;
   else if (a > 1.0f)
      a = 1.0f;
   ia = (unsigned)(a * 255.0f + 0.5f);
   ir = (unsigned)((r < 0.0f ? 0.0f : r > 1.0f ? 1.0f : r) * a * 255.0f + 0.5f);
   ig = (unsigned)((g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g) * a * 255.0f + 0.5f);
   ib = (unsigned)((b < 0.0f ? 0.0f : b > 1.0f ? 1.0f : b) * a * 255.0f + 0.5f);
   return ir | (ig << 8) | (ib << 16) | (ia << 24);
}

/* x * y / 255, rounded, exact for bytes */
#define SW_MUL255(x, y, t) ((t) = (x) * (y) + 128u, ((t) + ((t) >> 8)) >> 8)

/* @s scaled by premultiplied @c, channel by channel */
static INLINE uint32_t sw_modulate(uint32_t s, uint32_t c)
{
   uint32_t t, r, g, b, a;
   if (c == 0xffffffffu)
      return s;
   r = SW_MUL255( s        & 0xffu,  c        & 0xffu, t);
   g = SW_MUL255((s >>  8) & 0xffu, (c >>  8) & 0xffu, t);
   b = SW_MUL255((s >> 16) & 0xffu, (c >> 16) & 0xffu, t);
   a = SW_MUL255( s >> 24,           c >> 24,          t);
   return r | (g << 8) | (b << 16) | (a << 24);
}

/* Premultiplied @s over @d; @d keeps its alpha byte */
static INLINE uint32_t sw_over(uint32_t s, uint32_t d)
{
   uint32_t ia = 255u - (s >> 24);
   uint32_t rb, g;
   if (ia == 255u)
      return d;
   if (!ia)
      return (s & 0x00ffffffu) | (d & 0xff000000u);
   rb = (d & 0x00ff00ffu) * ia + 0x00800080u;
   rb = ((rb + ((rb >> 8) & 0x00ff00ffu)) >> 8) & 0x00ff00ffu;
   g  = ((d >> 8) & 0xffu) * ia + 0x80u;
   g  = ((g + (g >> 8)) >> 8) & 0xffu;
   rb += s & 0x00ff00ffu;
   g  += (s >> 8) & 0xffu;
   /* no carry: s is premultiplied, so s + d * (1 - a) stays a byte */
   return (rb & 0x00ff00ffu) | ((g & 0xffu) << 8) | (d & 0xff000000u);
}

/* ---- the GPU ---------------------------------------------------------- */
#ifdef SWITCH_NX_GPU

/* The 3D engine draws what the 2D engine cannot. On the Switch 2 only
 * with NXG_S2_3D, until it has been seen to run on one: its programs
 * from NAK (deko3d_shaders/menu_sm86.h), the rest as the Switch's. */
#if !defined(NXG_S2)
#define NXG_3D
#include "deko3d_shaders/menu_dksh.h"
#elif defined(NXG_S2_3D)
#define NXG_3D
#include "deko3d_shaders/menu_sm86.h"
#endif
/* The menu's effects: DKSH on the Switch, NAK's on the Switch 2 */
#if defined(NXG_3D) && defined(HAVE_SHADERPIPELINE)
#define NXG_3D_FX
#endif

#define NXG_FRAMES      3
#define NXG_BUFFERS     3
#define NXG_SW_SLOTS    (NXG_FRAMES + 2)
#define NXG_RGUI_SLOTS  (NXG_FRAMES + 1)
#define NXG_FRAME_MEM   8
#define NXG_CMD_WORDS   (256 * 1024)
#define NXG_ARENA_SIZE  (512 * 1024)
/* Surfaces share blocks mapped once: no map call per texture */
#define NXG_HEAP_BLOCK  (8 * 1024 * 1024)
#define NXG_HEAP_BLOCKS 16
#define NXG_FB_W        1920
#define NXG_FB_H        1080
/* Sixteen GOBs a block, as libnx's own framebuffer and the TRM have */
#define NXG_BLOCK_LOG2  4
#define NXG_FB_PITCH    SW_ALIGN(NXG_FB_W * 4, 64)
#define NXG_FB_ROWS     SW_ALIGN(NXG_FB_H, 8 << NXG_BLOCK_LOG2)
#define NXG_FB_SIZE     (NXG_FB_PITCH * NXG_FB_ROWS)

/* Command words: method, subchannel, count and mode */
#define NXG_INCR        1
#define NXG_NONINCR     3
#define NXG_INLINE      4
#define NXG_ONCE        5 /* the first word to the method, the rest to the next */
#define NXG_HDR(mode, count, sub, method) \
   ((uint32_t)(method) | ((uint32_t)(sub) << 13) \
    | ((uint32_t)(count) << 16) | ((uint32_t)(mode) << 29))

#define NXG_SUB_3D      0
#define NXG_SUB_2D      3
#ifdef NXG_S2
#define NXG_CLASS_3D    0xC797 /* AMPERE_B */
#else
#define NXG_CLASS_3D    0xB197 /* MAXWELL_B */
#endif
#define NXG_CLASS_2D    0x902D /* FERMI_TWOD_A */

/* Host methods */
#define NXG_HOST_MEMOP_A        0x00A
#define NXG_HOST_SYNCPT_PAYLOAD 0x01C
#define NXG_HOST_WFI            0x01E
#define NXG_MEMOP_L2_FLUSH_DIRTY (16u << 27)

/* The 3D class: what the end of a frame signals with on the Switch */
#define NXG_3D_SYNCPT_ACTION    0x0B2
#define NXG_3D_FLUSH            0x451
#define NXG_SYNCPT_FLUSH        (1u << 16)
#define NXG_SYNCPT_INCR         (1u << 20)

/* FERMI_TWOD_A */
#define NXG_2D_DST_FORMAT       0x080
#define NXG_2D_DST_WIDTH        0x086
#define NXG_2D_SRC_FORMAT       0x08C
#define NXG_2D_TEX_INVALIDATE   0x090
#define NXG_2D_SRC_PITCH        0x091
#define NXG_2D_CLIP_ENABLE      0x0A4
#define NXG_2D_BETA1            0x0A9
#define NXG_2D_BETA4            0x0AA
#define NXG_2D_OPERATION        0x0AB
#define NXG_2D_COMPRESSION      0x0B5
#define NXG_2D_CORRAL_SIZE      0x221
#define NXG_2D_SAMPLE_MODE      0x223
#define NXG_2D_PIXELS_DST_X0    0x22C

#define NXG_OP_SRCCOPY_PREMULT  5
#define NXG_OP_BLEND_PREMULT    6
#define NXG_SAMPLE_CORNER       1u
#define NXG_SAMPLE_BILINEAR     (1u << 4)

/* Colour surface formats */
#define NXG_FMT_RGBA8           0xD5
#define NXG_FMT_BGRX8           0xE6
#define NXG_FMT_B5G6R5          0xE8

#ifdef NXG_S2
/* nvhost ioctls, as drako3d issues them (dir << 30 | size << 16 |
 * type << 8 | nr) */
#define NXG_IOC(dir, size, type, nr) \
   (((u32)(dir) << 30) | ((u32)(size) << 16) | ((u32)(type) << 8) | (u32)(nr))
#define NXG_GP_GET_CHARACTERISTICS NXG_IOC(3, 0xE8, 0x47, 0x05)
#define NXG_GP_SET_CLIENT_HANDLE   NXG_IOC(1, 0x08, 0x47, 0x14)
#define NXG_GP_GET_ENGINE_LIST     NXG_IOC(3, 0x10, 0x47, 0x1F)
#define NXG_AS_BIND_CHANNEL        NXG_IOC(1, 0x04, 0x41, 0x01)
#define NXG_AS_GET_VA_REGIONS      NXG_IOC(3, 0x40, 0x41, 0x08)
#define NXG_AS_INITIALIZE_EX       NXG_IOC(1, 0x28, 0x41, 0x09)
#define NXG_CH_SET_TIMEOUT         NXG_IOC(1, 0x04, 0x48, 0x03)
#define NXG_CH_ALLOC_OBJ_CTX       NXG_IOC(3, 0x10, 0x48, 0x09)
#define NXG_CH_SET_ERROR_NOTIFIER  NXG_IOC(3, 0x18, 0x48, 0x0C)
#define NXG_CH_ALLOC_GPFIFO_EX2    NXG_IOC(3, 0x20, 0x48, 0x1A)
#define NXG_CH_KICKOFF_PB          NXG_IOC(3, 0x18, 0x48, 0x1B)
#define NXG_CH_SETUP_USERD         NXG_IOC(3, 0x10, 0x48, 0x1E)
#define NXG_CH_SET_GR_MODE         NXG_IOC(3, 0x08, 0x48, 0x1F)
#define NXG_CH_SET_ENGINE          NXG_IOC(1, 0x04, 0x48, 0x20)
#define NXG_TSG_SET_TIMESLICE      NXG_IOC(3, 0x04, 0x54, 0x08)
#define NXG_TSG_BIND_CHANNEL_EX    NXG_IOC(3, 0x18, 0x54, 0x0B)
#define NXG_KICKOFF_FLAGS          0x6 /* fence out, base */
#define NXG_GPFIFO_ENTRIES         0x800
#define NXG_KIND_GENERIC           0x06
#define NXG_CLIENT_NAME            0x78616e7872ULL /* "rxnax" */
#define NXG_INVALID_FD             0xffffffffu

typedef struct nxg_va
{
   struct nxg_va *next;
   uint64_t start;
   uint64_t end;
} nxg_va_t;

/* A window buffer as the display knows it */
typedef struct nxg_s2_buf
{
   GsvNativeSync release;
   uint32_t      mem_id;
   uint32_t      image_id;
   uint8_t       layer_slot;
   bool          has_mem;
   bool          has_image;
   bool          has_alias;
} nxg_s2_buf_t;
#endif

/* A free range of a heap block, and a range let go of with a frame */
typedef struct nxg_range
{
   struct nxg_range *next;
   uint32_t off;
   uint32_t size;
} nxg_range_t;

typedef struct nxg_heap
{
   nxg_mem_t    mem;
   nxg_range_t *free;
} nxg_heap_t;

typedef struct nxg_hretire
{
   uint32_t off;
   uint32_t size;
   uint16_t block;
} nxg_hretire_t;

typedef struct nxg_frame
{
   NvFence    fence;
   uint64_t   serial;
   nxg_mem_t  cmd;
   nxg_mem_t  arena;
   nxg_mem_t *retire;
   nxg_hretire_t *hretire;
   unsigned   retire_count;
   unsigned   retire_cap;
   unsigned   hretire_count;
   unsigned   hretire_cap;
   uint32_t   used;
   uint32_t   arena_off;
   bool       fence_valid;
} nxg_frame_t;

/* Memory the CPU writes a frame into and the 2D engine reads in place:
 * a ring slot, a RGUI slot or the threaded wrapper's frame memory */
typedef struct nxg_linear
{
   nxg_mem_t  mem;
   uint64_t   use_serial;
   uint32_t   pitch;
   unsigned   dims;
   bool       used;
} nxg_linear_t;

#ifdef NXG_3D
/* A program as the 3D engine is told of it */
typedef struct nxg_prog
{
#ifdef NXG_S2
   uint64_t addr;       /* of its header */
   uint32_t prefetch;   /* 256-byte units */
#endif
   uint32_t offset;     /* of its entry, in the code memory */
   uint32_t gprs;
   uint32_t end;        /* of its code */
   uint32_t param_d8;
   uint32_t param_65b;
   uint32_t param_489;
   bool     early;
   bool     post_depth;
   bool     per_sample;
} nxg_prog_t;

#ifdef NXG_3D_FX
#define NXG_3D_EFFECTS 16
#define NXG_3D_MESHES  4

/* A menu effect's programs, as effect_install made them */
typedef struct nxg_fx
{
   nxg_prog_t vsh;
   nxg_prog_t fsh;
   unsigned   flags;
} nxg_fx_t;

/* An effect mesh's positions where the GPU reads them, made once */
typedef struct nxg_mesh
{
   nxg_mem_t mem;
   uint32_t  id;
   unsigned  count;
} nxg_mesh_t;
#endif
#endif

typedef struct nxg
{
   nxg_frame_t     frames[NXG_FRAMES];
   nxg_linear_t    sw[NXG_SW_SLOTS];
   nxg_linear_t    rgui[NXG_RGUI_SLOTS];
   nxg_linear_t    frame_mem[NXG_FRAME_MEM];
   nxg_heap_t      heap[NXG_HEAP_BLOCKS];
   nxg_mem_t       fb;
   sw_surf_t       white;
#ifdef NXG_3D
   nxg_mem_t       code3d;
   nxg_mem_t       pool3d;
   nxg_mem_t       cb3d;
   nxg_prog_t      vsh;
   nxg_prog_t      fsh;
   /* The frame's descriptors, the last one written; what the engine
    * was last given */
   const sw_surf_t *tic_surf;
   uint64_t        tic_iova;
   unsigned        tic_dims;
   uint32_t        tic_last;
   uint32_t        tic_used;
   uint32_t        tex_handle;
   uint32_t        scissor3d[2];
   float           mvp3d[16];
   int             blend3d;
   bool            has3d;
   bool            engine3d;
   bool            mvp3d_valid;
#ifdef NXG_3D_FX
   nxg_fx_t        fx[NXG_3D_EFFECTS];
   nxg_mesh_t      meshes[NXG_3D_MESHES];
   uint32_t        code3d_used;
   unsigned        fx_count;
   unsigned        mesh_next;
   /* The programs bound (-1 the menu's, else an effect's), the blend
    * factors (an enum nxg_blendfn), the vertices an effect's */
   int             prog3d;
   int             blendfn3d;
   bool            layout_fx;
   bool            code3d_dirty;
#endif
#endif
#ifdef NXG_S2
   nxg_s2_buf_t    bufs[NXG_BUFFERS];
   GsvGpu          gpu;
   GsvSync         sync;
   Event           worker_event;
   Event           map_event;
   Event           wait_event;
   Vi2Layer       *layer;
   nxg_va_t       *va_free;
   uint32_t        worker_slot;
   uint32_t        map_slot;
   uint32_t        wait_slot;
   uint32_t        vm_id;
   uint32_t        ctrl_fd;
   uint32_t        as_fd;
   uint32_t        tsg_fd;
   uint32_t        ch_fd;
   uint32_t        engine_id;
   uint32_t        producer_id;
   uint8_t         client;
   bool            sync_open;
   bool            gpu_open;
   bool            worker_event_open;
   bool            map_event_open;
   bool            wait_slot_open;
   bool            client_open;
   bool            layer_open;
#else
   NvAddressSpace  as;
   NvGpuChannel    ch;
   bool            as_open;
   bool            ch_open;
   bool            win_configured;
#endif
   /* What the last frame was blitted from, for a dupe */
   nxg_linear_t   *last_owner;
   uint64_t        last_iova;
   uint32_t        last_pitch;
   unsigned        last_dims;
   unsigned        last_x;
   uint64_t        serial;
   uint64_t        done_serial;
   unsigned        out_dims;
   unsigned        src_dims;
   uint32_t        src_pitch;
   uint64_t        src_iova;
   uint32_t        src_fmt;
   unsigned        cur;
   int             slot;
   int             op_mode;
   int             sw_last;
   int             sw_lent;
   int             rgui_last;
   bool            nv_open;
   bool            src_valid;
   bool            recording;
   bool            swap_interval_set;
} nxg_t;

static unsigned nxg_mode_dims(int op_mode)
{
   return op_mode == AppletOperationMode_Console
      ? VIDEO_SCALE_PACK(1920, 1080) : VIDEO_SCALE_PACK(1280, 720);
}

/* ---- the platform: memory, fences, the channel, the window ------------ */
#ifdef NXG_S2

/* The VM's addresses, first fit, in 64 KiB units */
static uint64_t nxg_va_alloc(nxg_t *g, uint64_t size)
{
   nxg_va_t **pp;
   size = SW_ALIGN(size, 0x10000);
   for (pp = &g->va_free; *pp; pp = &(*pp)->next)
   {
      nxg_va_t *r = *pp;
      if (r->end - r->start >= size)
      {
         uint64_t va = r->start;
         r->start   += size;
         if (r->start == r->end)
         {
            *pp = r->next;
            free(r);
         }
         return va;
      }
   }
   return 0;
}

static void nxg_va_free(nxg_t *g, uint64_t va, uint64_t size)
{
   nxg_va_t **pp = &g->va_free;
   nxg_va_t *prev = NULL, *n;
   size = SW_ALIGN(size, 0x10000);
   while (*pp && (*pp)->start < va)
   {
      prev = *pp;
      pp   = &(*pp)->next;
   }
   if (prev && prev->end == va)
   {
      prev->end += size;
      if (prev->next && prev->end == prev->next->start)
      {
         n          = prev->next;
         prev->end  = n->end;
         prev->next = n->next;
         free(n);
      }
      return;
   }
   if (*pp && va + size == (*pp)->start)
   {
      (*pp)->start = va;
      return;
   }
   if (!(n = (nxg_va_t*)malloc(sizeof(*n))))
      return;
   n->start = va;
   n->end   = va + size;
   n->next  = *pp;
   *pp      = n;
}

/* The worker's answer to a mapping request */
static void nxg_map_wait(nxg_t *g)
{
   if (g->map_event_open)
      eventWait(&g->map_event, 5000000000ULL);
   else
   {
      eventWait(&g->worker_event, 5000000000ULL);
      eventClear(&g->worker_event);
   }
}

static bool nxg_mem_new(nxg_t *g, nxg_mem_t *m, uint32_t size,
      bool gpu_cached, bool block_linear)
{
   GsvGpuMapping mapping;
   u8 kind = 0;
   (void)block_linear; /* one generic kind for both on Ampere */
   size = SW_ALIGN(size, 0x10000);
   memset(m, 0, sizeof(*m));
   if (!(m->cpu = (uint8_t*)SW_ALIGNED_ALLOC(0x1000, size)))
      return false;
   armDCacheFlush(m->cpu, size);
   if (R_FAILED(svcUnknown80(m->cpu, size, 0x20, 0x20)))
      goto fail_cpu;
   if (R_FAILED(gsvGpuRegisterMemory(&g->gpu, (u64)(uintptr_t)m->cpu,
               size, g->client, &m->range_id)))
      goto fail_attr;
   if (!(m->iova = nxg_va_alloc(g, size)))
      goto fail_reg;
   memset(&mapping, 0, sizeof(mapping));
   mapping.vm_id         = g->vm_id;
   mapping.va            = m->iova;
   mapping.size          = size;
   mapping.range_id      = m->range_id;
   mapping.range_offset  = 0;
   mapping.kind          = NXG_KIND_GENERIC;
   mapping.fallback_kind = NXG_KIND_GENERIC;
   mapping.gpu_cacheable = gpu_cached;
   mapping.client        = g->client;
   if (R_FAILED(gsvGpuMap(&g->gpu, &mapping, g->map_slot, &kind)))
      goto fail_va;
   nxg_map_wait(g);
   m->size = size;
   return true;

fail_va:
   nxg_va_free(g, m->iova, size);
fail_reg:
   if (R_SUCCEEDED(gsvGpuUnregisterMemory(&g->gpu, m->range_id, g->map_slot)))
      nxg_map_wait(g);
fail_attr:
   svcUnknown80(m->cpu, size, 0x20, 0);
fail_cpu:
   SW_ALIGNED_FREE(m->cpu);
   m->cpu = NULL;
   return false;
}

static void nxg_mem_free(nxg_t *g, nxg_mem_t *m)
{
   if (!m->cpu)
      return;
   if (R_SUCCEEDED(gsvGpuUnmap(&g->gpu, g->vm_id, m->iova, m->size,
               g->map_slot)))
      nxg_map_wait(g);
   nxg_va_free(g, m->iova, m->size);
   if (R_SUCCEEDED(gsvGpuUnregisterMemory(&g->gpu, m->range_id,
               g->map_slot)))
      nxg_map_wait(g);
   svcUnknown80(m->cpu, m->size, 0x20, 0);
   SW_ALIGNED_FREE(m->cpu);
   m->cpu  = NULL;
   m->size = 0;
}

/* A producer counter's target in the form the sync session compares:
 * 64 bits when the counter is, around its current value */
static void nxg_fence_target(nxg_t *g, const NvFence *f, u64 *target,
      u8 *mode)
{
   u64 cur = 0;
   *target = f->value;
   *mode   = 0;
   if (     R_SUCCEEDED(gsvSyncReadCounter(&g->sync, f->id, &cur, mode))
         && *mode == 1 && cur != UINT64_MAX)
      *target = cur + (u64)(s64)(s32)(f->value - (u32)cur);
   else
      *mode = 0;
}

static bool nxg_fence_done(nxg_t *g, const NvFence *f)
{
   u64 v = 0;
   if ((s32)f->id < 0)
      return true;
   if (     R_FAILED(gsvSyncReadCounter(&g->sync, f->id, &v, NULL))
         || v == UINT64_MAX)
      return true;
   return (s32)((u32)v - f->value) >= 0;
}

/* Blocks on an event the sync session signals at the target: no sleep */
static void nxg_fence_wait(nxg_t *g, const NvFence *f)
{
   while (!nxg_fence_done(g, f))
   {
      u64 target;
      u8 mode;
      if (!g->wait_slot_open)
      {
         svcSleepThread(50000);
         continue;
      }
      nxg_fence_target(g, f, &target, &mode);
      if (R_FAILED(gsvSyncArmEvent(&g->sync, g->wait_slot, f->id, target,
                  mode)))
      {
         svcSleepThread(50000);
         continue;
      }
      if (!nxg_fence_done(g, f))
         eventWait(&g->wait_event, 1000000000ULL);
      gsvSyncDisarmEvent(&g->sync, g->wait_slot);
   }
}

static void nxg_native_from_fence(nxg_t *g, const NvFence *f,
      GsvNativeSync *out)
{
   u32 id = ~f->id;
   u64 target;
   u8 mode;
   nxg_fence_target(g, f, &target, &mode);
   memset(out, 0, sizeof(*out));
   memcpy(&out->data[0], &id, sizeof(id));
   out->data[4] = mode;
   memcpy(&out->data[8], &target, sizeof(target));
}

static void nxg_native_wait(nxg_t *g, GsvNativeSync *s)
{
   unsigned i;
   bool any = false;
   for (i = 0; i < sizeof(s->data); i++)
      any |= s->data[i] != 0;
   if (!any)
      return;
   {
      u32 id;
      u64 target;
      NvFence f;
      memcpy(&id, &s->data[0], sizeof(id));
      memcpy(&target, &s->data[8], sizeof(target));
      if (s->data[4] <= 1)
      {
         f.id    = ~id;
         f.value = (u32)target;
         nxg_fence_wait(g, &f);
      }
      else
         gsvSyncWaitNative(&g->sync, s, UINT64_MAX);
   }
   memset(s, 0, sizeof(*s));
}

static bool nxg_device_init(nxg_t *g)
{
   GsvGpuVaRange ranges[4];
   u32 nranges = 0, last_slot = 0;
   g->ctrl_fd = g->as_fd = g->tsg_fd = g->ch_fd = NXG_INVALID_FD;

   if (R_FAILED(nvInitialize()))
      return false;
   g->nv_open = true;
   if (R_FAILED(gsvOpenSync(&g->sync)))
      return false;
   g->sync_open = true;
   /* The window's layer may have registered already */
   gsvRegisterFrame(2, NULL);
   if (R_FAILED(gsvOpenGpu(&g->gpu)))
      return false;
   g->gpu_open = true;
   if (     R_FAILED(gsvGpuGetSlotRange(&g->gpu, &g->worker_slot, &last_slot))
         || last_slot < g->worker_slot)
      return false;
   if (R_FAILED(gsvGpuCreateSlotEvent(&g->gpu, g->worker_slot,
               &g->worker_event, false)))
      return false;
   g->worker_event_open = true;
   if (R_FAILED(gsvGpuOpenWorker(&g->gpu, g->worker_slot, &g->worker_event)))
      return false;
   g->map_slot = g->worker_slot;
   if (     last_slot > g->worker_slot
         && R_SUCCEEDED(gsvGpuCreateSlotEvent(&g->gpu, g->worker_slot + 1,
               &g->map_event, true)))
   {
      g->map_slot       = g->worker_slot + 1;
      g->map_event_open = true;
   }
   if (     R_SUCCEEDED(gsvSyncAllocateSlot(&g->sync, &g->wait_slot))
         && R_SUCCEEDED(gsvSyncCreateEvent(&g->sync, g->wait_slot,
               &g->wait_event, true)))
      g->wait_slot_open = true;
   if (R_FAILED(gsvGpuRegisterClient(&g->gpu, NXG_CLIENT_NAME, &g->client)))
      return false;
   g->client_open = true;
   if (R_FAILED(gsvGpuCreateVm(&g->gpu, &g->vm_id)))
      return false;
   memset(ranges, 0, sizeof(ranges));
   if (     R_FAILED(gsvGpuGetVmUserRanges(&g->gpu, g->vm_id, ranges, &nranges))
         || !nranges)
      return false;
   if (R_FAILED(gsvRegisterVm(g->vm_id)))
      return false;

   if (R_FAILED(nvOpen(&g->ctrl_fd, "/dev/nvhost-ctrl-gpu")))
      return false;
   {
      /* The graphics engine: the entry with both type words zero */
      struct { u32 a, b, id, c; } engines[0x400 / 16];
      u32 req[4], i, n;
      memset(engines, 0, sizeof(engines));
      memset(req, 0, sizeof(req));
      if (R_FAILED(nvIoctl3(g->ctrl_fd, NXG_GP_GET_ENGINE_LIST, req,
                  engines, sizeof(engines))))
         return false;
      n            = req[0] / sizeof(engines[0]);
      g->engine_id = NXG_INVALID_FD;
      for (i = 0; i < n && i < ARRAY_SIZE(engines); i++)
         if (!engines[i].a && !engines[i].b)
            g->engine_id = engines[i].id;
      if (g->engine_id == NXG_INVALID_FD)
         return false;
   }

   /* The VM imported as the address space, its regions the heap */
   if (R_FAILED(nvOpen(&g->as_fd, "/dev/nvhost-as-gpu")))
      return false;
   {
      struct
      {
         u32 flags, vm_id, big_page_size, reserved;
         u64 va_range_start, va_range_end, va_range_split;
      } init;
      struct
      {
         u64 buf_addr;
         u32 buf_size;
         u32 reserved;
         u8  inline_regions[0x30];
      } query;
      struct { u64 offset; u32 page_size; u32 reserved; u64 pages; } regions[2];
      u32 size, i;
      memset(&init, 0, sizeof(init));
      init.flags          = 5;
      init.vm_id          = g->vm_id;
      init.va_range_start = ranges[0].first;
      init.va_range_end   = ranges[0].last + 1;
      if (R_FAILED(nvIoctl(g->as_fd, NXG_AS_INITIALIZE_EX, &init)))
         return false;
      memset(&query, 0, sizeof(query));
      if (R_FAILED(nvIoctl(g->as_fd, NXG_AS_GET_VA_REGIONS, &query)))
         return false;
      size = query.buf_size;
      if (size != sizeof(regions[0]) && size != sizeof(regions))
         return false;
      memset(&query, 0, sizeof(query));
      memset(regions, 0, sizeof(regions));
      query.buf_size = size;
      if (R_FAILED(nvIoctl3(g->as_fd, NXG_AS_GET_VA_REGIONS, &query,
                  regions, size)))
         return false;
      for (i = 0; i < size / sizeof(regions[0]); i++)
      {
         uint64_t start = SW_ALIGN(regions[i].offset, 0x10000ULL);
         uint64_t end   = regions[i].offset
            + regions[i].pages * regions[i].page_size;
         if (start < ranges[0].first)
            start = SW_ALIGN(ranges[0].first, 0x10000ULL);
         if (end > ranges[0].last + 1)
            end = ranges[0].last + 1;
         end &= ~(uint64_t)0xffff;
         if (start < end)
            nxg_va_free(g, start, end - start);
      }
      if (!g->va_free)
         return false;
   }

   /* The channel: a TSG on the graphics engine, the 3D class allocated,
    * the 2D class bound in the stream */
   if (R_FAILED(nvOpen(&g->tsg_fd, "/dev/nvhost-tsg-gpu")))
      return false;
   if (R_FAILED(nvOpen(&g->ch_fd, "/dev/nvhost-gpu")))
      return false;
   {
      u32 bind[6], timeout = 1000, timeslice = 1024, mode[2];
      u64 userd[2], cookie = (u64)(uintptr_t)g;
      struct { u32 num_entries, num_jobs, flags, producer_id; u32 reserved[4]; } alloc;
      struct { u32 class_num, flags; u64 obj_id; } obj;
      struct { u64 offset, size; u32 mem, padding; } notifier;
      if (R_FAILED(nvIoctl(g->ch_fd, NXG_CH_SET_ENGINE, &g->engine_id)))
         return false;
      if (R_FAILED(nvIoctl(g->as_fd, NXG_AS_BIND_CHANNEL, &g->ch_fd)))
         return false;
      memset(bind, 0, sizeof(bind));
      bind[0] = g->ch_fd;
      if (R_FAILED(nvIoctl(g->tsg_fd, NXG_TSG_BIND_CHANNEL_EX, bind)))
         return false;
      memset(&alloc, 0, sizeof(alloc));
      alloc.num_entries = NXG_GPFIFO_ENTRIES;
      alloc.num_jobs    = 8;
      alloc.flags       = 2;
      if (     R_FAILED(nvIoctl(g->ch_fd, NXG_CH_ALLOC_GPFIFO_EX2, &alloc))
            || !alloc.producer_id)
         return false;
      g->producer_id = alloc.producer_id;
      memset(userd, 0, sizeof(userd));
      if (     R_FAILED(nvIoctl(g->ch_fd, NXG_CH_SETUP_USERD, userd))
            || !userd[0])
         return false;
      memset(&obj, 0, sizeof(obj));
      obj.class_num = NXG_CLASS_3D;
      obj.obj_id    = 0xDEADBEEF;
      if (R_FAILED(nvIoctl(g->ch_fd, NXG_CH_ALLOC_OBJ_CTX, &obj)))
         return false;
      memset(&notifier, 0, sizeof(notifier));
      notifier.size = 0x1000;
      notifier.mem  = 1;
      nvIoctl(g->ch_fd, NXG_CH_SET_ERROR_NOTIFIER, &notifier);
      nvIoctl(g->ch_fd, NXG_GP_SET_CLIENT_HANDLE, &cookie);
      nvIoctl(g->ch_fd, NXG_CH_SET_TIMEOUT, &timeout);
      mode[0] = 0;
      mode[1] = 2;
      nvIoctl(g->ch_fd, NXG_CH_SET_GR_MODE, mode);
      nvIoctl(g->tsg_fd, NXG_TSG_SET_TIMESLICE, &timeslice);
   }
   return true;
}

/* Each window buffer registered with the display, a slot of the
 * layer each */
static bool nxg_window_init(nxg_t *g, NWindow *win)
{
   GsvImageLayout layout;
   u64 size = 0, avail = 0;
   unsigned i;
   gsvMakeImageLayout(&layout, NXG_FB_W, NXG_FB_H, NXG_BLOCK_LOG2 + 3,
         NXG_FB_PITCH / 64, 1 /* RGBA8 */, 1 /* block-linear */, 0);
   if (R_FAILED(gsvGetImageSize(&layout, &size)) || size > NXG_FB_SIZE)
      return false;
   if (R_FAILED(nwindowGetVi2Layer(win, &g->layer)))
      return false;
   g->layer_open = true;
   if (R_FAILED(vi2GetLayerAvailability(g->layer, &avail)))
      return false;
   for (i = 0; i < NXG_BUFFERS; i++)
   {
      nxg_s2_buf_t *b = &g->bufs[i];
      u64 off         = (u64)i * NXG_FB_SIZE;
      if (!avail)
         return false;
      b->layer_slot = (uint8_t)__builtin_ctzll(avail);
      avail        &= avail - 1;
      if (R_FAILED(gsvReserveImageMemory(3, size, &b->mem_id)))
         return false;
      b->has_mem = true;
      if (R_FAILED(gsvRegisterImageMemory(b->mem_id,
                  (u64)(uintptr_t)g->fb.cpu + off, size, g->fb.range_id, off)))
         return false;
      if (R_FAILED(gsvCreateImage(&layout, b->mem_id, &b->image_id)))
         return false;
      b->has_image = true;
      if (R_FAILED(gsvCreateDisplayAlias(b->image_id, 1)))
         return false;
      b->has_alias = true;
   }
   return true;
}

static void nxg_platform_free(nxg_t *g, NWindow *win)
{
   unsigned i;
   if (g->layer_open)
   {
      Vi2PresentOptions o;
      u64 slots = 0;
      GsvNativeSync release;
      if (R_SUCCEEDED(vi2GetLayerAvailability(g->layer, &slots)) && slots)
      {
         vi2MakeBasicPresentOptions(&o, NXG_FB_W, NXG_FB_H,
               armGetSystemTick());
         o.flags &= ~1ULL;
         memset(&release, 0, sizeof(release));
         vi2UnregisterLayerImage(g->layer, (u8)__builtin_ctzll(slots), &o,
               &slots, &release);
      }
   }
   for (i = 0; i < NXG_BUFFERS; i++)
   {
      nxg_s2_buf_t *b = &g->bufs[i];
      nxg_native_wait(g, &b->release);
      if (b->has_alias)
         gsvDestroyDisplayAlias(b->image_id);
      if (b->has_image)
         gsvDestroyImage(b->image_id);
      if (b->has_mem)
         gsvReleaseImageMemory(b->mem_id);
   }
   if (g->layer_open)
      nwindowReleaseVi2Layer(win);
}

static void nxg_device_free(nxg_t *g)
{
   while (g->va_free)
   {
      nxg_va_t *n = g->va_free->next;
      free(g->va_free);
      g->va_free = n;
   }
   if (g->ch_fd != NXG_INVALID_FD)
      nvClose(g->ch_fd);
   if (g->tsg_fd != NXG_INVALID_FD)
      nvClose(g->tsg_fd);
   if (g->as_fd != NXG_INVALID_FD)
      nvClose(g->as_fd);
   if (g->ctrl_fd != NXG_INVALID_FD)
      nvClose(g->ctrl_fd);
   if (g->client_open)
      gsvGpuReleaseClient(&g->gpu, g->client);
   if (g->wait_slot_open)
   {
      eventClose(&g->wait_event);
      gsvSyncFreeSlot(&g->sync, g->wait_slot);
   }
   if (g->map_event_open)
      eventClose(&g->map_event);
   if (g->worker_event_open)
      eventClose(&g->worker_event);
   if (g->gpu_open)
      gsvGpuClose(&g->gpu);
   if (g->sync_open)
      gsvSyncClose(&g->sync);
   if (g->nv_open)
      nvExit();
}

#else /* the Switch */

static bool nxg_mem_new(nxg_t *g, nxg_mem_t *m, uint32_t size,
      bool gpu_cached, bool block_linear)
{
   size = SW_ALIGN(size, 0x1000);
   memset(m, 0, sizeof(*m));
   if (!(m->cpu = (uint8_t*)SW_ALIGNED_ALLOC(0x1000, size)))
      return false;
   if (R_FAILED(nvMapCreate(&m->map, m->cpu, size, 0x20000,
               NvKind_Pitch, true)))
   {
      SW_ALIGNED_FREE(m->cpu);
      m->cpu = NULL;
      return false;
   }
   if (R_FAILED(nvAddressSpaceMap(&g->as, nvMapGetHandle(&m->map),
               gpu_cached, block_linear ? NvKind_Generic_16BX2 : NvKind_Pitch,
               &m->iova)))
   {
      nvMapClose(&m->map);
      SW_ALIGNED_FREE(m->cpu);
      m->cpu = NULL;
      return false;
   }
   m->size = size;
   return true;
}

static void nxg_mem_free(nxg_t *g, nxg_mem_t *m)
{
   if (!m->cpu)
      return;
   nvAddressSpaceUnmap(&g->as, m->iova);
   nvMapClose(&m->map);
   SW_ALIGNED_FREE(m->cpu);
   m->cpu  = NULL;
   m->size = 0;
}

static bool nxg_fence_done(nxg_t *g, const NvFence *f)
{
   NvFence c = *f;
   (void)g;
   return R_SUCCEEDED(nvFenceWait(&c, 0));
}

static void nxg_fence_wait(nxg_t *g, const NvFence *f)
{
   NvFence c = *f;
   (void)g;
   nvFenceWait(&c, -1);
}

static bool nxg_device_init(nxg_t *g)
{
   const nvioctl_gpu_characteristics *chars;
   if (R_FAILED(nvInitialize()))
      return false;
   if (R_FAILED(nvGpuInit()))
   {
      nvExit();
      return false;
   }
   if (R_FAILED(nvFenceInit()))
   {
      nvGpuExit();
      nvExit();
      return false;
   }
   if (R_FAILED(nvMapInit()))
   {
      nvFenceExit();
      nvGpuExit();
      nvExit();
      return false;
   }
   g->nv_open = true;
   chars      = nvGpuGetCharacteristics();
   if (R_FAILED(nvAddressSpaceCreate(&g->as,
               chars ? chars->big_page_size : 0x20000)))
      return false;
   g->as_open = true;
   if (R_FAILED(nvGpuChannelCreate(&g->ch, &g->as, NvChannelPriority_Medium)))
      return false;
   g->ch_open = true;
   return true;
}

/* The window's buffers, block-linear, as libnx's framebuffer makes them */
static bool nxg_window_init(nxg_t *g, NWindow *win)
{
   NvGraphicBuffer grbuf;
   unsigned i;
   if (R_FAILED(nwindowSetDimensions(win, NXG_FB_W, NXG_FB_H)))
      return false;
   memset(&grbuf, 0, sizeof(grbuf));
   grbuf.header.num_ints           = (sizeof(NvGraphicBuffer)
         - sizeof(NativeHandle)) / 4;
   grbuf.unk0                      = -1;
   grbuf.magic                     = 0xDAFFCAFF;
   grbuf.pid                       = 42;
   grbuf.usage                     = GRALLOC_USAGE_HW_COMPOSER
      | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
   grbuf.format                    = PIXEL_FORMAT_RGBA_8888;
   grbuf.ext_format                = PIXEL_FORMAT_RGBA_8888;
   grbuf.num_planes                = 1;
   grbuf.planes[0].width           = NXG_FB_W;
   grbuf.planes[0].height          = NXG_FB_H;
   grbuf.planes[0].color_format    = NvColorFormat_A8B8G8R8;
   grbuf.planes[0].layout          = NvLayout_BlockLinear;
   grbuf.planes[0].kind            = NvKind_Generic_16BX2;
   grbuf.planes[0].block_height_log2 = NXG_BLOCK_LOG2;
   grbuf.nvmap_id                  = nvMapGetId(&g->fb.map);
   grbuf.stride                    = NXG_FB_PITCH / 4;
   grbuf.total_size                = NXG_FB_SIZE;
   grbuf.planes[0].pitch           = NXG_FB_PITCH;
   grbuf.planes[0].size            = NXG_FB_SIZE;
   for (i = 0; i < NXG_BUFFERS; i++)
   {
      grbuf.planes[0].offset = i * NXG_FB_SIZE;
      if (R_FAILED(nwindowConfigureBuffer(win, (s32)i, &grbuf)))
         return false;
   }
   g->win_configured = true;
   return true;
}

static void nxg_platform_free(nxg_t *g, NWindow *win)
{
   if (g->win_configured)
      nwindowReleaseBuffers(win);
}

static void nxg_device_free(nxg_t *g)
{
   if (g->ch_open)
      nvGpuChannelClose(&g->ch);
   if (g->as_open)
      nvAddressSpaceClose(&g->as);
   if (g->nv_open)
   {
      nvMapExit();
      nvFenceExit();
      nvGpuExit();
      nvExit();
   }
}

#endif

/* ---- frames ------------------------------------------------------------ */

/* Let go of once the frame being recorded has passed: no frame still
 * on the GPU reads it then */
static void nxg_retire(nxg_t *g, nxg_mem_t *m)
{
   nxg_frame_t *f = &g->frames[g->cur];
   if (!m->cpu)
      return;
   if (f->retire_count == f->retire_cap)
   {
      unsigned   cap = f->retire_cap ? f->retire_cap * 2 : 16;
      nxg_mem_t *r   = (nxg_mem_t*)realloc(f->retire, cap * sizeof(*r));
      if (!r)
      {
         /* Nowhere to park it: every frame is waited out instead */
         unsigned i;
         for (i = 0; i < NXG_FRAMES; i++)
            if (g->frames[i].fence_valid)
               nxg_fence_wait(g, &g->frames[i].fence);
         nxg_mem_free(g, m);
         return;
      }
      f->retire     = r;
      f->retire_cap = cap;
   }
   f->retire[f->retire_count++] = *m;
   m->cpu = NULL;
}

/* First fit in the heap's blocks, @size rounded to the 2D engine's
 * alignment; a block is mapped when none has room */
static bool nxg_heap_alloc(nxg_t *g, uint32_t size, uint16_t *block,
      uint32_t *off)
{
   unsigned i;
   size = SW_ALIGN(size, 256);
   for (i = 0; i < NXG_HEAP_BLOCKS; i++)
   {
      nxg_heap_t *b = &g->heap[i];
      nxg_range_t **pp;
      if (!b->mem.cpu)
      {
         if (!nxg_mem_new(g, &b->mem, NXG_HEAP_BLOCK, false, false))
            return false;
         if (!(b->free = (nxg_range_t*)malloc(sizeof(*b->free))))
            return false;
         b->free->next = NULL;
         b->free->off  = 0;
         b->free->size = NXG_HEAP_BLOCK;
      }
      for (pp = &b->free; *pp; pp = &(*pp)->next)
      {
         nxg_range_t *r = *pp;
         if (r->size < size)
            continue;
         *block   = (uint16_t)i;
         *off     = r->off;
         r->off  += size;
         r->size -= size;
         if (!r->size)
         {
            *pp = r->next;
            free(r);
         }
         return true;
      }
   }
   return false;
}

static void nxg_heap_free(nxg_t *g, uint16_t block, uint32_t off,
      uint32_t size)
{
   nxg_heap_t *b       = &g->heap[block];
   nxg_range_t **pp    = &b->free;
   nxg_range_t *prev   = NULL, *n;
   size = SW_ALIGN(size, 256);
   while (*pp && (*pp)->off < off)
   {
      prev = *pp;
      pp   = &(*pp)->next;
   }
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
   if (!(n = (nxg_range_t*)malloc(sizeof(*n))))
      return;
   n->off  = off;
   n->size = size;
   n->next = *pp;
   *pp     = n;
}

/* A heap range let go of once the frame being recorded has passed */
static void nxg_heap_retire(nxg_t *g, uint16_t block, uint32_t off,
      uint32_t size)
{
   nxg_frame_t *f = &g->frames[g->cur];
   if (f->hretire_count == f->hretire_cap)
   {
      unsigned cap     = f->hretire_cap ? f->hretire_cap * 2 : 16;
      nxg_hretire_t *r = (nxg_hretire_t*)realloc(f->hretire,
            cap * sizeof(*r));
      if (!r)
      {
         unsigned i;
         for (i = 0; i < NXG_FRAMES; i++)
            if (g->frames[i].fence_valid)
               nxg_fence_wait(g, &g->frames[i].fence);
         nxg_heap_free(g, block, off, size);
         return;
      }
      f->hretire     = r;
      f->hretire_cap = cap;
   }
   f->hretire[f->hretire_count].block = block;
   f->hretire[f->hretire_count].off   = off;
   f->hretire[f->hretire_count].size  = size;
   f->hretire_count++;
}

static void nxg_poll_done(nxg_t *g)
{
   unsigned i;
   for (i = 0; i < NXG_FRAMES; i++)
   {
      nxg_frame_t *f = &g->frames[i];
      if (     f->fence_valid
            && f->serial > g->done_serial
            && nxg_fence_done(g, &f->fence))
         g->done_serial = f->serial;
   }
}

/* Room for @n words in the frame's commands; NULL drops the draw */
static uint32_t *nxg_cmd(nxg_t *g, unsigned n)
{
   nxg_frame_t *f = &g->frames[g->cur];
   uint32_t *p;
   if (f->used + n > NXG_CMD_WORDS)
      return NULL;
   p        = (uint32_t*)f->cmd.cpu + f->used;
   f->used += n;
   return p;
}

/* The one wait on the frame path: the slot about to be recorded */
static void nxg_frame_recycle(nxg_t *g, unsigned i)
{
   nxg_frame_t *f = &g->frames[i];
   unsigned j;
   if (f->fence_valid)
   {
      nxg_fence_wait(g, &f->fence);
      if (f->serial > g->done_serial)
         g->done_serial = f->serial;
   }
   for (j = 0; j < f->retire_count; j++)
      nxg_mem_free(g, &f->retire[j]);
   for (j = 0; j < f->hretire_count; j++)
      nxg_heap_free(g, f->hretire[j].block, f->hretire[j].off,
            f->hretire[j].size);
   f->retire_count  = 0;
   f->hretire_count = 0;
   f->used          = 0;
   f->arena_off     = 0;
}

static void nxg_free(nxg_t *g, NWindow *win)
{
   unsigned i;
   if (!g)
      return;
   for (i = 0; i < NXG_FRAMES; i++)
      if (g->frames[i].fence_valid)
         nxg_fence_wait(g, &g->frames[i].fence);
   nxg_platform_free(g, win);
   for (i = 0; i < NXG_FRAMES; i++)
   {
      nxg_frame_t *f = &g->frames[i];
      unsigned j;
      for (j = 0; j < f->retire_count; j++)
         nxg_mem_free(g, &f->retire[j]);
      free(f->retire);
      free(f->hretire);
      nxg_mem_free(g, &f->arena);
      nxg_mem_free(g, &f->cmd);
   }
   for (i = 0; i < NXG_SW_SLOTS; i++)
      nxg_mem_free(g, &g->sw[i].mem);
   for (i = 0; i < NXG_RGUI_SLOTS; i++)
      nxg_mem_free(g, &g->rgui[i].mem);
   for (i = 0; i < NXG_FRAME_MEM; i++)
      nxg_mem_free(g, &g->frame_mem[i].mem);
   nxg_mem_free(g, &g->white.mem);
#ifdef NXG_3D
#ifdef NXG_3D_FX
   for (i = 0; i < NXG_3D_MESHES; i++)
      nxg_mem_free(g, &g->meshes[i].mem);
#endif
   nxg_mem_free(g, &g->code3d);
   nxg_mem_free(g, &g->pool3d);
   nxg_mem_free(g, &g->cb3d);
#endif
   for (i = 0; i < NXG_HEAP_BLOCKS; i++)
   {
      while (g->heap[i].free)
      {
         nxg_range_t *n = g->heap[i].free->next;
         free(g->heap[i].free);
         g->heap[i].free = n;
      }
      nxg_mem_free(g, &g->heap[i].mem);
   }
   nxg_mem_free(g, &g->fb);
   nxg_device_free(g);
   free(g);
}

#ifdef NXG_3D
static void nxg_3d_turn(nxg_t *g, bool to3d);
static void nxg_3d_frame_begin(nxg_t *g, uint64_t fb);
static bool nxg_3d_init(nxg_t *g);
#endif

static void nxg_bind_engines(nxg_t *g)
{
   uint32_t *w = nxg_cmd(g, 9);
   w[0]  = NXG_HDR(NXG_NONINCR, 1, NXG_SUB_3D, 0);
   w[1]  = NXG_CLASS_3D;
   w[2]  = NXG_HDR(NXG_NONINCR, 1, NXG_SUB_2D, 0);
   w[3]  = NXG_CLASS_2D;
   w[4]  = NXG_HDR(NXG_INCR, 1, NXG_SUB_2D, NXG_2D_BETA1);
   w[5]  = 0xFFu << 23;
   w[6]  = NXG_HDR(NXG_INLINE, 0, NXG_SUB_2D, NXG_2D_CLIP_ENABLE);
   w[7]  = NXG_HDR(NXG_INLINE, 0x3f, NXG_SUB_2D, NXG_2D_CORRAL_SIZE);
   w[8]  = NXG_HDR(NXG_INLINE, 0, NXG_SUB_2D, NXG_2D_COMPRESSION);
}

static nxg_t *nxg_init(NWindow *win)
{
   nxg_t *g = (nxg_t*)calloc(1, sizeof(*g));
   unsigned i;
   if (!g)
      return NULL;
   g->sw_last   = -1;
   g->sw_lent   = -1;
   g->rgui_last = -1;
   g->slot      = -1;

   if (!nxg_device_init(g))
      goto error;
   for (i = 0; i < NXG_FRAMES; i++)
   {
      if (!nxg_mem_new(g, &g->frames[i].cmd, NXG_CMD_WORDS * 4, false,
               false))
         goto error;
      if (!nxg_mem_new(g, &g->frames[i].arena, NXG_ARENA_SIZE, false,
               false))
         goto error;
   }
   if (!nxg_mem_new(g, &g->fb, NXG_BUFFERS * NXG_FB_SIZE, true, true))
      goto error;
   if (!nxg_window_init(g, win))
      goto error;

   g->op_mode  = (int)appletGetOperationMode();
   g->out_dims = nxg_mode_dims(g->op_mode);
#ifndef NXG_S2
   nwindowSetCrop(win, 0, 0, VIDEO_SCALE_W(g->out_dims),
         VIDEO_SCALE_H(g->out_dims));
#endif

   /* The 1x1 white a fill samples */
   if (!nxg_mem_new(g, &g->white.mem, 0x1000, false, false))
      goto error;
   memset(g->white.mem.cpu, 0xff, 0x1000);
   armDCacheFlush(g->white.mem.cpu, 0x1000);
   g->white.pixels = g->white.mem.cpu;
   g->white.pitch  = SW_PITCH_ALIGN;
   g->white.dims   = VIDEO_SCALE_PACK(1, 1);

   nxg_frame_recycle(g, 0);
   nxg_bind_engines(g);
#ifdef NXG_3D
   if (nxg_3d_init(g))
      RARCH_LOG("[Switch] Video on the GPU's 2D engine, menus' geometry on its 3D engine.\n");
   else
#endif
   RARCH_LOG("[Switch] Video on the GPU's 2D engine.\n");
   return g;

error:
   RARCH_WARN("[Switch] No GPU channel; video on the CPU.\n");
   nxg_free(g, win);
   return NULL;
}

/* Two words of 32.32 fixed point, fraction first */
static void nxg_fixed(uint32_t *w, double v)
{
   int64_t f = (int64_t)floor(v * 4294967296.0 + 0.5);
   w[0]      = (uint32_t)(uint64_t)f;
   w[1]      = (uint32_t)((uint64_t)f >> 32);
}

/* The 2D engine's source: memory at @iova, pitch-linear */
static bool nxg_source(nxg_t *g, uint64_t iova, uint32_t pitch,
      unsigned dims, uint32_t fmt)
{
   uint32_t *w;
   if (     g->src_valid && g->src_iova == iova && g->src_pitch == pitch
         && g->src_dims == dims && g->src_fmt == fmt)
      return true;
   if (!(w = nxg_cmd(g, 9)))
      return false;
   w[0] = NXG_HDR(NXG_INCR, 2, NXG_SUB_2D, NXG_2D_SRC_FORMAT);
   w[1] = fmt;
   w[2] = 1; /* pitch */
   w[3] = NXG_HDR(NXG_INCR, 5, NXG_SUB_2D, NXG_2D_SRC_PITCH);
   w[4] = pitch;
   w[5] = VIDEO_SCALE_W(dims);
   w[6] = VIDEO_SCALE_H(dims);
   w[7] = (uint32_t)(iova >> 32);
   w[8] = (uint32_t)iova;
   g->src_iova  = iova;
   g->src_pitch = pitch;
   g->src_dims  = dims;
   g->src_fmt   = fmt;
   g->src_valid = true;
   return true;
}

/* One blit from the bound source: destination rectangle in whole
 * pixels, the source's start and step in texels */
static void nxg_blit(nxg_t *g, int op, uint32_t beta4, bool filter,
      int dx, int dy, int dw, int dh, double sx, double sy,
      double du, double dv)
{
   uint32_t *w;
   if (dw <= 0 || dh <= 0)
      return;
#ifdef NXG_3D
   nxg_3d_turn(g, false);
#endif
   if (!(w = nxg_cmd(g, 17)))
      return;
   w[0]  = NXG_HDR(NXG_INLINE, op, NXG_SUB_2D, NXG_2D_OPERATION);
   w[1]  = NXG_HDR(NXG_INCR, 1, NXG_SUB_2D, NXG_2D_BETA4);
   w[2]  = beta4;
   w[3]  = NXG_HDR(NXG_INLINE, NXG_SAMPLE_CORNER
         | (filter ? NXG_SAMPLE_BILINEAR : 0), NXG_SUB_2D,
         NXG_2D_SAMPLE_MODE);
   w[4]  = NXG_HDR(NXG_INCR, 12, NXG_SUB_2D, NXG_2D_PIXELS_DST_X0);
   w[5]  = (uint32_t)dx;
   w[6]  = (uint32_t)dy;
   w[7]  = (uint32_t)dw;
   w[8]  = (uint32_t)dh;
   nxg_fixed(&w[9],  du);
   nxg_fixed(&w[11], dv);
   /* Sampled at the first pixel's centre */
   nxg_fixed(&w[13], sx + du * 0.5);
   nxg_fixed(&w[15], sy + dv * 0.5);
}

/* A window buffer to draw into, and the wait for the display to have
 * let go of it: on the GPU on the Switch, on the CPU (an event, not a
 * sleep) on the Switch 2 */
static bool nxg_frame_begin(nxg_t *g, NWindow *win)
{
   uint64_t iova;
   uint32_t *w;
#ifdef NXG_S2
   (void)win;
   g->slot = (g->slot + 1) % NXG_BUFFERS;
   if (g->slot < 0)
      g->slot = 0;
   nxg_native_wait(g, &g->bufs[g->slot].release);
#else
   NvMultiFence mf;
   s32 slot;
   unsigned i;
   if (R_FAILED(nwindowDequeueBuffer(win, &slot, &mf)))
      return false;
   g->slot = slot;
   for (i = 0; i < mf.num_fences; i++)
   {
      if ((s32)mf.fences[i].id < 0)
         continue;
      if (!(w = nxg_cmd(g, 3)))
         break;
      w[0] = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_HOST_SYNCPT_PAYLOAD);
      w[1] = mf.fences[i].value;
      w[2] = 0u /* wait */ | (1u << 4) | (mf.fences[i].id << 8);
   }
#endif
   iova         = g->fb.iova + (uint64_t)g->slot * NXG_FB_SIZE;
   g->src_valid = false;
   g->recording = true;
#ifdef NXG_3D
   if (g->has3d)
      nxg_3d_frame_begin(g, iova);
#endif
   if (!(w = nxg_cmd(g, 12)))
      return true;
   /* CPU-written sources are read through the texture cache */
   w[0]  = NXG_HDR(NXG_INLINE, 2, NXG_SUB_2D, NXG_2D_TEX_INVALIDATE);
   w[1]  = NXG_HDR(NXG_INCR, 5, NXG_SUB_2D, NXG_2D_DST_FORMAT);
   w[2]  = NXG_FMT_RGBA8;
   w[3]  = 0; /* block-linear */
   w[4]  = NXG_BLOCK_LOG2 << 4;
   w[5]  = 1;
   w[6]  = 0;
   w[7]  = NXG_HDR(NXG_INCR, 4, NXG_SUB_2D, NXG_2D_DST_WIDTH);
   w[8]  = NXG_FB_W;
   w[9]  = NXG_FB_H;
   w[10] = (uint32_t)(iova >> 32);
   w[11] = (uint32_t)iova;
   return true;
}

/* Flush, submit and show: the display waits the frame's fence, not the
 * CPU */
static void nxg_frame_end(nxg_t *g, NWindow *win)
{
   nxg_frame_t *f = &g->frames[g->cur];
   uint32_t *w;
#ifdef NXG_S2
   u32 entry[2];
   struct { u64 reserved; u32 num_entries; u32 flags; u64 fence; } data;
   Vi2PresentOptions o;
   GsvNativeSync producer;
   u64 slots = 0;
   u16 num = 0, den = 0;
   nxg_s2_buf_t *b = &g->bufs[g->slot];
   g->recording = false;
   /* The fence is the kernel's, at the end of the work; the writes
    * reach memory before it */
   if ((w = nxg_cmd(g, 7)))
   {
      w[0] = NXG_HDR(NXG_INCR, 1, NXG_SUB_3D, NXG_HOST_WFI);
      w[1] = 1; /* all */
      w[2] = NXG_HDR(NXG_INCR, 4, NXG_SUB_3D, NXG_HOST_MEMOP_A);
      w[3] = 0;
      w[4] = 0;
      w[5] = 0;
      w[6] = NXG_MEMOP_L2_FLUSH_DIRTY;
   }
   armDCacheFlush(f->cmd.cpu, f->used * 4);
   if (f->arena_off)
      armDCacheFlush(f->arena.cpu, f->arena_off);
   entry[0] = (u32)f->cmd.iova;
   entry[1] = (u32)(f->cmd.iova >> 32) | (1u << 9) /* not main */
            | (f->used << 10);
   memset(&data, 0, sizeof(data));
   data.num_entries = 1;
   data.flags       = NXG_KICKOFF_FLAGS;
   if (R_SUCCEEDED(nvIoctl2(g->ch_fd, NXG_CH_KICKOFF_PB, &data, entry,
               sizeof(entry))))
   {
      f->fence.id    = (u32)data.fence;
      f->fence.value = (u32)(data.fence >> 32);
      f->fence_valid = true;
   }
   else
      f->fence_valid = false;
   f->serial = ++g->serial;

   vi2MakeBasicPresentOptions(&o, NXG_FB_W, NXG_FB_H, armGetSystemTick());
   o.crop_left             = 0;
   o.crop_top              = 0;
   o.crop_right_inclusive  = (u16)(VIDEO_SCALE_W(g->out_dims) - 1);
   o.crop_bottom_inclusive = (u16)(VIDEO_SCALE_H(g->out_dims) - 1);
   if (R_SUCCEEDED(nwindowGetPresentInterval(win, &num, &den)))
   {
      o.interval_numerator   = num;
      o.interval_denominator = num ? den : 1;
   }
   memset(&producer, 0, sizeof(producer));
   if (f->fence_valid)
      nxg_native_from_fence(g, &f->fence, &producer);
   if (R_FAILED(vi2PresentLayer(g->layer, b->layer_slot, b->image_id, &o,
               &producer, &slots, &b->release)))
   {
      /* Shown once done, then */
      if (f->fence_valid)
         nxg_fence_wait(g, &f->fence);
      memset(&producer, 0, sizeof(producer));
      vi2PresentLayer(g->layer, b->layer_slot, b->image_id, &o,
            &producer, &slots, &b->release);
   }
#else
   uint32_t id = nvGpuChannelGetSyncpointId(&g->ch);
   NvMultiFence mf;
   g->recording = false;
   if ((w = nxg_cmd(g, 6)))
   {
      /* Flushed to memory, for the display to read */
      w[0] = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_FLUSH);
      w[1] = NXG_HDR(NXG_INCR, 1, NXG_SUB_3D, NXG_3D_SYNCPT_ACTION);
      w[2] = id | NXG_SYNCPT_INCR | NXG_SYNCPT_FLUSH;
      w[3] = NXG_HDR(NXG_INCR, 1, NXG_SUB_3D, NXG_3D_SYNCPT_ACTION);
      w[4] = id | NXG_SYNCPT_INCR | NXG_SYNCPT_FLUSH;
      w[5] = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_FLUSH);
      nvGpuChannelIncrFence(&g->ch);
      nvGpuChannelIncrFence(&g->ch);
   }
   armDCacheFlush(f->cmd.cpu, f->used * 4);
   if (f->arena_off)
      armDCacheFlush(f->arena.cpu, f->arena_off);
   nvGpuChannelGetFence(&g->ch, &f->fence);
   nvGpuChannelAppendEntry(&g->ch, f->cmd.iova, f->used,
         GPFIFO_ENTRY_NOT_MAIN, 0);
   nvGpuChannelKickoff(&g->ch);
   f->serial      = ++g->serial;
   f->fence_valid = true;
   nvMultiFenceCreate(&mf, &f->fence);
   nwindowQueueBuffer(win, g->slot, &mf);
#endif
   g->cur = (g->cur + 1) % NXG_FRAMES;
   nxg_frame_recycle(g, g->cur);
}

/* @size bytes of the frame's own memory, @align apart, for a draw's
 * small source or its vertices */
static uint8_t *nxg_arena(nxg_t *g, uint32_t size, uint32_t align,
      uint64_t *iova)
{
   nxg_frame_t *f = &g->frames[g->cur];
   uint32_t off   = SW_ALIGN(f->arena_off, align);
   if (off + size > NXG_ARENA_SIZE)
      return NULL;
   f->arena_off = off + size;
   *iova        = f->arena.iova + off;
   return f->arena.cpu + off;
}

/* A ring slot none in flight reads, not @skip0 nor @skip1 */
static int nxg_linear_pick(nxg_t *g, nxg_linear_t *ring, unsigned n,
      int skip0, int skip1)
{
   unsigned i;
   nxg_poll_done(g);
   for (i = 0; i < n; i++)
      if (     (int)i != skip0 && (int)i != skip1
            && ring[i].use_serial <= g->done_serial)
         return (int)i;
   return -1;
}

static bool nxg_linear_fit(nxg_t *g, nxg_linear_t *l, uint32_t size)
{
   nxg_mem_t mem;
   if (l->mem.cpu && l->mem.size >= size)
      return true;
   if (!nxg_mem_new(g, &mem, size, false, false))
      return false;
   if (l->mem.cpu)
      nxg_retire(g, &l->mem);
   l->mem = mem;
   return true;
}

#ifdef NXG_3D
/* ---- the 3D engine ----------------------------------------------------
 *
 * What the 2D engine cannot draw - a rotated quad, a strip, a texture
 * shaded vertex by vertex - goes to the 3D engine of the same channel,
 * methods written here as deko3d writes them, with no macros: the menu
 * shaders deko3d draws with (DKSH, deko3d_shaders/menu_dksh.h), their
 * texture descriptors in a pool of the frame's own, the draw a strip
 * from the frame's arena. The engines take turns, a wait for idle
 * between them. */

/* MAXWELL_B methods, word offsets, named as deko3d's engine_3d.def */
#define NXG_3D_WAIT_FOR_IDLE     0x044
#define NXG_3D_MME_IRAM          0x045 /* position, then words to 0x046 */
#define NXG_3D_MME_START         0x047 /* macro, then its start to 0x048 */
#define NXG_3D_EARLY_FRAGMENT    0x084
#define NXG_3D_SPA_VERSION       0x0C4
#define NXG_3D_FRAGMENT_D8       0x0D8
#define NXG_3D_RASTERIZER        0x0DF
#define NXG_3D_SAMPLE_SHADING    0x1D5
#define NXG_3D_RT0               0x200 /* address, size, format, tiling */
#define NXG_3D_RT_STRIDE         0x10
#define NXG_3D_VIEWPORT_XFORM    0x280 /* scale xyz, translate xyz */
#define NXG_3D_VIEWPORT          0x300 /* x/w, y/h, near, far */
#define NXG_3D_WINDOW_RECTS      0x340
#define NXG_3D_DRAW_FIRST        0x35D
#define NXG_3D_DRAW_COUNT        0x35E
#define NXG_3D_DEPTH_MODE        0x35F
#define NXG_3D_INVALIDATE_SHADER 0x369
#define NXG_3D_POLYGON_FRONT     0x36B
#define NXG_3D_POLYGON_BACK      0x36C
#define NXG_3D_SCISSOR           0x380 /* enable, x min/max, y min/max */
#define NXG_3D_POST_DEPTH        0x3C7
#define NXG_3D_FRAGMENT_TABLE    0x3D0
#define NXG_3D_VERTEX_RUNOUT     0x3E1
#define NXG_3D_SAMPLE_MASK       0x3EF
#define NXG_3D_SCREEN_SCISSOR    0x3FD
#define NXG_3D_ATTRIBS           0x458
#define NXG_3D_GEOMETRY_47C      0x47C
#define NXG_3D_RT_CONTROL        0x487
#define NXG_3D_FRAGMENT_489      0x489
#define NXG_3D_INVALIDATE_TEX    0x4A2
#define NXG_3D_DEPTH_TEST        0x4B3
#define NXG_3D_INDEPENDENT_BLEND 0x4B9
#define NXG_3D_DEPTH_WRITE       0x4BA
#define NXG_3D_ALPHA_TEST        0x4BB
#define NXG_3D_BLEND_ENABLE      0x4D8
#define NXG_3D_STENCIL           0x4E0
#define NXG_3D_WINDOW_ORIGIN     0x4EB
#define NXG_3D_INVALIDATE_TSC    0x509
#define NXG_3D_INVALIDATE_TIC    0x50A
#define NXG_3D_MULTISAMPLE       0x54D
#define NXG_3D_DEPTH_TARGET      0x54E
#define NXG_3D_TSC_POOL          0x557 /* address, highest index */
#define NXG_3D_TIC_POOL          0x55D
#define NXG_3D_ZCULL_REGION      0x564
#define NXG_3D_MULTISAMPLE_MODE  0x574
#define NXG_3D_PROGRAM_REGION    0x582
#define NXG_3D_VERTEX_END        0x585
#define NXG_3D_VERTEX_BEGIN      0x586
#define NXG_3D_PER_INSTANCE      0x620
#define NXG_3D_CULL              0x646
#define NXG_3D_VIEWPORT_ENABLE   0x64B
#define NXG_3D_CLIP_CONTROL      0x64F
#define NXG_3D_FRAGMENT_65B      0x65B
#define NXG_3D_LOGIC_OP          0x671
#define NXG_3D_COLOR_MASK        0x680
#define NXG_3D_Z_COMPRESSION     0x673
#define NXG_3D_COLOR_COMPRESSION 0x678
#define NXG_3D_VERTEX_SIZE       0x180 /* AMPERE_B: a buffer's size */
#define NXG_3D_PIPE_NOP          0x68B
#define NXG_3D_VERTEX_ARRAY      0x700 /* config, address */
#define NXG_3D_BLEND             0x781 /* equation, src, dst; alpha's */
#define NXG_3D_VERTEX_LIMIT      0x7C0
#define NXG_3D_PROGRAM           0x800 /* config, offset, -, registers */
#define NXG_3D_PROGRAM_STRIDE    0x10
#define NXG_3D_CB_SELECT         0x8E0 /* size, address, offset */
#define NXG_3D_CB_DATA           0x8E4
#define NXG_3D_CB_BIND           0x904 /* a stage's, eight apart */
#define NXG_3D_BINDLESS_SLOT     0x982
#define NXG_3D_MACRO0            0xE00 /* register; value, mask to 0xE01 */

#define NXG_3D_PRIM_STRIP        5
#define NXG_3D_FILL              0x1B02
#define NXG_3D_BLEND_ADD         1
#define NXG_3D_STAGE_VERTEX      1 /* program slots: 0 is VertexA */
#define NXG_3D_STAGE_FRAGMENT    5

/* The shaders' resources, where uam's code finds them: textures by
 * handle in the driver's c[0], the fragment stage's from 0x690;
 * uniform block 0 in c[2] */
#define NXG_3D_DRIVER_CB_SIZE    0x1000
#define NXG_3D_FRAGMENT_TEXTURE  0x690
#define NXG_3D_UBO_SIZE          0x400 /* the ribbon's 816 bytes */
#define NXG_3D_UBO_SLOT          2

/* Memory: code and its tail no code may sit in; the sampler and the
 * frames' texture descriptors; constants and the vertex runout */
#define NXG_3D_CODE_ALIGN        0x100
#ifdef NXG_3D_FX
#define NXG_3D_CODE_SIZE         0x10000 /* and the menu effects' */
#else
#define NXG_3D_CODE_SIZE         0x1000
#endif
#define NXG_3D_CODE_TAIL         0x400
#define NXG_3D_TICS              512
#define NXG_3D_TIC_OFF           0x100
#define NXG_3D_POOL_SIZE         (NXG_3D_TIC_OFF + NXG_FRAMES * NXG_3D_TICS * 32)
#define NXG_3D_UBO_OFF           NXG_3D_DRIVER_CB_SIZE
#define NXG_3D_RUNOUT_OFF        (NXG_3D_UBO_OFF + NXG_3D_UBO_SIZE)
#define NXG_3D_CB_MEM            (NXG_3D_RUNOUT_OFF + 0x100)
#define NXG_3D_VTX_FLOATS        8 /* position, texture, colour */

#define NXG_3D_DKSH_MAGIC        0x48534B44u

/* deko3d's WriteHardwareReg (pgraph.mme) for the Fermi MME: wait for
 * idle, the value and mask to the firmware's arguments, the register to
 * FirmwareCall[4], then read the first argument until the firmware
 * sets it to 1 - giving up after 4096 reads, as NVK's does, rather
 * than hold the channel. Assembled by hand: no prebuilt table exists,
 * deko3d builds its own; decoded with mesa's mme_fermi as
 *    0  maddr 0x044            9  send r1
 *    1  send 0                10  r4 = 4096
 *    2  parm r2               11  r1 = state[0xD00]
 *    3  parm r3               12  r4 -= 1
 *    4  maddr 0xD00, +1       13  bz r4, +3 (no delay)
 *    5  send 0                14  r1 -= 1
 *    6  send r2               15  bnz r1, -4 (no delay)
 *    7  send r3               16  maddr 0x040, exit
 *    8  maddr 0x8C4           17  send 0 */
#ifndef NXG_S2
static const uint32_t nxg_3d_mme_hw_reg[18] = {
   0x00110021, 0x00000041, 0x00000201, 0x00000301, 0x07400021, 0x00000041,
   0x00001041, 0x00001841, 0x02310021, 0x00000841, 0x04000411, 0x03400115,
   0xFFFFE411, 0x0000E027, 0xFFFFC911, 0xFFFF0837, 0x001000A1, 0x00000041
};
#endif

#ifndef NXG_S2
static uint32_t nxg_le32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
      | ((uint32_t)p[3] << 24);
}
#endif

static uint32_t nxg_f32(float v)
{
   uint32_t u;
   memcpy(&u, &v, 4);
   return u;
}

/* A program of @type from DKSH @blob, its code copied to @off of the
 * code memory. Only what these shaders need: no compiler constants, no
 * scratch, no VertexA, no fragment table - anything else stays 2D. */
#ifdef NXG_S2
/* A program as NAK made it - its 128-byte header, then its code - copied
 * to @off of the code memory. What its fragment stage runs with: one
 * sample, the menu's single target */
static bool nxg_3d_program_sm86(nxg_t *g, const uint32_t *words,
      size_t size, unsigned gprs, bool early, uint32_t off, nxg_prog_t *p)
{
   if (size <= 128 || off + size > g->code3d.size)
      return false;
   memcpy(g->code3d.cpu + off, words, size);
   p->addr       = g->code3d.iova + off;
   p->prefetch   = (uint32_t)MIN((size + 255) / 256, 127);
   p->offset     = off;
   p->gprs       = gprs;
   p->end        = off + (uint32_t)size;
   p->early      = early;
   p->post_depth = false;
   p->per_sample = false;
   p->param_d8   = 0x087F6080;
   p->param_65b  = 0;
   p->param_489  = 0;
   return true;
}
#else
static bool nxg_3d_program(nxg_t *g, const uint8_t *blob, size_t size,
      uint32_t type, uint32_t off, nxg_prog_t *p)
{
   uint32_t ctrl, code, progs;
   const uint8_t *h;
   if (size < 24 || nxg_le32(blob) != NXG_3D_DKSH_MAGIC)
      return false;
   ctrl  = nxg_le32(blob + 8);
   code  = nxg_le32(blob + 12);
   progs = nxg_le32(blob + 16);
   if (     !nxg_le32(blob + 20) || progs + 64 > ctrl
         || ctrl + code > size || off + code > g->code3d.size)
      return false;
   h = blob + progs;
   if (     nxg_le32(h) != type
         || nxg_le32(h + 16) /* constants */ || nxg_le32(h + 20) /* scratch */
         || (type == 0 && nxg_le32(h + 28)) /* VertexA */
         || (type == 1 && h[24]) /* fragment table */)
      return false;
   memcpy(g->code3d.cpu + off, blob + ctrl, code);
   p->offset    = off + nxg_le32(h + 4);
   p->gprs      = nxg_le32(h + 8);
   p->end       = off + code;
   if (type == 1)
   {
      p->early     = h[25];
      p->post_depth = h[26];
      p->per_sample = h[27];
      p->param_d8  = nxg_le32(h + 44);
      p->param_65b = (uint32_t)h[48] | ((uint32_t)h[49] << 8);
      p->param_489 = (uint32_t)h[50] | ((uint32_t)h[51] << 8);
   }
   return true;
}
#endif

static uint32_t *nxg_3d_m(uint32_t *w, unsigned m, uint32_t v)
{
   if (v < 0x2000)
      *w++ = NXG_HDR(NXG_INLINE, v, NXG_SUB_3D, m);
   else
   {
      *w++ = NXG_HDR(NXG_INCR, 1, NXG_SUB_3D, m);
      *w++ = v;
   }
   return w;
}

static uint32_t *nxg_3d_addr(uint32_t *w, unsigned m, uint64_t iova)
{
   *w++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, m);
   *w++ = (uint32_t)(iova >> 32);
   *w++ = (uint32_t)iova;
   return w;
}

/* @n words in a run from @m, all @v */
static uint32_t *nxg_3d_fill(uint32_t *w, unsigned m, unsigned n, uint32_t v)
{
   unsigned i;
   *w++ = NXG_HDR(NXG_INCR, n, NXG_SUB_3D, m);
   for (i = 0; i < n; i++)
      *w++ = v;
   return w;
}

/* Hands back what a reservation of @n words at @start did not use */
static void nxg_cmd_trim(nxg_t *g, const uint32_t *start, unsigned n,
      const uint32_t *end)
{
   g->frames[g->cur].used -= n - (unsigned)(end - start);
}

static uint32_t *nxg_3d_bind_program(uint32_t *w, unsigned slot,
      const nxg_prog_t *p)
{
#ifdef NXG_S2
   /* AMPERE_B: config, registers, binding, its header's address, how
    * much of it to fetch ahead */
   *w++ = NXG_HDR(NXG_INCR, 1, NXG_SUB_3D,
         NXG_3D_PROGRAM + slot * NXG_3D_PROGRAM_STRIDE);
   *w++ = 1u | (slot << 4);
   *w++ = NXG_HDR(NXG_INCR, 5, NXG_SUB_3D,
         NXG_3D_PROGRAM + slot * NXG_3D_PROGRAM_STRIDE + 3);
   *w++ = p->gprs;
   *w++ = slot - 1;
   *w++ = (uint32_t)(p->addr >> 32);
   *w++ = (uint32_t)p->addr;
   *w++ = p->prefetch;
#else
   *w++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D,
         NXG_3D_PROGRAM + slot * NXG_3D_PROGRAM_STRIDE);
   *w++ = 1u | (slot << 4); /* enabled, its stage */
   *w++ = p->offset;
   w    = nxg_3d_m(w, NXG_3D_PROGRAM + slot * NXG_3D_PROGRAM_STRIDE + 3,
         p->gprs);
#endif
   /* No compiler constants: c[1] unbound */
   return nxg_3d_m(w, NXG_3D_CB_BIND + (slot - 1) * 8, 1u << 4);
}

/* Blend factors: the menu's premultiplied, and the effects' */
enum nxg_blendfn
{
   NXG_BLENDFN_PREMUL = 0,
   NXG_BLENDFN_ADD,
   NXG_BLENDFN_SCREEN,
   NXG_BLENDFN_ALPHA
};

static uint32_t *nxg_3d_blendfn(uint32_t *w, int fn)
{
   /* Equation, source, destination; the alpha's. D3D factors: zero 1,
    * one 2, source alpha 5, one less it 6, destination colour 9 */
   static const uint8_t f[4][4] = {
      { 2, 6, 2, 6 }, /* premultiplied */
      { 2, 2, 2, 2 }, /* adds */
      { 9, 2, 1, 2 }, /* screen: lights what is under it */
      { 5, 6, 2, 6 }  /* by its alpha */
   };
   *w++ = NXG_HDR(NXG_INCR, 6, NXG_SUB_3D, NXG_3D_BLEND);
   *w++ = NXG_3D_BLEND_ADD;
   *w++ = f[fn][0];
   *w++ = f[fn][1];
   *w++ = NXG_3D_BLEND_ADD;
   *w++ = f[fn][2];
   *w++ = f[fn][3];
   return w;
}

/* A vertex and a fragment program, and what the fragment one runs
 * with */
static uint32_t *nxg_3d_bind_pair(uint32_t *w, const nxg_prog_t *vsh,
      const nxg_prog_t *fsh)
{
   w    = nxg_3d_bind_program(w, NXG_3D_STAGE_VERTEX, vsh);
   w    = nxg_3d_bind_program(w, NXG_3D_STAGE_FRAGMENT, fsh);
   w    = nxg_3d_m(w, NXG_3D_FRAGMENT_TABLE, 0);
   w    = nxg_3d_m(w, NXG_3D_EARLY_FRAGMENT, fsh->early);
   w    = nxg_3d_m(w, NXG_3D_POST_DEPTH, fsh->post_depth);
   *w++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_FRAGMENT_D8);
   *w++ = fsh->param_d8;
   *w++ = 0x20;
   w    = nxg_3d_m(w, NXG_3D_FRAGMENT_489, fsh->param_489);
   w    = nxg_3d_m(w, NXG_3D_FRAGMENT_65B, fsh->param_65b);
   return nxg_3d_m(w, NXG_3D_SAMPLE_SHADING, fsh->per_sample ? 0x30 : 0x01);
}

/* The menu's scissor cut to the output, as the scissor register takes
 * it; false when nothing is left */
static bool nxg_3d_scissor(const switch_video_t *sw, uint32_t *sc)
{
   int sx0 = 0, sy0 = 0;
   int sx1 = (int)VIDEO_SCALE_W(sw->out_dims);
   int sy1 = (int)VIDEO_SCALE_H(sw->out_dims);
   if (sw->scissor_on)
   {
      int x0 = VIDEO_POS_X(sw->scissor_pos);
      int y0 = VIDEO_POS_Y(sw->scissor_pos);
      int x1 = x0 + (int)VIDEO_SCALE_W(sw->scissor_dims);
      int y1 = y0 + (int)VIDEO_SCALE_H(sw->scissor_dims);
      if (x0 > sx0) sx0 = x0;
      if (y0 > sy0) sy0 = y0;
      if (x1 < sx1) sx1 = x1;
      if (y1 < sy1) sy1 = y1;
   }
   if (sx1 <= sx0 || sy1 <= sy0)
      return false;
   sc[0] = (uint32_t)sx0 | ((uint32_t)sx1 << 16);
   sc[1] = (uint32_t)sy0 | ((uint32_t)sy1 << 16);
   return true;
}

static uint32_t *nxg_3d_put_scissor(nxg_t *g, uint32_t *c,
      const uint32_t *sc)
{
   if (sc[0] == g->scissor3d[0] && sc[1] == g->scissor3d[1])
      return c;
   *c++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_SCISSOR + 1);
   *c++ = sc[0];
   *c++ = sc[1];
   g->scissor3d[0] = sc[0];
   g->scissor3d[1] = sc[1];
   return c;
}

#ifndef NXG_S2
/* A register written by the firmware, through the macro */
static uint32_t *nxg_3d_hw_reg(uint32_t *w, uint32_t reg, uint32_t v,
      uint32_t mask)
{
   *w++ = NXG_HDR(NXG_ONCE, 3, NXG_SUB_3D, NXG_3D_MACRO0);
   *w++ = reg;
   *w++ = v;
   *w++ = mask;
   return w;
}
#endif

/* The engine's state, once: what deko3d's queue sets up - its register
 * writes through the firmware, less the tile cache's (the tiled cache
 * stays off) and its other macros - and the pipeline the menu draws
 * with */
static void nxg_3d_setup(nxg_t *g)
{
   uint32_t *w0 = nxg_cmd(g, 368), *w = w0;
   uint64_t cb  = g->cb3d.iova;
   unsigned i;
   if (!w)
      return;
#ifdef NXG_S2
   /* AMPERE_B: SPA 8.6; no compression, the buffers have no tags; every
    * vertex buffer empty */
   w = nxg_3d_m(w, NXG_3D_SPA_VERSION, (8u << 8) | 6u);
   w = nxg_3d_m(w, NXG_3D_Z_COMPRESSION, 0);
   w = nxg_3d_fill(w, NXG_3D_COLOR_COMPRESSION, 8, 0);
   w = nxg_3d_fill(w, NXG_3D_VERTEX_SIZE, 32, 0);
#else
   *w++ = NXG_HDR(NXG_ONCE, 19, NXG_SUB_3D, NXG_3D_MME_IRAM);
   *w++ = 0;
   for (i = 0; i < 18; i++)
      *w++ = nxg_3d_mme_hw_reg[i];
   *w++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_MME_START);
   *w++ = 0; /* macro 0 ... */
   *w++ = 0; /* ... from position 0 */
   w = nxg_3d_hw_reg(w, 0x00418800, 0x00000001, 0x00000001);
   w = nxg_3d_hw_reg(w, 0x00419A08, 0x00000000, 0x00000010);
   w = nxg_3d_hw_reg(w, 0x00419F78, 0x00000000, 0x00000008);
   w = nxg_3d_hw_reg(w, 0x00404468, 0x07FFFFFF, 0x3FFFFFFF);
   w = nxg_3d_hw_reg(w, 0x00419A04, 0x00000001, 0x00000001);
   w = nxg_3d_hw_reg(w, 0x00419A04, 0x00000002, 0x00000002);
#endif
   /* No zcull storage is bound to the channel */
   w = nxg_3d_m(w, NXG_3D_ZCULL_REGION, 0x3F);
   w = nxg_3d_m(w, NXG_3D_MULTISAMPLE, 0);
   w = nxg_3d_m(w, NXG_3D_MULTISAMPLE_MODE, 0);
   w = nxg_3d_fill(w, NXG_3D_WINDOW_RECTS, 16, 0);
   w = nxg_3d_fill(w, NXG_3D_PER_INSTANCE, 16, 0);
   w = nxg_3d_fill(w, NXG_3D_SAMPLE_MASK, 4, 0xFFFF);
#ifndef NXG_S2
   w = nxg_3d_m(w, NXG_3D_BINDLESS_SLOT, 0);
#endif
   w = nxg_3d_m(w, NXG_3D_INDEPENDENT_BLEND, 1);
   w = nxg_3d_m(w, NXG_3D_VIEWPORT_ENABLE, 1);
   w = nxg_3d_m(w, NXG_3D_CLIP_CONTROL, 0x181D); /* deko3d's, no clamp */
   w = nxg_3d_m(w, NXG_3D_DEPTH_MODE, 0); /* -1..1 */
   w = nxg_3d_m(w, NXG_3D_WINDOW_ORIGIN, 0); /* top-down, as the buffer */
   w = nxg_3d_m(w, NXG_3D_RASTERIZER, 1);
   w = nxg_3d_m(w, NXG_3D_POLYGON_FRONT, NXG_3D_FILL);
   w = nxg_3d_m(w, NXG_3D_POLYGON_BACK, NXG_3D_FILL);
   w = nxg_3d_m(w, NXG_3D_CULL, 0);
   w = nxg_3d_m(w, NXG_3D_DEPTH_TARGET, 0);
   w = nxg_3d_m(w, NXG_3D_DEPTH_TEST, 0);
   w = nxg_3d_m(w, NXG_3D_DEPTH_WRITE, 0);
   w = nxg_3d_m(w, NXG_3D_STENCIL, 0);
   w = nxg_3d_m(w, NXG_3D_ALPHA_TEST, 0);
   w = nxg_3d_m(w, NXG_3D_LOGIC_OP, 0);
   w = nxg_3d_m(w, NXG_3D_COLOR_MASK, 0x1111);
   /* Premultiplied: one, one less the source's alpha */
   w = nxg_3d_blendfn(w, NXG_BLENDFN_PREMUL);
   w = nxg_3d_m(w, NXG_3D_BLEND_ENABLE, 0);
   /* One target, the window's buffer, sixteen GOBs a block; the rest
    * off */
   w = nxg_3d_m(w, NXG_3D_RT_CONTROL, 1u | (076543210u << 4));
   for (i = 1; i < 8; i++)
      w = nxg_3d_m(w, NXG_3D_RT0 + i * NXG_3D_RT_STRIDE + 4, 0);
   *w++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_SCREEN_SCISSOR);
   *w++ = (uint32_t)NXG_FB_W << 16;
   *w++ = (uint32_t)NXG_FB_H << 16;
   /* The viewport's bounds the whole buffer, depth 0..1 from -1..1; its
    * transform each draw's */
   *w++ = NXG_HDR(NXG_INCR, 4, NXG_SUB_3D, NXG_3D_VIEWPORT);
   *w++ = (uint32_t)NXG_FB_W << 16;
   *w++ = (uint32_t)NXG_FB_H << 16;
   *w++ = nxg_f32(0.0f);
   *w++ = nxg_f32(1.0f);
   w = nxg_3d_m(w, NXG_3D_SCISSOR, 1);
   /* Vertices: buffer 0, 32 bytes apart; the other attributes fixed */
   *w++ = NXG_HDR(NXG_INCR, 32, NXG_SUB_3D, NXG_3D_ATTRIBS);
   *w++ = 0 | (0u << 7) | (0x04u << 21) | (7u << 27);  /* vec2 at 0 */
   *w++ = 0 | (8u << 7) | (0x04u << 21) | (7u << 27);  /* vec2 at 8 */
   *w++ = 0 | (16u << 7) | (0x01u << 21) | (7u << 27); /* vec4 at 16 */
   for (i = 3; i < 32; i++)
      *w++ = (1u << 6) | (0x12u << 21) | (7u << 27);
   w = nxg_3d_m(w, NXG_3D_VERTEX_ARRAY,
         (1u << 12) | (NXG_3D_VTX_FLOATS * 4));
   for (i = 1; i < 16; i++)
      w = nxg_3d_m(w, NXG_3D_VERTEX_ARRAY + i * 4, 0);
   w = nxg_3d_m(w, NXG_3D_DRAW_FIRST, 0);
   w = nxg_3d_addr(w, NXG_3D_VERTEX_RUNOUT, cb + NXG_3D_RUNOUT_OFF);
   /* Descriptors: the sampler, the frames' textures */
   w = nxg_3d_addr(w, NXG_3D_TSC_POOL, g->pool3d.iova);
   w = nxg_3d_m(w, NXG_3D_TSC_POOL + 2, 0);
   w = nxg_3d_addr(w, NXG_3D_TIC_POOL, g->pool3d.iova + NXG_3D_TIC_OFF);
   w = nxg_3d_m(w, NXG_3D_TIC_POOL + 2, NXG_FRAMES * NXG_3D_TICS - 1);
   /* Constants: the driver's c[0] in every stage, the vertex stage's
    * uniform block in c[2] */
   *w++ = NXG_HDR(NXG_INCR, 3, NXG_SUB_3D, NXG_3D_CB_SELECT);
   *w++ = NXG_3D_DRIVER_CB_SIZE;
   *w++ = (uint32_t)(cb >> 32);
   *w++ = (uint32_t)cb;
   for (i = 0; i < 5; i++)
      w = nxg_3d_m(w, NXG_3D_CB_BIND + i * 8, 1u);
   *w++ = NXG_HDR(NXG_INCR, 3, NXG_SUB_3D, NXG_3D_CB_SELECT);
   *w++ = NXG_3D_UBO_SIZE;
   *w++ = (uint32_t)((cb + NXG_3D_UBO_OFF) >> 32);
   *w++ = (uint32_t)(cb + NXG_3D_UBO_OFF);
   w = nxg_3d_m(w, NXG_3D_CB_BIND, 1u | (NXG_3D_UBO_SLOT << 4));
#ifdef NXG_3D_FX
   /* The effects' fragment programs read it too; the menu's never */
   w = nxg_3d_m(w, NXG_3D_CB_BIND + (NXG_3D_STAGE_FRAGMENT - 1) * 8,
         1u | (NXG_3D_UBO_SLOT << 4));
#endif
   /* The programs: vertex and fragment on, VertexA and the stages
    * between them off */
#ifndef NXG_S2
   w = nxg_3d_addr(w, NXG_3D_PROGRAM_REGION, g->code3d.iova);
#endif
   w = nxg_3d_m(w, NXG_3D_PROGRAM, 0);
   for (i = 2; i < NXG_3D_STAGE_FRAGMENT; i++)
      w = nxg_3d_m(w, NXG_3D_PROGRAM + i * NXG_3D_PROGRAM_STRIDE, i << 4);
   w = nxg_3d_m(w, NXG_3D_GEOMETRY_47C, 0);
   w = nxg_3d_bind_pair(w, &g->vsh, &g->fsh);
   w = nxg_3d_m(w, NXG_3D_INVALIDATE_SHADER, 1u | (1u << 4) | (1u << 12));
   nxg_cmd_trim(g, w0, 368, w);
}

/* The memory, the shaders and the sampler; false leaves the menu 2D */
static bool nxg_3d_init(nxg_t *g)
{
   uint32_t *tsc;
   uint32_t fsh_off;
   if (!nxg_mem_new(g, &g->code3d, NXG_3D_CODE_SIZE, false, false))
      return false;
#ifdef NXG_S2
   fsh_off = SW_ALIGN(sizeof(nxg_s2_menu_vsh), NXG_3D_CODE_ALIGN);
   if (     !nxg_3d_program_sm86(g, nxg_s2_menu_vsh, sizeof(nxg_s2_menu_vsh),
               NXG_S2_MENU_VSH_GPRS, false, 0, &g->vsh)
         || !nxg_3d_program_sm86(g, nxg_s2_menu_fsh,
               sizeof(nxg_s2_menu_fsh), NXG_S2_MENU_FSH_GPRS,
               NXG_S2_MENU_FSH_EARLY_FRAGMENT, fsh_off, &g->fsh)
         || g->fsh.end + NXG_3D_CODE_TAIL > g->code3d.size)
      return false;
#else
   fsh_off = SW_ALIGN(nxg_le32(deko3d_menu_vsh_dksh + 12), NXG_3D_CODE_ALIGN);
   if (     !nxg_3d_program(g, deko3d_menu_vsh_dksh,
               sizeof(deko3d_menu_vsh_dksh), 0, 0, &g->vsh)
         || !nxg_3d_program(g, deko3d_menu_fsh_dksh,
               sizeof(deko3d_menu_fsh_dksh), 1, fsh_off, &g->fsh)
         || g->fsh.end + NXG_3D_CODE_TAIL > g->code3d.size)
      return false;
#endif
   armDCacheFlush(g->code3d.cpu, g->code3d.size);
   if (     !nxg_mem_new(g, &g->pool3d, NXG_3D_POOL_SIZE, false, false)
         || !nxg_mem_new(g, &g->cb3d, NXG_3D_CB_MEM, false, false))
      return false;
   /* Linear, clamped to the edge, no mipmaps: deko3d's descriptor for
    * that sampler */
   memset(g->pool3d.cpu, 0, NXG_3D_TIC_OFF);
   tsc    = (uint32_t*)g->pool3d.cpu;
   tsc[0] = 2u | (2u << 3) | (2u << 6) /* clamp to edge */
          | (1u << 10) /* compare op Less, compare off */
          | (1u << 13) /* sRGB conversion */
          | (1u << 14) | (1u << 17); /* font filter 1x1 */
   tsc[1] = 2u /* mag linear */ | (2u << 4) /* min linear */
          | (1u << 6) /* no mip */ | (1u << 8) | (1u << 9);
   tsc[2] = (15u * 256u) << 12; /* lod clamp 0..15 */
   armDCacheFlush(g->pool3d.cpu, NXG_3D_TIC_OFF);
   memset(g->cb3d.cpu, 0, g->cb3d.size);
   armDCacheFlush(g->cb3d.cpu, g->cb3d.size);
   /* The engine's state is the channel's, frame to frame: what it was
    * given is known from here on */
   g->tex_handle   = ~0u;
   g->blend3d      = -1;
   g->scissor3d[0] = g->scissor3d[1] = ~0u;
#ifdef NXG_3D_FX
   g->code3d_used  = g->fsh.end;
   g->prog3d       = -1;
   g->blendfn3d    = NXG_BLENDFN_PREMUL;
#endif
   g->has3d        = true;
   nxg_3d_setup(g);
   return true;
}

/* The frame's buffer as the target, its slice of descriptors and the
 * textures the CPU wrote fresh to the engine */
static void nxg_3d_frame_begin(nxg_t *g, uint64_t fb)
{
   uint32_t *w;
   g->engine3d = false;
   g->tic_used = 0;
   g->tic_surf = NULL;
   if (!(w = nxg_cmd(g, 12)))
      return;
   w[0]  = NXG_HDR(NXG_INCR, 8, NXG_SUB_3D, NXG_3D_RT0);
   w[1]  = (uint32_t)(fb >> 32);
   w[2]  = (uint32_t)fb;
   w[3]  = NXG_FB_W; /* a multiple of the 16 pixels a GOB is wide */
   w[4]  = NXG_FB_H;
   w[5]  = NXG_FMT_RGBA8;
   w[6]  = NXG_BLOCK_LOG2 << 4;
   w[7]  = 1;
   w[8]  = NXG_FB_SIZE >> 2;
   w[9]  = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_INVALIDATE_TIC);
   w[10] = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_INVALIDATE_TSC);
   w[11] = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_INVALIDATE_TEX);
}

/* The engines take turns over the buffer */
static void nxg_3d_turn(nxg_t *g, bool to3d)
{
   uint32_t *w;
   if (g->engine3d == to3d || !(w = nxg_cmd(g, 1)))
      return;
   w[0]        = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_WAIT_FOR_IDLE);
   g->engine3d = to3d;
}

/* @s's descriptor in the frame's pool: pitch-linear premultiplied
 * RGBA8, deko3d's for such an image; ~0 when the frame has no room */
static uint32_t nxg_3d_tic(nxg_t *g, const sw_surf_t *s)
{
   uint32_t idx, *t;
   if (     g->tic_surf == s && g->tic_iova == s->mem.iova
         && g->tic_dims == s->dims)
      return g->tic_last;
   if (g->tic_used >= NXG_3D_TICS)
      return ~0u;
   idx = g->cur * NXG_3D_TICS + g->tic_used++;
   t   = (uint32_t*)(g->pool3d.cpu + NXG_3D_TIC_OFF + idx * 32);
   /* A8R8G8B8, unorm, swizzled R G B A */
   t[0] = 0x08u | (2u << 7) | (2u << 10) | (2u << 13) | (2u << 16)
        | (2u << 19) | (3u << 22) | (4u << 25) | (5u << 28);
   t[1] = (uint32_t)s->mem.iova;
   t[2] = (uint32_t)(s->mem.iova >> 32) | (2u << 21); /* pitch */
   t[3] = s->pitch >> 5;
   t[4] = (VIDEO_SCALE_W(s->dims) - 1) | (7u << 23) /* 2D, no mips */
        | (7u << 29); /* border: the sampler's */
   t[5] = (VIDEO_SCALE_H(s->dims) - 1) | (1u << 31); /* normalized */
   t[6] = (2u << 23) | (1u << 25); /* anisotropy spread: two, one */
   t[7] = 0;
   armDCacheFlush(t, 32);
   g->tic_surf = s;
   g->tic_iova = s->mem.iova;
   g->tic_dims = s->dims;
   g->tic_last = idx;
   return idx;
}

/* A strip of @n vertices in the 0..1 box at @pos/@dims (bottom-up, on
 * an output @out_h high) by @mvp; the colours straight, @tex
 * premultiplied or none. False when the frame has no room for it. */
static bool nxg_3d_draw(switch_video_t *sw, const float *vert,
      const float *tc, const float *col, unsigned n, const float *mvp,
      unsigned pos, unsigned dims, float out_h, const sw_surf_t *tex)
{
   nxg_t *g = (nxg_t*)sw->gpu;
   float x  = (float)VIDEO_POS_X(pos), y = (float)VIDEO_POS_Y(pos);
   float w  = (float)VIDEO_SCALE_W(dims), h = (float)VIDEO_SCALE_H(dims);
   uint32_t handle, sc[2], *c0, *c;
   uint64_t iova;
   float *v;
   unsigned i;

   if (!g->has3d || !g->recording)
      return false;
   if (!nxg_3d_scissor(sw, sc))
      return true;
   if ((handle = nxg_3d_tic(g, tex ? tex : &g->white)) == ~0u)
      return false;
   if (!(v = (float*)nxg_arena(g, n * NXG_3D_VTX_FLOATS * 4, 16, &iova)))
      return false;
   /* Premultiplied, as the surfaces are */
   for (i = 0; i < n; i++, v += NXG_3D_VTX_FLOATS)
   {
      float a = col ? col[4 * i + 3] : 1.0f;
      v[0] = vert[2 * i];
      v[1] = vert[2 * i + 1];
      v[2] = tc[2 * i];
      v[3] = tc[2 * i + 1];
      v[4] = col ? col[4 * i]     * a : 1.0f;
      v[5] = col ? col[4 * i + 1] * a : 1.0f;
      v[6] = col ? col[4 * i + 2] * a : 1.0f;
      v[7] = a;
   }
   nxg_3d_turn(g, true);
   if (!(c0 = nxg_cmd(g, 100)))
      return false;
   c = c0;
#ifdef NXG_3D_FX
   /* The menu's own state again, after an effect */
   if (g->prog3d != -1)
   {
      c         = nxg_3d_bind_pair(c, &g->vsh, &g->fsh);
      g->prog3d = -1;
   }
   if (g->blendfn3d != NXG_BLENDFN_PREMUL)
   {
      c            = nxg_3d_blendfn(c, NXG_BLENDFN_PREMUL);
      g->blendfn3d = NXG_BLENDFN_PREMUL;
   }
   if (g->layout_fx)
   {
      c    = nxg_3d_m(c, NXG_3D_VERTEX_ARRAY,
            (1u << 12) | (NXG_3D_VTX_FLOATS * 4));
      *c++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_ATTRIBS + 1);
      *c++ = 0 | (8u << 7) | (0x04u << 21) | (7u << 27);
      *c++ = 0 | (16u << 7) | (0x01u << 21) | (7u << 27);
      g->layout_fx = false;
   }
#endif
   if (g->blend3d != (int)sw->blend)
   {
      c          = nxg_3d_m(c, NXG_3D_BLEND_ENABLE, sw->blend);
      g->blend3d = sw->blend;
   }
   c = nxg_3d_put_scissor(g, c, sc);
   /* The box's -1..1 onto its pixels, top-down */
   *c++ = NXG_HDR(NXG_INCR, 6, NXG_SUB_3D, NXG_3D_VIEWPORT_XFORM);
   *c++ = nxg_f32(w * 0.5f);
   *c++ = nxg_f32(-h * 0.5f);
   *c++ = nxg_f32(0.5f);
   *c++ = nxg_f32(x + w * 0.5f);
   *c++ = nxg_f32(out_h - y - h * 0.5f);
   *c++ = nxg_f32(0.5f);
   if (!g->mvp3d_valid || memcmp(g->mvp3d, mvp, sizeof(g->mvp3d)))
   {
      uint64_t ubo = g->cb3d.iova + NXG_3D_UBO_OFF;
      *c++ = NXG_HDR(NXG_INCR, 4, NXG_SUB_3D, NXG_3D_CB_SELECT);
      *c++ = NXG_3D_UBO_SIZE;
      *c++ = (uint32_t)(ubo >> 32);
      *c++ = (uint32_t)ubo;
      *c++ = 0;
      *c++ = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_PIPE_NOP);
      *c++ = NXG_HDR(NXG_NONINCR, 16, NXG_SUB_3D, NXG_3D_CB_DATA);
      memcpy(c, mvp, 16 * sizeof(float));
      c += 16;
      memcpy(g->mvp3d, mvp, sizeof(g->mvp3d));
      g->mvp3d_valid = true;
   }
   /* Image and sampler 0 */
   if (handle != g->tex_handle)
   {
      *c++ = NXG_HDR(NXG_INCR, 4, NXG_SUB_3D, NXG_3D_CB_SELECT);
      *c++ = NXG_3D_DRIVER_CB_SIZE;
      *c++ = (uint32_t)(g->cb3d.iova >> 32);
      *c++ = (uint32_t)g->cb3d.iova;
      *c++ = NXG_3D_FRAGMENT_TEXTURE;
      *c++ = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_PIPE_NOP);
      *c++ = NXG_HDR(NXG_NONINCR, 1, NXG_SUB_3D, NXG_3D_CB_DATA);
      *c++ = handle;
      g->tex_handle = handle;
   }
   c = nxg_3d_addr(c, NXG_3D_VERTEX_ARRAY + 1, iova);
#ifdef NXG_S2
   c = nxg_3d_addr(c, NXG_3D_VERTEX_SIZE, n * NXG_3D_VTX_FLOATS * 4);
#else
   c = nxg_3d_addr(c, NXG_3D_VERTEX_LIMIT,
         iova + n * NXG_3D_VTX_FLOATS * 4 - 1);
#endif
   c = nxg_3d_m(c, NXG_3D_VERTEX_BEGIN, NXG_3D_PRIM_STRIP);
   c = nxg_3d_m(c, NXG_3D_DRAW_COUNT, n);
   c = nxg_3d_m(c, NXG_3D_VERTEX_END, 0);
   nxg_cmd_trim(g, c0, 100, c);
   return true;
}
#endif /* NXG_3D */

#endif /* SWITCH_NX_GPU */

/* ---- surfaces --------------------------------------------------------- */

static sw_surf_t *sw_surf_new(switch_video_t *sw, unsigned dims)
{
   uint32_t pitch = SW_ALIGN(VIDEO_SCALE_W(dims) * 4, SW_PITCH_ALIGN);
   uint32_t size  = pitch * VIDEO_SCALE_H(dims);
   sw_surf_t *s   = (sw_surf_t*)calloc(1, sizeof(*s));
   if (!s || !size)
   {
      free(s);
      return NULL;
   }
   s->pitch = pitch;
   s->dims  = dims;
#ifdef SWITCH_NX_GPU
   if (sw->gpu)
   {
      nxg_t *g = (nxg_t*)sw->gpu;
      uint32_t off;
      if (size <= NXG_HEAP_BLOCK / 2 && nxg_heap_alloc(g, size, &s->block, &off))
      {
         s->heap     = true;
         s->mem.cpu  = g->heap[s->block].mem.cpu + off;
         s->mem.iova = g->heap[s->block].mem.iova + off;
         s->mem.size = size;
      }
      else if (!nxg_mem_new(g, &s->mem, size, false, false))
      {
         free(s);
         return NULL;
      }
      s->pixels = s->mem.cpu;
      return s;
   }
#endif
   if (!(s->pixels = (uint8_t*)malloc(size)))
   {
      free(s);
      return NULL;
   }
   return s;
}

/* Gone once no frame in flight reads it */
static void sw_surf_free(switch_video_t *sw, sw_surf_t *s)
{
   if (!s)
      return;
#ifdef SWITCH_NX_GPU
   if (sw->gpu)
   {
      nxg_t *g = (nxg_t*)sw->gpu;
      if (s->heap)
         nxg_heap_retire(g, s->block,
               (uint32_t)(s->mem.cpu - g->heap[s->block].mem.cpu),
               s->mem.size);
      else
         nxg_retire(g, &s->mem);
      free(s);
      return;
   }
#endif
   free(s->pixels);
   free(s);
}

/* Rows @y0.. @rows written by the CPU, made visible to the GPU */
static void sw_surf_flush(switch_video_t *sw, sw_surf_t *s,
      unsigned y0, unsigned rows)
{
#ifdef SWITCH_NX_GPU
   if (sw->gpu && rows)
      armDCacheFlush(s->pixels + (size_t)y0 * s->pitch,
            (size_t)rows * s->pitch);
#else
   (void)sw; (void)s; (void)y0; (void)rows;
#endif
}

/* A texture loader's ARGB words, premultiplied into @s */
static void sw_surf_fill_argb(sw_surf_t *s, const uint32_t *src,
      unsigned w, unsigned h, bool rgba)
{
   unsigned x, y;
   for (y = 0; y < h; y++)
   {
      uint32_t *d = (uint32_t*)(s->pixels + (size_t)y * s->pitch);
      const uint32_t *in = src + (size_t)y * w;
      for (x = 0; x < w; x++)
      {
         uint32_t p = in[x], t, r, g, b, a = p >> 24;
         if (rgba)
         {
            r = p & 0xffu;
            b = (p >> 16) & 0xffu;
         }
         else
         {
            r = (p >> 16) & 0xffu;
            b = p & 0xffu;
         }
         g = (p >> 8) & 0xffu;
         if (a != 255u)
         {
            r = SW_MUL255(r, a, t);
            g = SW_MUL255(g, a, t);
            b = SW_MUL255(b, a, t);
         }
         d[x] = r | (g << 8) | (b << 16) | (a << 24);
      }
   }
}

/* ---- drawing ---------------------------------------------------------- */

/* @op cut to the scissor and the screen; false when nothing is left.
 * The source rectangle is cut in proportion. */
static bool sw_op_clip(const switch_video_t *sw, sw_op_t *op)
{
   float x0 = 0.0f, y0 = 0.0f;
   float x1 = (float)VIDEO_SCALE_W(sw->out_dims);
   float y1 = (float)VIDEO_SCALE_H(sw->out_dims);
   float nx0, ny0, nx1, ny1;
   if (op->dw <= 0.0f || op->dh <= 0.0f)
      return false;
   if (sw->scissor_on)
   {
      float sx0 = (float)VIDEO_POS_X(sw->scissor_pos);
      float sy0 = (float)VIDEO_POS_Y(sw->scissor_pos);
      float sx1 = sx0 + (float)VIDEO_SCALE_W(sw->scissor_dims);
      float sy1 = sy0 + (float)VIDEO_SCALE_H(sw->scissor_dims);
      if (sx0 > x0) x0 = sx0;
      if (sy0 > y0) y0 = sy0;
      if (sx1 < x1) x1 = sx1;
      if (sy1 < y1) y1 = sy1;
   }
   /* Whole pixels: a draw covers the pixels whose centres it holds */
   nx0 = (float)floor(op->dx + 0.5f);
   ny0 = (float)floor(op->dy + 0.5f);
   nx1 = (float)floor(op->dx + op->dw + 0.5f);
   ny1 = (float)floor(op->dy + op->dh + 0.5f);
   if (nx0 < x0) nx0 = x0;
   if (ny0 < y0) ny0 = y0;
   if (nx1 > x1) nx1 = x1;
   if (ny1 > y1) ny1 = y1;
   if (nx1 <= nx0 || ny1 <= ny0)
      return false;
   {
      float ku = op->sw / op->dw;
      float kv = op->sh / op->dh;
      float nsx = op->sx + (nx0 - op->dx) * ku;
      float nsy = op->sy + (ny0 - op->dy) * kv;
      if (op->gradient)
      {
         /* The corners re-weighted to the cut rectangle */
         float fx0 = (nx0 - op->dx) / op->dw, fx1 = (nx1 - op->dx) / op->dw;
         float fy0 = (ny0 - op->dy) / op->dh, fy1 = (ny1 - op->dy) / op->dh;
         if (fx0 > 0.0f || fx1 < 1.0f || fy0 > 0.0f || fy1 < 1.0f)
         {
            uint32_t c[4];
            unsigned k, ch;
            for (k = 0; k < 4; k++)
            {
               float fx = (k & 1) ? fx1 : fx0;
               float fy = (k & 2) ? fy1 : fy0;
               uint32_t v = 0;
               for (ch = 0; ch < 32; ch += 8)
               {
                  float tl = (float)((op->color[0] >> ch) & 0xffu);
                  float tr = (float)((op->color[1] >> ch) & 0xffu);
                  float bl = (float)((op->color[2] >> ch) & 0xffu);
                  float br = (float)((op->color[3] >> ch) & 0xffu);
                  float t  = tl + (tr - tl) * fx;
                  float b  = bl + (br - bl) * fx;
                  v       |= ((uint32_t)(t + (b - t) * fy + 0.5f) & 0xffu) << ch;
               }
               c[k] = v;
            }
            memcpy(op->color, c, sizeof(c));
         }
      }
      op->sw = (nx1 - nx0) * ku;
      op->sh = (ny1 - ny0) * kv;
      op->sx = nsx;
      op->sy = nsy;
   }
   op->dx = nx0;
   op->dy = ny0;
   op->dw = nx1 - nx0;
   op->dh = ny1 - ny0;
   return true;
}

/* The CPU's execution of a clipped @op into the framebuffer */
static void sw_op_cpu(switch_video_t *sw, const sw_op_t *op)
{
   int x, y;
   int dx       = (int)op->dx;
   int dy       = (int)op->dy;
   int dw       = (int)op->dw;
   int dh       = (int)op->dh;
   unsigned pitch = sw->stride / sizeof(uint32_t);
   uint32_t *out  = sw->out_buffer + (size_t)dy * pitch + dx;

   if (!op->src)
   {
      for (y = 0; y < dh; y++, out += pitch)
      {
         uint32_t cl, cr;
         if (op->gradient)
         {
            /* Each row's ends, then across */
            float fy = dh > 1 ? ((float)y + 0.5f) / (float)dh : 0.5f;
            unsigned ch;
            cl = cr = 0;
            for (ch = 0; ch < 32; ch += 8)
            {
               float tl = (float)((op->color[0] >> ch) & 0xffu);
               float tr = (float)((op->color[1] >> ch) & 0xffu);
               float bl = (float)((op->color[2] >> ch) & 0xffu);
               float br = (float)((op->color[3] >> ch) & 0xffu);
               cl |= ((uint32_t)(tl + (bl - tl) * fy + 0.5f) & 0xffu) << ch;
               cr |= ((uint32_t)(tr + (br - tr) * fy + 0.5f) & 0xffu) << ch;
            }
         }
         else
            cl = cr = op->color[0];
         if (cl == cr)
         {
            if (!op->blend || (cl >> 24) == 255u)
               for (x = 0; x < dw; x++)
                  out[x] = (cl & 0x00ffffffu) | (out[x] & 0xff000000u);
            else if (cl >> 24)
               for (x = 0; x < dw; x++)
                  out[x] = sw_over(cl, out[x]);
         }
         else
            for (x = 0; x < dw; x++)
            {
               float fx = ((float)x + 0.5f) / (float)dw;
               uint32_t c = 0;
               unsigned ch;
               for (ch = 0; ch < 32; ch += 8)
               {
                  float l = (float)((cl >> ch) & 0xffu);
                  float r = (float)((cr >> ch) & 0xffu);
                  c |= ((uint32_t)(l + (r - l) * fx + 0.5f) & 0xffu) << ch;
               }
               out[x] = op->blend ? sw_over(c, out[x])
                  : (c & 0x00ffffffu) | (out[x] & 0xff000000u);
            }
      }
      return;
   }

   {
      /* Nearest, at pixel centres, in 16.16 */
      const sw_surf_t *s = op->src;
      int sw_px          = (int)VIDEO_SCALE_W(s->dims);
      int sh_px          = (int)VIDEO_SCALE_H(s->dims);
      int64_t su         = (int64_t)((double)op->sw / op->dw * 65536.0);
      int64_t sv         = (int64_t)((double)op->sh / op->dh * 65536.0);
      int64_t u0         = (int64_t)((double)op->sx * 65536.0) + su / 2;
      int64_t v          = (int64_t)((double)op->sy * 65536.0) + sv / 2;
      uint32_t c         = op->color[0];
      uint16_t *cols     = sw->op_cols;
      if (dw > (int)ARRAY_SIZE(sw->op_cols))
         dw = (int)ARRAY_SIZE(sw->op_cols);
      for (x = 0; x < dw; x++)
      {
         int col = (int)((u0 + su * x) >> 16);
         cols[x] = (uint16_t)(col < 0 ? 0 : col >= sw_px ? sw_px - 1 : col);
      }
      for (y = 0; y < dh; y++, out += pitch, v += sv)
      {
         int row = (int)(v >> 16);
         const uint32_t *in;
         if (row < 0)
            row = 0;
         else if (row >= sh_px)
            row = sh_px - 1;
         in = (const uint32_t*)(s->pixels + (size_t)row * s->pitch);
         if (!op->blend)
            for (x = 0; x < dw; x++)
               out[x] = (sw_modulate(in[cols[x]], c) & 0x00ffffffu)
                  | (out[x] & 0xff000000u);
         else if (c == 0xffffffffu)
            for (x = 0; x < dw; x++)
            {
               uint32_t p = in[cols[x]];
               if (p >> 24)
                  out[x] = sw_over(p, out[x]);
            }
         else
            for (x = 0; x < dw; x++)
            {
               uint32_t p = in[cols[x]];
               if (p >> 24)
                  out[x] = sw_over(sw_modulate(p, c), out[x]);
            }
      }
   }
}

#ifdef SWITCH_NX_GPU
/* The 2D engine's execution of a clipped @op */
static void sw_op_gpu(switch_video_t *sw, const sw_op_t *op)
{
   nxg_t *g          = (nxg_t*)sw->gpu;
   const sw_surf_t *s = op->src;
   uint32_t beta     = op->color[0];
   double sx = op->sx, sy = op->sy, ssw = op->sw, ssh = op->sh;
   bool filter       = op->filter;
   int mode          = op->blend ? NXG_OP_BLEND_PREMULT
                                 : NXG_OP_SRCCOPY_PREMULT;

   if (!s)
   {
      if (     op->gradient
            && (   op->color[0] != op->color[1]
                || op->color[0] != op->color[2]
                || op->color[0] != op->color[3]))
      {
         /* The corners as a 2x2 image stretched centre to centre:
          * bilinear is the gradient */
         uint64_t iova;
         uint32_t *p = (uint32_t*)nxg_arena(g, 2 * SW_PITCH_ALIGN, SW_PITCH_ALIGN, &iova);
         if (!p)
            return;
         p[0]                      = op->color[0];
         p[1]                      = op->color[1];
         p[SW_PITCH_ALIGN / 4]     = op->color[2];
         p[SW_PITCH_ALIGN / 4 + 1] = op->color[3];
         if (!nxg_source(g, iova, SW_PITCH_ALIGN, VIDEO_SCALE_PACK(2, 2),
                  NXG_FMT_RGBA8))
            return;
         nxg_blit(g, mode, 0xffffffffu, true,
               (int)op->dx, (int)op->dy, (int)op->dw, (int)op->dh,
               0.5, 0.5, 1.0 / op->dw, 1.0 / op->dh);
         return;
      }
      s      = &g->white;
      sx     = 0.0;
      sy     = 0.0;
      ssw    = 1.0;
      ssh    = 1.0;
      filter = false;
   }
   if (!nxg_source(g, s->mem.iova, s->pitch, s->dims, NXG_FMT_RGBA8))
      return;
   nxg_blit(g, mode, beta, filter,
         (int)op->dx, (int)op->dy, (int)op->dw, (int)op->dh,
         sx, sy, ssw / op->dw, ssh / op->dh);
}
#endif

static void sw_op_draw(switch_video_t *sw, sw_op_t *op)
{
   if (!sw_op_clip(sw, op))
      return;
#ifdef SWITCH_NX_GPU
   if (sw->gpu)
   {
      if (((nxg_t*)sw->gpu)->recording)
         sw_op_gpu(sw, op);
      return;
   }
#endif
   if (sw->out_buffer)
      sw_op_cpu(sw, op);
}

/* ---- menu (gfx_display) ----------------------------------------------- */

static const float sw_default_vertices[8] = {
   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
};
static const float sw_default_tex_coords[8] = {
   0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f
};

static const float *gfx_display_switch_get_default_vertices(void)
{
   return sw_default_vertices;
}

static const float *gfx_display_switch_get_default_tex_coords(void)
{
   return sw_default_tex_coords;
}

static void *gfx_display_switch_get_default_mvp(void *data)
{
   switch_video_t *sw = (switch_video_t*)data;
   return sw ? &sw->mvp : NULL;
}

static void gfx_display_switch_blend_begin(void *data)
{
   switch_video_t *sw = (switch_video_t*)data;
   if (sw)
      sw->blend = true;
}

static void gfx_display_switch_blend_end(void *data)
{
   switch_video_t *sw = (switch_video_t*)data;
   if (sw)
      sw->blend = false;
}

static void gfx_display_switch_scissor_begin(void *data,
      unsigned video_dims, int x, int y, unsigned dims)
{
   switch_video_t *sw = (switch_video_t*)data;
   (void)video_dims;
   if (!sw)
      return;
   sw->scissor_pos  = VIDEO_POS_PACK(x, y);
   sw->scissor_dims = dims;
   sw->scissor_on   = true;
}

static void gfx_display_switch_scissor_end(void *data, unsigned video_dims)
{
   switch_video_t *sw = (switch_video_t*)data;
   (void)video_dims;
   if (sw)
      sw->scissor_on = false;
}

#ifdef NXG_3D
/* What the 2D engine cannot draw, drawn by the 3D engine: a strip, a
 * matrix that is not the default (rotated, scaled), a texture shaded
 * by more than one colour. True when it is done with (drawn, or a strip
 * there was no room for). */
static bool gfx_display_switch_draw_3d(gfx_display_ctx_draw_t *draw,
      switch_video_t *sw, unsigned video_dims)
{
   const struct video_coords *c = draw->coords;
   const float *mvp = draw->matrix_data
      ? ((const math_matrix_4x4*)draw->matrix_data)->data : sw->mvp.data;
   const float *vert = c->vertex    ? c->vertex    : sw_default_vertices;
   const float *tc   = c->tex_coord ? c->tex_coord : sw_default_tex_coords;
   unsigned n        = c->vertices, i;
   bool need;
   if (!sw->gpu || !((nxg_t*)sw->gpu)->has3d || !n)
      return false;
   need = n != 4 || memcmp(mvp, sw->mvp.data, sizeof(sw->mvp.data));
   if (!need && draw->texture && c->color)
      for (i = 4; i < 16 && !need; i++)
         need = c->color[i] != c->color[i & 3];
   if (!need)
      return false;
   /* The defaults are one quad's */
   if (n > 4 && (vert == sw_default_vertices || tc == sw_default_tex_coords))
      return true;
   return nxg_3d_draw(sw, vert, tc, c->color, n, mvp, draw->pos,
         draw->dims, (float)VIDEO_SCALE_H(video_dims ? video_dims
            : sw->out_dims), (const sw_surf_t*)draw->texture) || n != 4;
}
#endif

/* One quad, as an operation. The quad is placed by its vertices in the
 * draw's rectangle, bottom-up; its texels by the texture coordinates,
 * top-down. A rotation the 2D engine cannot draw keeps its bounds;
 * the scale grows it about its centre. */
static void gfx_display_switch_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_dims)
{
   switch_video_t *sw = (switch_video_t*)data;
   const float *vert, *tc, *col;
   const sw_surf_t *tex;
   float px[4], py[4];
   float x0, x1, y0, y1, cx, cy, k;
   unsigned i, t, b, l, r;
   float out_h;
   sw_op_t op;

   /* An effect: drawn by draw_pipeline */
   if (!sw || !draw || !draw->coords || draw->effect)
      return;
#ifdef NXG_3D
   if (gfx_display_switch_draw_3d(draw, sw, video_dims))
      return;
#endif
   if (draw->coords->vertices != 4)
      return;
   vert  = draw->coords->vertex    ? draw->coords->vertex    : sw_default_vertices;
   tc    = draw->coords->tex_coord ? draw->coords->tex_coord : sw_default_tex_coords;
   col   = draw->coords->color;
   tex   = (const sw_surf_t*)draw->texture;
   out_h = (float)VIDEO_SCALE_H(video_dims ? video_dims : sw->out_dims);

   for (i = 0; i < 4; i++)
   {
      px[i] = (float)VIDEO_POS_X(draw->pos)
         + vert[2 * i]     * (float)VIDEO_SCALE_W(draw->dims);
      py[i] = out_h - ((float)VIDEO_POS_Y(draw->pos)
         + vert[2 * i + 1] * (float)VIDEO_SCALE_H(draw->dims));
   }
   x0 = x1 = px[0];
   y0 = y1 = py[0];
   for (i = 1; i < 4; i++)
   {
      if (px[i] < x0) x0 = px[i];
      if (px[i] > x1) x1 = px[i];
      if (py[i] < y0) y0 = py[i];
      if (py[i] > y1) y1 = py[i];
   }
   if (draw->scale_factor > 0.0f && draw->scale_factor != 1.0f)
   {
      k  = draw->scale_factor;
      cx = (x0 + x1) * 0.5f;
      cy = (y0 + y1) * 0.5f;
      x0 = cx + (x0 - cx) * k;
      x1 = cx + (x1 - cx) * k;
      y0 = cy + (y0 - cy) * k;
      y1 = cy + (y1 - cy) * k;
   }

   /* The vertices at the corners: top/bottom of the left edge, and the
    * right edge's */
   t = (py[2] < py[0]) ? 2 : 0; /* the top of the first column */
   b = t ^ 2;
   l = (px[0] <= px[1]) ? 0 : 1;
   r = l ^ 1;

   memset(&op, 0, sizeof(op));
   op.dx    = x0;
   op.dy    = y0;
   op.dw    = x1 - x0;
   op.dh    = y1 - y0;
   op.blend = sw->blend;
   op.filter = true;

   if (tex)
   {
      float tw = (float)VIDEO_SCALE_W(tex->dims);
      float th = (float)VIDEO_SCALE_H(tex->dims);
      float u0 = tc[2 * (t | l)],   u1 = tc[2 * (t | r)];
      float v0 = tc[2 * (t | l) + 1], v1 = tc[2 * (b | l) + 1];
      op.src = tex;
      op.sx  = u0 * tw;
      op.sw  = (u1 - u0) * tw;
      op.sy  = v0 * th;
      op.sh  = (v1 - v0) * th;
      /* One colour for a texture: what the corners average */
      if (col)
      {
         float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
         for (i = 0; i < 4; i++)
         {
            c[0] += col[4 * i];
            c[1] += col[4 * i + 1];
            c[2] += col[4 * i + 2];
            c[3] += col[4 * i + 3];
         }
         op.color[0] = sw_pack_premul(c[0] * 0.25f, c[1] * 0.25f,
               c[2] * 0.25f, c[3] * 0.25f);
      }
      else
         op.color[0] = 0xffffffffu;
      if (!(op.color[0] >> 24))
         return;
   }
   else
   {
      static const unsigned corner[4] = { 2, 3, 0, 1 }; /* TL TR BL BR */
      if (col)
      {
         for (i = 0; i < 4; i++)
         {
            unsigned v = corner[i];
            /* the vertex at that corner, flipped with the quad */
            if (t == 0)
               v ^= 2;
            if (l == 1)
               v ^= 1;
            op.color[i] = sw_pack_premul(col[4 * v], col[4 * v + 1],
                  col[4 * v + 2], col[4 * v + 3]);
         }
      }
      else
         op.color[0] = op.color[1] = op.color[2] = op.color[3] = 0xffffffffu;
      op.gradient = (op.color[0] != op.color[1]
                  || op.color[0] != op.color[2]
                  || op.color[0] != op.color[3]);
      if (!op.gradient && !(op.color[0] >> 24) && op.blend)
         return;
   }
   sw_op_draw(sw, &op);
}

#ifdef NXG_3D_FX
/* ---- menu effects ------------------------------------------------------
 *
 * The menu's effects on the 3D engine: their programs from the DKSH
 * the description carries - NAK's SM 8.6 on the Switch 2 - appended to
 * the code memory; drawn over the
 * effect mesh's positions, which the GPU keeps a copy of per mesh, with
 * the uniforms the Vulkan driver gives them. Clip y points up here and
 * gl_FragCoord runs down: the ribbons take GL's y flip, the effects
 * over the screen's quad Vulkan's. */

static void *gfx_display_switch_effect_install(void *data,
      const gfx_display_effect_desc_t *desc)
{
   switch_video_t *sw = (switch_video_t*)data;
   nxg_t *g;
   nxg_fx_t *fx;
   uint32_t start;
#ifdef NXG_S2
   if (     !sw || !sw->gpu || !desc
         || !desc->sm86_vertex || !desc->sm86_fragment)
      return NULL;
#else
   if (     !sw || !sw->gpu || !desc
         || !desc->dksh_vertex || !desc->dksh_fragment)
      return NULL;
#endif
   g = (nxg_t*)sw->gpu;
   if (!g->has3d || g->fx_count >= NXG_3D_EFFECTS)
      return NULL;
   fx    = &g->fx[g->fx_count];
   start = SW_ALIGN(g->code3d_used, NXG_3D_CODE_ALIGN);
   /* Past the code frames in flight run: written while they run */
#ifdef NXG_S2
   if (     !nxg_3d_program_sm86(g, desc->sm86_vertex,
               desc->sm86_vertex_size, desc->sm86_vertex_gprs, false,
               start, &fx->vsh)
         || !nxg_3d_program_sm86(g, desc->sm86_fragment,
               desc->sm86_fragment_size, desc->sm86_fragment_gprs, false,
               SW_ALIGN(fx->vsh.end, NXG_3D_CODE_ALIGN), &fx->fsh)
         || fx->fsh.end + NXG_3D_CODE_TAIL > g->code3d.size)
      return NULL;
#else
   if (     !nxg_3d_program(g, desc->dksh_vertex, desc->dksh_vertex_size,
               0, start, &fx->vsh)
         || !nxg_3d_program(g, desc->dksh_fragment,
               desc->dksh_fragment_size, 1,
               SW_ALIGN(fx->vsh.end, NXG_3D_CODE_ALIGN), &fx->fsh)
         || fx->fsh.end + NXG_3D_CODE_TAIL > g->code3d.size)
      return NULL;
#endif
   armDCacheFlush(g->code3d.cpu + start, fx->fsh.end - start);
   fx->flags       = desc->flags;
   g->code3d_used  = fx->fsh.end;
   g->code3d_dirty = true;
   g->fx_count++;
   return fx;
}

/* @mesh's positions where the GPU reads them: made once per mesh, the
 * oldest let go of with the frame */
static const nxg_mesh_t *nxg_3d_mesh(nxg_t *g, const gfx_display_mesh_t *mesh)
{
   nxg_mesh_t *m;
   unsigned i;
   for (i = 0; i < NXG_3D_MESHES; i++)
      if (     g->meshes[i].mem.cpu && g->meshes[i].id == mesh->id
            && g->meshes[i].count == mesh->vertex_count)
         return &g->meshes[i];
   m            = &g->meshes[g->mesh_next];
   g->mesh_next = (g->mesh_next + 1) % NXG_3D_MESHES;
   nxg_retire(g, &m->mem);
   if (!nxg_mem_new(g, &m->mem, mesh->vertex_count * 8, false, false))
      return NULL;
   memcpy(m->mem.cpu, mesh->positions, mesh->vertex_count * 8);
   armDCacheFlush(m->mem.cpu, mesh->vertex_count * 8);
   m->id    = mesh->id;
   m->count = mesh->vertex_count;
   return m;
}

static void gfx_display_switch_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp, void *data, unsigned video_dims)
{
   switch_video_t *sw = (switch_video_t*)data;
   const nxg_fx_t *fx = draw ? (const nxg_fx_t*)draw->effect : NULL;
   const gfx_display_mesh_t *mesh;
   const nxg_mesh_t *m;
   const float *ubo;
   float W, H, u[4];
   unsigned n, nubo, off;
   uint32_t sc[2], *c0, *c;
   nxg_t *g;
   int fn;

   if (!sw || !fx || !p_disp || !sw->gpu)
      return;
   g    = (nxg_t*)sw->gpu;
   mesh = p_disp->effect_mesh;
   n    = p_disp->effect_vertices;
   if (     !g->has3d || !g->recording || !mesh || !mesh->positions
         || n < 3 || n > mesh->vertex_count || !nxg_3d_scissor(sw, sc))
      return;
   W = (float)VIDEO_SCALE_W(video_dims);
   H = (float)VIDEO_SCALE_H(video_dims);
   if (fx->flags & GFX_EFFECT_RIBBON)
   {
      /* time, y flip, alpha, then the ribbon's 50 vec4s */
      if (!(ubo = gfx_display_ribbon_ubo(p_disp, 1.0f)))
         return;
      nubo = 204;
      off  = 0;
   }
   else if (fx->flags & GFX_EFFECT_MESH)
   {
      u[0] = p_disp->effect_time;
      u[1] = 1.0f;
      u[2] = draw->color ? draw->color[3] : 1.0f;
      ubo  = u;
      nubo = 3;
      off  = 0;
   }
   else
   {
      /* After the MVP these do not read: OutputSize, time, y flip */
      u[0] = W;
      u[1] = H;
      u[2] = p_disp->effect_time;
      u[3] = (fx->flags & GFX_EFFECT_YFLIP) ? -1.0f : 1.0f;
      ubo  = u;
      nubo = 4;
      off  = 64;
   }
   fn = (fx->flags & GFX_EFFECT_BLEND_ADD)    ? NXG_BLENDFN_ADD
      : (fx->flags & GFX_EFFECT_BLEND_SCREEN) ? NXG_BLENDFN_SCREEN
      :                                         NXG_BLENDFN_ALPHA;
   if (!(m = nxg_3d_mesh(g, mesh)))
      return;
   nxg_3d_turn(g, true);
   if (!(c0 = nxg_cmd(g, 300)))
      return;
   c = c0;
   if (g->code3d_dirty)
   {
      c = nxg_3d_m(c, NXG_3D_INVALIDATE_SHADER, 1u | (1u << 4) | (1u << 12));
      g->code3d_dirty = false;
   }
   if (g->prog3d != (int)(fx - g->fx))
   {
      c         = nxg_3d_bind_pair(c, &fx->vsh, &fx->fsh);
      g->prog3d = (int)(fx - g->fx);
   }
   if (g->blend3d != 1)
   {
      c          = nxg_3d_m(c, NXG_3D_BLEND_ENABLE, 1);
      g->blend3d = 1;
   }
   if (g->blendfn3d != fn)
   {
      c            = nxg_3d_blendfn(c, fn);
      g->blendfn3d = fn;
   }
   c    = nxg_3d_put_scissor(g, c, sc);
   *c++ = NXG_HDR(NXG_INCR, 6, NXG_SUB_3D, NXG_3D_VIEWPORT_XFORM);
   *c++ = nxg_f32(W * 0.5f);
   *c++ = nxg_f32(-H * 0.5f);
   *c++ = nxg_f32(0.5f);
   *c++ = nxg_f32(W * 0.5f);
   *c++ = nxg_f32(H * 0.5f);
   *c++ = nxg_f32(0.5f);
   *c++ = NXG_HDR(NXG_INCR, 4, NXG_SUB_3D, NXG_3D_CB_SELECT);
   *c++ = NXG_3D_UBO_SIZE;
   *c++ = (uint32_t)((g->cb3d.iova + NXG_3D_UBO_OFF) >> 32);
   *c++ = (uint32_t)(g->cb3d.iova + NXG_3D_UBO_OFF);
   *c++ = off;
   *c++ = NXG_HDR(NXG_INLINE, 0, NXG_SUB_3D, NXG_3D_PIPE_NOP);
   *c++ = NXG_HDR(NXG_NONINCR, nubo, NXG_SUB_3D, NXG_3D_CB_DATA);
   memcpy(c, ubo, nubo * sizeof(float));
   c += nubo;
   /* Over the menu's MVP: sent again with its next draw */
   if (off < 64)
      g->mvp3d_valid = false;
   if (!g->layout_fx)
   {
      /* Positions alone, eight bytes apart; nothing fetched past them */
      c    = nxg_3d_m(c, NXG_3D_VERTEX_ARRAY, (1u << 12) | 8);
      *c++ = NXG_HDR(NXG_INCR, 2, NXG_SUB_3D, NXG_3D_ATTRIBS + 1);
      *c++ = (1u << 6) | (0x12u << 21) | (7u << 27);
      *c++ = (1u << 6) | (0x12u << 21) | (7u << 27);
      g->layout_fx = true;
   }
   c = nxg_3d_addr(c, NXG_3D_VERTEX_ARRAY + 1, m->mem.iova);
#ifdef NXG_S2
   c = nxg_3d_addr(c, NXG_3D_VERTEX_SIZE, n * 8);
#else
   c = nxg_3d_addr(c, NXG_3D_VERTEX_LIMIT, m->mem.iova + n * 8 - 1);
#endif
   c = nxg_3d_m(c, NXG_3D_VERTEX_BEGIN, NXG_3D_PRIM_STRIP);
   c = nxg_3d_m(c, NXG_3D_DRAW_COUNT, n);
   c = nxg_3d_m(c, NXG_3D_VERTEX_END, 0);
   nxg_cmd_trim(g, c0, 300, c);
}
#else
static void gfx_display_switch_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp, void *data, unsigned video_dims)
{
   (void)draw; (void)p_disp; (void)data; (void)video_dims;
}
#endif

/* ---- textures --------------------------------------------------------- */

static uintptr_t switch_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   switch_video_t *sw        = (switch_video_t*)video_data;
   struct texture_image *img = (struct texture_image*)data;
   sw_surf_t *s;
   (void)threaded;
   (void)filter_type;
   if (     !sw || !img || !img->pixels || !img->width || !img->height
         || !VIDEO_SCALE_FITS(img->width, img->height))
      return 0;
   if (!(s = sw_surf_new(sw, VIDEO_SCALE_PACK(img->width, img->height))))
      return 0;
   sw_surf_fill_argb(s, img->pixels, img->width, img->height,
         img->supports_rgba);
   sw_surf_flush(sw, s, 0, img->height);
   return (uintptr_t)s;
}

static void switch_unload_texture(void *data, bool threaded,
      uintptr_t handle)
{
   (void)threaded;
   if (data && handle)
      sw_surf_free((switch_video_t*)data, (sw_surf_t*)handle);
}

/* ---- font ------------------------------------------------------------- */

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void                         *font_data;
   switch_video_t               *sw;
   sw_surf_t                    *atlas;
   unsigned                      atlas_dims;
} switch_font_t;

static void *switch_font_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)calloc(1, sizeof(switch_font_t));
   (void)is_threaded;
   if (!font)
      return NULL;
   if (!font_renderer_create_default(&font->font_driver,
            &font->font_data, font_path, font_size, FONT_ATLAS_FORMAT_A8))
   {
      free(font);
      return NULL;
   }
   font->sw = (switch_video_t*)data;
   {
      struct font_atlas *a = font->font_driver->get_atlas(font->font_data);
      if (a)
      {
         /* It may grow, the surface following; with the GPU its cells
          * are copied where frames still in flight may be reading */
         a->max_dims = VIDEO_SCALE_PACK(2048, 2048);
#ifdef SWITCH_NX_GPU
         if (font->sw && font->sw->gpu)
            a->inflight = NXG_FRAMES;
#endif
      }
   }
   return font;
}

static void switch_font_free(void *data, bool is_threaded)
{
   switch_font_t *font = (switch_font_t *)data;
   (void)is_threaded;
   if (!font)
      return;
   if (font->atlas)
      sw_surf_free(font->sw, font->atlas);
   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);
   free(font);
}

/* The atlas's coverage as premultiplied white: the dirty rectangles,
 * or all of it at a new size */
static bool switch_font_upload(switch_font_t *font)
{
   struct font_atlas *a = font->font_driver->get_atlas(font->font_data);
   unsigned dims, i, n;
   if (!a || !a->buffer || !a->width || !a->height || !font->sw)
      return false;
   dims = VIDEO_SCALE_PACK(a->width, a->height);
   if (!font->atlas || font->atlas_dims != dims)
   {
      sw_surf_t *s = sw_surf_new(font->sw, dims);
      if (!s)
         return false;
      if (font->atlas)
         sw_surf_free(font->sw, font->atlas);
      font->atlas      = s;
      font->atlas_dims = dims;
      a->dirty         = true;
      a->dirty_rects   = 0;
      a->dirty_xy0     = VIDEO_SCALE_PACK(0, 0);
      a->dirty_xy1     = dims;
   }
   if (!a->dirty)
      return true;
   n = font_atlas_dirty_rects(a);
   for (i = 0; i < n; i++)
   {
      unsigned xy0, xy1, x, y, x1, y1;
      font_atlas_dirty_rect(a, i, &xy0, &xy1);
      x1 = VIDEO_SCALE_W(xy1);
      y1 = VIDEO_SCALE_H(xy1);
      if (x1 > a->width)  x1 = a->width;
      if (y1 > a->height) y1 = a->height;
      for (y = VIDEO_SCALE_H(xy0); y < y1; y++)
      {
         const uint8_t *in = a->buffer + (size_t)y * a->width;
         uint32_t *out     = (uint32_t*)(font->atlas->pixels
               + (size_t)y * font->atlas->pitch);
         for (x = VIDEO_SCALE_W(xy0); x < x1; x++)
            out[x] = (uint32_t)in[x] * 0x01010101u;
      }
      if (y1 > VIDEO_SCALE_H(xy0))
         sw_surf_flush(font->sw, font->atlas, VIDEO_SCALE_H(xy0),
               y1 - VIDEO_SCALE_H(xy0));
   }
   a->dirty       = false;
   a->dirty_rects = 0;
   return true;
}

static int switch_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   switch_font_t *font = (switch_font_t*)data;
   if (!font)
      return 0;
   return font_renderer_get_message_width(font->font_driver,
         font->font_data, msg, msg_len, scale);
}

/* One line's glyphs at pen (@x, @y), the baseline, top-down pixels */
static void switch_font_line(switch_font_t *font, const char *msg,
      size_t len, float x, float y, float scale, uint32_t color)
{
   const struct font_glyph *fallback = font->font_driver->get_glyph(
         font->font_data, '?');
   const char *end = msg + len;
   float dx = 0.0f, dy = 0.0f;
   while (msg < end)
   {
      const struct font_glyph *g;
      uint32_t code = utf8_walk(&msg);
      if (     !(g = font->font_driver->get_glyph(font->font_data, code))
            && !(g = fallback))
         continue;
      if (VIDEO_SCALE_W(g->dims) && VIDEO_SCALE_H(g->dims))
      {
         sw_op_t op;
         memset(&op, 0, sizeof(op));
         op.src      = font->atlas;
         op.sx       = (float)VIDEO_SCALE_W(g->atlas_pos);
         op.sy       = (float)VIDEO_SCALE_H(g->atlas_pos);
         op.sw       = (float)VIDEO_SCALE_W(g->dims);
         op.sh       = (float)VIDEO_SCALE_H(g->dims);
         op.dx       = x + ((float)g->draw_offset_x + dx) * scale;
         op.dy       = y + ((float)g->draw_offset_y + dy) * scale;
         op.dw       = op.sw * scale;
         op.dh       = op.sh * scale;
         op.color[0] = color;
         op.blend    = true;
         op.filter   = scale != 1.0f;
         sw_op_draw(font->sw, &op);
      }
      dx += (float)g->advance_x;
      dy += (float)g->advance_y;
   }
}

static void switch_font_render_msg(void *userdata, void *data,
      const char *msg, size_t msg_len, const struct font_params *params)
{
   switch_font_t *font = (switch_font_t*)data;
   switch_video_t *sw  = (switch_video_t*)userdata;
   struct font_line_metrics *lm = NULL;
   font_params_resolved_t rp;
   uint32_t color, shadow;
   float line_h = 0.0f, out_w, out_h;
   const char *start, *p;
   unsigned line = 0;
   bool shadows;

   if (!font || !sw || !msg || !msg_len || !*msg)
      return;
   font->sw = sw;
   if (!switch_font_upload(font))
      return;
   font_driver_resolve_params(params, &rp);
   color   = sw_pack_premul(rp.color[0], rp.color[1], rp.color[2],
         rp.color[3]);
   shadow  = sw_pack_premul(rp.color[0] * rp.drop_mod,
         rp.color[1] * rp.drop_mod, rp.color[2] * rp.drop_mod,
         rp.color[3] * rp.drop_alpha);
   shadows = (rp.drop_x || rp.drop_y) && (shadow >> 24);
   out_w   = (float)VIDEO_SCALE_W(sw->out_dims);
   out_h   = (float)VIDEO_SCALE_H(sw->out_dims);
   if (font->font_driver->get_line_metrics)
      font->font_driver->get_line_metrics(font->font_data, &lm);
   if (lm)
      line_h = lm->height * rp.scale;

   start = p = msg;
   for (;;)
   {
      if (p == msg + msg_len || *p == '\n' || !*p)
      {
         size_t len = (size_t)(p - start);
         if (len)
         {
            float x = rp.x * out_w;
            float y = (1.0f - rp.y) * out_h + (float)line * line_h;
            if (rp.text_align != TEXT_ALIGN_LEFT)
            {
               float w = (float)font_renderer_get_message_width(
                     font->font_driver, font->font_data, start, len,
                     rp.scale);
               x -= rp.text_align == TEXT_ALIGN_CENTER ? w * 0.5f : w;
            }
            if (shadows)
               switch_font_line(font, start, len,
                     x + (float)rp.drop_x, y - (float)rp.drop_y,
                     rp.scale, shadow);
            switch_font_line(font, start, len, x, y, rp.scale, color);
         }
         if (p == msg + msg_len || !*p)
            break;
         start = ++p;
         line++;
      }
      else
         p++;
   }
}

static const struct font_glyph *switch_font_get_glyph(
    void *data, uint32_t code)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void *)font->font_data, code);
   return NULL;
}

static bool switch_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   switch_font_t *font = (switch_font_t *)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

/* ---- the CPU's frame -------------------------------------------------- */

/* Black around @pos/@dims inside @screen_dims, @pitch pixels a row:
 * the frame is scaled straight into the framebuffer, whose border
 * still holds the last frame's OSD and overlay. */
static void switch_clear_border(uint32_t *buf, unsigned pitch,
      unsigned screen_dims, unsigned pos, unsigned dims)
{
   unsigned sw = VIDEO_SCALE_W(screen_dims);
   unsigned sh = VIDEO_SCALE_H(screen_dims);
   unsigned x0 = VIDEO_POS_X(pos);
   unsigned y0 = VIDEO_POS_Y(pos);
   unsigned x1 = x0 + VIDEO_SCALE_W(dims);
   unsigned y1 = y0 + VIDEO_SCALE_H(dims);
   unsigned y;
   if (y0)
      memset(buf, 0, (size_t)pitch * y0 * sizeof(uint32_t));
   for (y = y0; y < y1; y++)
   {
      uint32_t *row = buf + (size_t)y * pitch;
      if (x0)
         memset(row, 0, x0 * sizeof(uint32_t));
      if (x1 < sw)
         memset(row + x1, 0, (sw - x1) * sizeof(uint32_t));
   }
   if (y1 < sh)
      memset(buf + (size_t)y1 * pitch, 0,
            (size_t)pitch * (sh - y1) * sizeof(uint32_t));
}

#ifdef HAVE_MENU
/* The menu's texels without alpha black, the rest opaque colour with
 * the alpha byte cleared, in place over its rectangle */
static void switch_menu_mask(uint32_t *buf, unsigned pitch,
      unsigned w, unsigned h)
{
   unsigned x, y;
   for (y = 0; y < h; y++, buf += pitch)
      for (x = 0; x < w; x++)
      {
         uint32_t p = buf[x];
         buf[x]     = (p >> 24) ? (p & 0x00ffffffu) : 0;
      }
}
#endif

/* needed to clear surface completely as hw scaling doesn't always scale to full resolution perflectly */
static void clear_screen(switch_video_t *sw)
{
    uint32_t stride;
    uint32_t *out_buffer;

    nwindowSetDimensions(sw->win, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims));

    out_buffer = (uint32_t*)framebufferBegin(&sw->fb, &stride);

    memset(out_buffer, 0, stride * 720);

    framebufferEnd(&sw->fb);
}

/* ---- overlay ---------------------------------------------------------- */
#ifdef HAVE_OVERLAY
static void switch_overlay_free(switch_video_t *sw)
{
   unsigned i;
   for (i = 0; i < sw->overlays_size; i++)
   {
      free(sw->overlays[i].pixels);
      sw_surf_free(sw, (sw_surf_t*)sw->overlays[i].surf);
   }
   free(sw->overlays);
   sw->overlays      = NULL;
   sw->overlays_size = 0;
}

static void switch_overlay_enable(void *data, bool state)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (sw)
      sw->overlay_enabled = state;
}

/* The page is copied once: into the framebuffer's channel order for the
 * CPU, premultiplied into GPU memory for the 2D engine. The pack's
 * pixels are not this driver's to keep. */
static bool switch_overlay_load(void *data,
            const void *image_data, unsigned num_images)
{
   unsigned i;
   switch_video_t                   *sw = (switch_video_t *)data;
   const struct texture_image   *images = (const struct texture_image *)image_data;

   if (!sw)
      return false;

   switch_overlay_free(sw);
   if (!num_images || !images)
      return true;

   if (!(sw->overlays = (switch_overlay_t*)calloc(num_images,
               sizeof(*sw->overlays))))
      return false;
   sw->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      size_t p, n;
      unsigned r;
      const uint32_t *src;
      uint32_t *dst;
      switch_overlay_t *o = &sw->overlays[i];

      o->tex[2]  = o->tex[3]  = 1.0f;
      o->vert[2] = o->vert[3] = 1.0f;
      o->alpha   = 1.0f;
      if (     !images[i].pixels || !images[i].width || !images[i].height
            || !VIDEO_SCALE_FITS(images[i].width, images[i].height))
         continue;
      o->dims  = VIDEO_SCALE_PACK(images[i].width, images[i].height);
#ifdef SWITCH_NX_GPU
      if (sw->gpu)
      {
         sw_surf_t *s = sw_surf_new(sw, o->dims);
         if (!s)
         {
            switch_overlay_free(sw);
            return false;
         }
         sw_surf_fill_argb(s, images[i].pixels, images[i].width,
               images[i].height, images[i].supports_rgba);
         sw_surf_flush(sw, s, 0, images[i].height);
         o->surf = s;
         continue;
      }
#endif
      n = (size_t)images[i].width * images[i].height;
      /* the pixels, then the row spans after them */
      if (!(o->pixels = (uint32_t*)malloc(n * sizeof(uint32_t)
                  + (size_t)images[i].height * 2 * sizeof(uint16_t))))
      {
         switch_overlay_free(sw);
         return false;
      }
      o->spans = (uint16_t*)(o->pixels + n);
      src      = images[i].pixels;
      dst      = o->pixels;
      /* R in the low byte, as the framebuffer and its scaler output */
      if (images[i].supports_rgba)
         memcpy(dst, src, n * sizeof(uint32_t));
      else
         for (p = 0; p < n; p++)
            dst[p] =  (src[p] & 0xff00ff00u)
                   | ((src[p] >> 16) & 0xffu)
                   | ((src[p] & 0xffu) << 16);
      for (r = 0; r < images[i].height; r++)
      {
         const uint32_t *row = dst + (size_t)r * images[i].width;
         unsigned first      = 0;
         unsigned last       = images[i].width;
         while (first < last && !(row[first] >> 24))
            first++;
         while (last > first && !(row[last - 1] >> 24))
            last--;
         o->spans[2 * r]     = (uint16_t)first;
         o->spans[2 * r + 1] = (uint16_t)last;
      }
   }

   return true;
}

static void switch_overlay_tex_geom(void *data,
            unsigned index, float x, float y, float w, float h)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].tex[0] = x;
   sw->overlays[index].tex[1] = y;
   sw->overlays[index].tex[2] = w;
   sw->overlays[index].tex[3] = h;
}

/* y-down from the top left, as the frontend gives it and the
 * framebuffer is laid out */
static void switch_overlay_vertex_geom(void *data,
            unsigned index, float x, float y, float w, float h)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].vert[0] = x;
   sw->overlays[index].vert[1] = y;
   sw->overlays[index].vert[2] = w;
   sw->overlays[index].vert[3] = h;
}

static void switch_overlay_full_screen(void *data, bool enable)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (sw)
      sw->overlay_full_screen = enable;
}

static void switch_overlay_set_alpha(void *data, unsigned index, float mod)
{
   switch_video_t *sw = (switch_video_t *)data;
   if (!sw || index >= sw->overlays_size)
      return;
   sw->overlays[index].alpha = mod;
}

#ifdef SWITCH_NX_GPU
/* The page through the 2D engine, each image one blend */
static void switch_overlay_render_gpu(switch_video_t *sw,
      unsigned frame_pos, unsigned frame_dims)
{
   unsigned i;
   float screen_w = (float)VIDEO_SCALE_W(sw->out_dims);
   float screen_h = (float)VIDEO_SCALE_H(sw->out_dims);
   float base_x   = sw->overlay_full_screen ? 0.0f : (float)VIDEO_POS_X(frame_pos);
   float base_y   = sw->overlay_full_screen ? 0.0f : (float)VIDEO_POS_Y(frame_pos);
   float base_w   = sw->overlay_full_screen ? screen_w : (float)VIDEO_SCALE_W(frame_dims);
   float base_h   = sw->overlay_full_screen ? screen_h : (float)VIDEO_SCALE_H(frame_dims);
   for (i = 0; i < sw->overlays_size; i++)
   {
      const switch_overlay_t *o = &sw->overlays[i];
      const sw_surf_t *s        = (const sw_surf_t*)o->surf;
      sw_op_t op;
      if (!s || o->alpha <= 0.0f)
         continue;
      memset(&op, 0, sizeof(op));
      op.src      = s;
      op.sx       = o->tex[0] * (float)VIDEO_SCALE_W(o->dims);
      op.sy       = o->tex[1] * (float)VIDEO_SCALE_H(o->dims);
      op.sw       = o->tex[2] * (float)VIDEO_SCALE_W(o->dims);
      op.sh       = o->tex[3] * (float)VIDEO_SCALE_H(o->dims);
      op.dx       = base_x + o->vert[0] * base_w;
      op.dy       = base_y + o->vert[1] * base_h;
      op.dw       = o->vert[2] * base_w;
      op.dh       = o->vert[3] * base_h;
      op.color[0] = sw_pack_premul(1.0f, 1.0f, 1.0f, o->alpha);
      op.blend    = true;
      op.filter   = true;
      sw_op_draw(sw, &op);
   }
}
#endif

/* Blend the page over @buf, @pitch pixels a row: across @screen_dims,
 * or the frame at @frame_pos, @frame_dims unless full screen. Nearest
 * sampling at pixel centres, stepped in 16.16 once per row and once
 * per image for the columns; opaque texels are stored, transparent
 * ones skipped. */
static void switch_overlay_render(switch_video_t *sw, uint32_t *buf,
      unsigned pitch, unsigned screen_dims, unsigned frame_pos,
      unsigned frame_dims)
{
   unsigned i;
   int screen_w = (int)VIDEO_SCALE_W(screen_dims);
   int screen_h = (int)VIDEO_SCALE_H(screen_dims);
   int base_x   = sw->overlay_full_screen ? 0 : VIDEO_POS_X(frame_pos);
   int base_y   = sw->overlay_full_screen ? 0 : VIDEO_POS_Y(frame_pos);
   int base_w   = sw->overlay_full_screen ? screen_w
      : (int)VIDEO_SCALE_W(frame_dims);
   int base_h   = sw->overlay_full_screen ? screen_h
      : (int)VIDEO_SCALE_H(frame_dims);

   for (i = 0; i < sw->overlays_size; i++)
   {
      int x, y, dst_x, dst_y, dst_w, dst_h, x0, y0, x1, y1, x_first, x_last;
      int64_t sx, sx_step, sy_step, sy;
      unsigned mod;
      const switch_overlay_t *o = &sw->overlays[i];
      int ol_w                  = (int)VIDEO_SCALE_W(o->dims);
      int ol_h                  = (int)VIDEO_SCALE_H(o->dims);
      uint16_t *cols            = sw->overlay_cols;

      if (     !o->pixels || o->alpha <= 0.0f
            || o->tex[2] <= 0.0f || o->tex[3] <= 0.0f)
         continue;
      dst_x = base_x + (int)floor(o->vert[0] * (double)base_w + 0.5);
      dst_y = base_y + (int)floor(o->vert[1] * (double)base_h + 0.5);
      dst_w = (int)(o->vert[2] * (float)base_w + 0.5f);
      dst_h = (int)(o->vert[3] * (float)base_h + 0.5f);
      if (dst_w <= 0 || dst_h <= 0)
         continue;
      x0 = dst_x < 0 ? 0 : dst_x;
      y0 = dst_y < 0 ? 0 : dst_y;
      x1 = dst_x + dst_w > screen_w ? screen_w : dst_x + dst_w;
      y1 = dst_y + dst_h > screen_h ? screen_h : dst_y + dst_h;
      if (x1 > (int)ARRAY_SIZE(sw->overlay_cols))
         x1 = (int)ARRAY_SIZE(sw->overlay_cols);
      if (x0 >= x1 || y0 >= y1)
         continue;
      mod      = o->alpha >= 1.0f ? 255u
         : (unsigned)(o->alpha * 255.0f + 0.5f);

      sx_step  = (int64_t)((double)o->tex[2] * ol_w * 65536.0 / dst_w);
      sy_step  = (int64_t)((double)o->tex[3] * ol_h * 65536.0 / dst_h);
      sx       = (int64_t)((double)o->tex[0] * ol_w * 65536.0)
         + sx_step / 2 + (int64_t)(x0 - dst_x) * sx_step;
      sy       = (int64_t)((double)o->tex[1] * ol_h * 65536.0)
         + sy_step / 2 + (int64_t)(y0 - dst_y) * sy_step;
      for (x = x0; x < x1; x++, sx += sx_step)
      {
         int col = (int)(sx >> 16);
         cols[x] = (uint16_t)(col < 0 ? 0 : col >= ol_w ? ol_w - 1 : col);
      }

      for (y = y0; y < y1; y++, sy += sy_step)
      {
         int lo, hi, row    = (int)(sy >> 16);
         unsigned first, last;
         const uint32_t *in;
         uint32_t *out      = buf + (size_t)y * pitch;
         if (row < 0)
            row = 0;
         else if (row >= ol_h)
            row = ol_h - 1;
         first = o->spans[2 * row];
         last  = o->spans[2 * row + 1];
         if (first >= last)
            continue;
         /* The screen columns the row's span maps to: cols[] only
          * ever grows */
         lo = x0;
         hi = x1;
         while (lo < hi)
         {
            int mid = (lo + hi) >> 1;
            if (cols[mid] < first)
               lo = mid + 1;
            else
               hi = mid;
         }
         x_first = lo;
         hi      = x1;
         while (lo < hi)
         {
            int mid = (lo + hi) >> 1;
            if (cols[mid] < last)
               lo = mid + 1;
            else
               hi = mid;
         }
         x_last = lo;
         in     = o->pixels + (size_t)row * ol_w;

         for (x = x_first; x < x_last; x++)
         {
            uint32_t s = in[cols[x]];
            uint32_t d, rb, g, t;
            unsigned a = s >> 24, ia;
            if (!a)
               continue;
            if (mod != 255u)
            {
               /* exact a * mod / 255, rounded */
               t = a * mod + 128u;
               a = (t + (t >> 8)) >> 8;
               if (!a)
                  continue;
            }
            d = out[x];
            if (a == 255u)
            {
               out[x] = (s & 0x00ffffffu) | (d & 0xff000000u);
               continue;
            }
            /* exact x / 255, two channels at once */
            ia = 255u - a;
            rb = (s & 0x00ff00ffu) * a + (d & 0x00ff00ffu) * ia + 0x00800080u;
            rb = ((rb + ((rb >> 8) & 0x00ff00ffu)) >> 8) & 0x00ff00ffu;
            g  = ((s >> 8) & 0xffu) * a + ((d >> 8) & 0xffu) * ia + 0x80u;
            g  = ((g + (g >> 8)) >> 8) & 0xffu;
            out[x] = rb | (g << 8) | (d & 0xff000000u);
         }
      }
   }
}
#endif

/* ---- the GPU's frame -------------------------------------------------- */
#ifdef SWITCH_NX_GPU

/* Where @frame lies when the 2D engine can read it in place: the slot
 * lent to the core or the wrapper's frame memory */
static nxg_linear_t *nxg_find(nxg_t *g, const void *frame, unsigned dims,
      unsigned pitch, unsigned bpp, uint64_t *iova, unsigned *x)
{
   uintptr_t p = (uintptr_t)frame;
   unsigned i;
   if (pitch % SW_PITCH_ALIGN)
      return NULL;
   for (i = 0; i < NXG_SW_SLOTS + NXG_FRAME_MEM; i++)
   {
      nxg_linear_t *l = (i < NXG_SW_SLOTS)
         ? &g->sw[i] : &g->frame_mem[i - NXG_SW_SLOTS];
      uintptr_t base  = (uintptr_t)l->mem.cpu;
      uintptr_t off;
      if (!l->mem.cpu || p < base || p >= base + l->mem.size)
         continue;
      off = p - base;
      if (     off + (uintptr_t)pitch * VIDEO_SCALE_H(dims) > l->mem.size
            || (off % pitch) % bpp
            || (off % pitch) / bpp + VIDEO_SCALE_W(dims) > pitch / bpp)
         return NULL;
      *iova = l->mem.iova + (off - off % pitch);
      *x    = (unsigned)((off % pitch) / bpp);
      armDCacheFlush(l->mem.cpu + (off - off % pitch),
            (size_t)pitch * VIDEO_SCALE_H(dims));
      return l;
   }
   return NULL;
}

/* The core's frame scaled into the viewport: read where it lies, or
 * copied once into a free ring slot */
static nxg_linear_t *nxg_content(switch_video_t *sw, const void *frame,
      unsigned dims, unsigned pitch)
{
   nxg_t *g       = (nxg_t*)sw->gpu;
   unsigned bpp   = sw->rgb32 ? 4 : 2;
   unsigned w     = VIDEO_SCALE_W(dims);
   unsigned h     = VIDEO_SCALE_H(dims);
   uint32_t pt    = SW_ALIGN(w * bpp, SW_PITCH_ALIGN);
   nxg_linear_t *l;
   uint64_t iova;
   unsigned x     = 0, y;
   int s;

   if ((l = nxg_find(g, frame, dims, pitch, bpp, &iova, &x)))
   {
      if (l >= g->sw && l < g->sw + NXG_SW_SLOTS)
         g->sw_last = (int)(l - g->sw);
      g->sw_lent = -1;
   }
   else
   {
      if ((s = nxg_linear_pick(g, g->sw, NXG_SW_SLOTS, g->sw_last,
                  g->sw_lent)) < 0)
         return NULL;
      l = &g->sw[s];
      if (!nxg_linear_fit(g, l, pt * h))
         return NULL;
      if (pitch == pt)
         memcpy(l->mem.cpu, frame, (size_t)pt * h);
      else
         for (y = 0; y < h; y++)
            memcpy(l->mem.cpu + (size_t)y * pt,
                  (const uint8_t*)frame + (size_t)y * pitch, w * bpp);
      armDCacheFlush(l->mem.cpu, (size_t)pt * h);
      l->pitch   = pt;
      l->dims    = dims;
      g->sw_last = s;
      iova       = l->mem.iova;
      pitch      = pt;
   }
   g->last_owner = l;
   g->last_iova  = iova;
   g->last_pitch = pitch;
   g->last_dims  = dims;
   g->last_x     = x;
   return l;
}

/* The last frame's source scaled into the viewport */
static nxg_linear_t *nxg_content_blit(switch_video_t *sw)
{
   nxg_t *g   = (nxg_t*)sw->gpu;
   unsigned w = VIDEO_SCALE_W(g->last_dims);
   unsigned h = VIDEO_SCALE_H(g->last_dims);
   unsigned x = g->last_x;
   if (!g->last_owner || !VIDEO_SCALE_W(sw->vp.dims)
         || !VIDEO_SCALE_H(sw->vp.dims))
      return NULL;
   if (!nxg_source(g, g->last_iova, g->last_pitch,
            VIDEO_SCALE_PACK(x + w, h),
            sw->rgb32 ? NXG_FMT_BGRX8 : NXG_FMT_B5G6R5))
      return NULL;
   nxg_blit(g, NXG_OP_SRCCOPY_PREMULT, 0xffffffffu, sw->smooth,
         VIDEO_POS_X(sw->vp.pos), VIDEO_POS_Y(sw->vp.pos),
         (int)VIDEO_SCALE_W(sw->vp.dims), (int)VIDEO_SCALE_H(sw->vp.dims),
         (double)x, 0.0,
         (double)w / (double)VIDEO_SCALE_W(sw->vp.dims),
         (double)h / (double)VIDEO_SCALE_H(sw->vp.dims));
   return g->last_owner;
}

static bool nxg_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   switch_video_t *sw = (switch_video_t*)data;
   nxg_t *g;
   unsigned bpp;
   uint32_t pt;
   int s;
   if (!sw || !(g = (nxg_t*)sw->gpu) || !fb->width || !fb->height)
      return false;
   bpp = sw->rgb32 ? 4 : 2;
   pt  = SW_ALIGN(fb->width * bpp, SW_PITCH_ALIGN);
   if ((s = nxg_linear_pick(g, g->sw, NXG_SW_SLOTS, g->sw_last, -1)) < 0)
      return false;
   if (!g->sw[s].mem.cpu || g->sw[s].mem.size < pt * fb->height)
   {
      /* A cached frame may point into the block being let go of */
      video_driver_cached_frame_retire();
      if (!nxg_linear_fit(g, &g->sw[s], pt * fb->height))
         return false;
   }
   g->sw[s].pitch   = pt;
   g->sw[s].dims    = VIDEO_SCALE_PACK(fb->width, fb->height);
   g->sw_lent       = s;
   fb->data         = g->sw[s].mem.cpu;
   fb->pitch        = pt;
   fb->format       = sw->rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   fb->memory_flags = RETRO_MEMORY_TYPE_CACHED;
   return true;
}

static void *nxg_frame_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *pitch_align)
{
   switch_video_t *sw = (switch_video_t*)data;
   nxg_t *g;
   unsigned i;
   if (!sw || !(g = (nxg_t*)sw->gpu) || size > 0xffffffffu)
      return NULL;
   for (i = 0; i < NXG_FRAME_MEM; i++)
   {
      nxg_linear_t *l = &g->frame_mem[i];
      if (l->used)
         continue;
      if (!nxg_mem_new(g, &l->mem, (uint32_t)size, false, false))
         return NULL;
      l->use_serial = 0;
      l->used       = true;
      *id           = i + 1;
      *cached       = true;
      *pitch_align  = SW_PITCH_ALIGN;
      return l->mem.cpu;
   }
   return NULL;
}

static bool nxg_frame_mem_ready(void *data, uintptr_t id)
{
   switch_video_t *sw = (switch_video_t*)data;
   nxg_t *g;
   if (!sw || !(g = (nxg_t*)sw->gpu) || !id || id > NXG_FRAME_MEM)
      return true;
   if (g->frame_mem[id - 1].use_serial <= g->done_serial)
      return true;
   nxg_poll_done(g);
   return g->frame_mem[id - 1].use_serial <= g->done_serial;
}

static void nxg_frame_mem_free(void *data, uintptr_t id)
{
   switch_video_t *sw = (switch_video_t*)data;
   nxg_t *g;
   if (!sw || !(g = (nxg_t*)sw->gpu) || !id || id > NXG_FRAME_MEM)
      return;
   if (g->last_owner == &g->frame_mem[id - 1])
      g->last_owner = NULL;
   nxg_retire(g, &g->frame_mem[id - 1].mem);
   g->frame_mem[id - 1].used = false;
}

#ifdef HAVE_MENU
/* RGUI, premultiplied into a ring slot: its RGBA4444 has red in the
 * high nibble */
static void nxg_set_texture_frame(switch_video_t *sw, const void *frame,
      bool rgb32, unsigned dims)
{
   nxg_t *g    = (nxg_t*)sw->gpu;
   unsigned w  = VIDEO_SCALE_W(dims);
   unsigned h  = VIDEO_SCALE_H(dims);
   uint32_t pt = SW_ALIGN(w * 4, SW_PITCH_ALIGN);
   nxg_linear_t *l;
   unsigned x, y;
   int s;
   if ((s = nxg_linear_pick(g, g->rgui, NXG_RGUI_SLOTS, g->rgui_last,
               -1)) < 0)
      return;
   l = &g->rgui[s];
   if (!nxg_linear_fit(g, l, pt * h))
      return;
   for (y = 0; y < h; y++)
   {
      uint32_t *out = (uint32_t*)(l->mem.cpu + (size_t)y * pt);
      if (rgb32)
      {
         const uint32_t *in = (const uint32_t*)frame + (size_t)y * w;
         for (x = 0; x < w; x++)
         {
            uint32_t p = in[x], t, a = p >> 24;
            uint32_t r = (p >> 16) & 0xffu, gg = (p >> 8) & 0xffu, b = p & 0xffu;
            if (a != 255u)
            {
               r  = SW_MUL255(r,  a, t);
               gg = SW_MUL255(gg, a, t);
               b  = SW_MUL255(b,  a, t);
            }
            out[x] = r | (gg << 8) | (b << 16) | (a << 24);
         }
      }
      else
      {
         const uint16_t *in = (const uint16_t*)frame + (size_t)y * w;
         for (x = 0; x < w; x++)
         {
            uint32_t p = in[x], t;
            uint32_t r = ((p >> 12) & 0xfu) * 17u;
            uint32_t gg = ((p >> 8) & 0xfu) * 17u;
            uint32_t b = ((p >> 4) & 0xfu) * 17u;
            uint32_t a = (p & 0xfu) * 17u;
            if (a != 255u)
            {
               r  = SW_MUL255(r,  a, t);
               gg = SW_MUL255(gg, a, t);
               b  = SW_MUL255(b,  a, t);
            }
            out[x] = r | (gg << 8) | (b << 16) | (a << 24);
         }
      }
   }
   armDCacheFlush(l->mem.cpu, (size_t)pt * h);
   l->pitch      = pt;
   l->dims       = dims;
   g->rgui_last  = s;
}

/* RGUI over the frame at the largest whole scale, blended */
static nxg_linear_t *nxg_draw_rgui(switch_video_t *sw)
{
   nxg_t *g = (nxg_t*)sw->gpu;
   nxg_linear_t *l;
   unsigned w, h, ow, oh, k, tw, th;
   if (g->rgui_last < 0)
      return NULL;
   l  = &g->rgui[g->rgui_last];
   w  = VIDEO_SCALE_W(l->dims);
   h  = VIDEO_SCALE_H(l->dims);
   ow = VIDEO_SCALE_W(g->out_dims);
   oh = VIDEO_SCALE_H(g->out_dims);
   if (!w || !h)
      return NULL;
   k  = MIN(ow / w, oh / h);
   if (!k)
      k = 1;
   tw = sw->menu_texture.fullscreen ? ow : w * k;
   th = sw->menu_texture.fullscreen ? oh : h * k;
   if (!nxg_source(g, l->mem.iova, l->pitch, l->dims, NXG_FMT_RGBA8))
      return NULL;
   nxg_blit(g, NXG_OP_BLEND_PREMULT,
         sw_pack_premul(1.0f, 1.0f, 1.0f, sw->menu_texture.alpha),
         sw->smooth && sw->menu_texture.fullscreen,
         (int)(ow - tw) / 2, (int)(oh - th) / 2, (int)tw, (int)th,
         0.0, 0.0, (double)w / tw, (double)h / th);
   return l;
}
#endif

static bool nxg_frame(switch_video_t *sw, const void *frame,
      unsigned dims, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   nxg_t *g            = (nxg_t*)sw->gpu;
   nxg_linear_t *owner = NULL;
#ifdef HAVE_MENU
   nxg_linear_t *rgui  = NULL;
   bool menu_alive     = (video_info->menu_st_flags
         & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
   int op_mode         = (int)appletGetOperationMode();
   bool full;

   if (op_mode != g->op_mode)
   {
      g->op_mode        = op_mode;
      g->out_dims       = nxg_mode_dims(op_mode);
#ifndef NXG_S2
      nwindowSetCrop(sw->win, 0, 0, VIDEO_SCALE_W(g->out_dims),
            VIDEO_SCALE_H(g->out_dims));
#endif
      sw->should_resize = true;
   }
   sw->out_dims = g->out_dims;
   if (!g->swap_interval_set || sw->vsync_changed)
   {
      nwindowSetSwapInterval(sw->win, sw->vsync ? 1 : 0);
      g->swap_interval_set = true;
      sw->vsync_changed    = false;
   }
   if (sw->should_resize || sw->vp.full_dims != g->out_dims)
   {
      sw->vp.full_dims = g->out_dims;
      if (sw->o_size)
      {
         sw->vp.pos  = VIDEO_POS_PACK(
               ((int)VIDEO_SCALE_W(g->out_dims) - (int)VIDEO_SCALE_W(sw->o_dims)) / 2,
               ((int)VIDEO_SCALE_H(g->out_dims) - (int)VIDEO_SCALE_H(sw->o_dims)) / 2);
         sw->vp.dims = sw->o_dims;
      }
      else
         video_driver_update_viewport(&sw->vp, false, sw->keep_aspect, true);
      if (VIDEO_POS_X(sw->vp.pos) < 0)
         VIDEO_POS_PUT_X(sw->vp.pos, 0);
      if (VIDEO_POS_Y(sw->vp.pos) < 0)
         VIDEO_POS_PUT_Y(sw->vp.pos, 0);
      sw->should_resize = false;
   }

   /* The frame's source, settled before a window buffer is taken: a
    * dupe reads the last one again */
   if (frame && dims)
      nxg_content(sw, frame, dims, pitch);

   if (!nxg_frame_begin(g, sw->win))
      return false;
   sw->scissor_on = false;
   sw->blend      = false;

   full = g->last_owner
      && sw->vp.pos  == VIDEO_POS_PACK(0, 0)
      && sw->vp.dims == g->out_dims;
#ifdef HAVE_MENU
   if (sw->menu_texture.enable && sw->menu_texture.fullscreen)
      full = false;
#endif
   if (!full)
   {
      sw_op_t op;
      memset(&op, 0, sizeof(op));
      op.dw       = (float)VIDEO_SCALE_W(g->out_dims);
      op.dh       = (float)VIDEO_SCALE_H(g->out_dims);
      op.color[0] = 0xff000000u;
      sw_op_draw(sw, &op);
   }
   owner = nxg_content_blit(sw);

#ifdef HAVE_MENU
   if (sw->menu_texture.enable)
      rgui = nxg_draw_rgui(sw);
   if (menu_alive)
      menu_driver_frame(menu_alive, video_info);
#endif

#ifdef HAVE_OVERLAY
   if (sw->overlay_enabled && sw->overlays)
   {
#ifdef HAVE_MENU
      if (!(menu_alive && video_info->overlay_behind_menu))
#endif
         switch_overlay_render_gpu(sw, sw->vp.pos, sw->vp.dims);
   }
#endif

#ifdef HAVE_MENU
   if (!menu_alive)
#endif
   if (video_info->statistics_show)
      font_driver_render_msg(sw, video_info->stat_text,
            video_info->stat_text_len,
            (const struct font_params*)&video_info->osd_stat_params, NULL);
#ifdef HAVE_GFX_WIDGETS
   if (video_info->widgets_active)
      gfx_widgets_frame(video_info);
#endif
   if (msg && *msg)
      font_driver_render_msg(sw, msg, strlen(msg), NULL, NULL);

   nxg_frame_end(g, sw->win);
   if (owner)
      owner->use_serial = g->serial;
#ifdef HAVE_MENU
   if (rgui)
      rgui->use_serial  = g->serial;
#endif
   return true;
}
#endif /* SWITCH_NX_GPU */

/* ---- driver ----------------------------------------------------------- */

static bool switch_get_metrics(void *data, enum display_metric_types type,
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

static void *switch_init(const video_info_t *video)
{
    switch_video_t *sw = (switch_video_t *)calloc(1, sizeof(*sw));
    if (!sw)
        return NULL;

   sw->win = nwindowGetDefault();
   matrix_4x4_ortho(sw->mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

    sw->vp.pos          = VIDEO_POS_PACK(0, 0);
    sw->o_dims          = video->dims;
    sw->vp.dims         = video->dims;
    sw->overlay_enabled = false;
#ifdef HAVE_MENU
    sw->in_menu         = false;
    sw->menu_texture.alpha = 1.0f;
#endif

    sw->vp.full_dims    = VIDEO_SCALE_PACK(1280, 720);
    sw->out_dims        = sw->vp.full_dims;

    sw->vsync = video->vsync;
    sw->rgb32 = video->rgb32;
    sw->keep_aspect = true;
    sw->should_resize = true;
    sw->o_size = true;
    sw->is_threaded = video->is_threaded;
    sw->smooth = video->smooth;
    sw->menu_texture.enable = false;

#ifdef SWITCH_NX_GPU
    sw->gpu = nxg_init(sw->win);
#endif
#ifdef NXG_3D
    /* With the 3D engine the menus hand over matrices and strips */
    if (sw->gpu && ((nxg_t*)sw->gpu)->has3d)
    {
       gfx_display_ctx_switch.handles_transform    = false;
       gfx_display_ctx_switch.handles_vertex_strip = true;
    }
#endif
    if (!sw->gpu)
    {
       framebufferCreate(&sw->fb, sw->win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
       framebufferMakeLinear(&sw->fb);
       /* Sanity check */
       sw->vp.dims  = VIDEO_SCALE_PACK(MIN(VIDEO_SCALE_W(sw->vp.dims), VIDEO_SCALE_W(sw->vp.full_dims)),
             MIN(VIDEO_SCALE_H(sw->vp.dims), VIDEO_SCALE_H(sw->vp.full_dims)));
       clear_screen(sw);
    }

    switch_fake_ctx.get_metrics = switch_get_metrics;
    video_context_driver_set(&switch_fake_ctx);

    /* no input driver of this driver's own: the frontend starts the
     * platform's */
    input_driver_video_window(INPUT_WINDOW_PLATFORM, NULL);

    return sw;
}

static void switch_update_viewport(switch_video_t *sw)
{
    /* Handle o_size mode (original size) specially */
    if (sw->o_size)
    {
        sw->vp.pos    = VIDEO_POS_PACK((int)(((float)VIDEO_SCALE_W(sw->vp.full_dims) - VIDEO_SCALE_W(sw->o_dims))) / 2,
              (int)(((float)VIDEO_SCALE_H(sw->vp.full_dims) - VIDEO_SCALE_H(sw->o_dims))) / 2);
        sw->vp.dims   = sw->o_dims;
        return;
    }

    video_driver_update_viewport(&sw->vp, false, sw->keep_aspect, true);
}

static void switch_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
    switch_video_t *sw   = (switch_video_t *)data;

    if (!sw)
        return;

    sw->keep_aspect      = true;
    sw->o_size           = false;

    switch (aspect_ratio_idx)
    {
       case ASPECT_RATIO_CORE:
          sw->o_size      = true;
          sw->keep_aspect = false;
          break;

       case ASPECT_RATIO_CUSTOM:
          /* What the last frame carried, not what the setting says
           * now: this runs on the video thread under the threaded
           * wrapper, which is the default here. */
          if (sw->frame_scale_integer)
          {
             video_driver_set_viewport_core();
             sw->o_size      = true;
             sw->keep_aspect = false;
          }
          break;

       default:
          break;
    }


    sw->should_resize = true;
}

/* The CPU path's menus, widgets and text, over the frame drawn */
static void switch_frame_cpu_overdraw(switch_video_t *sw, const char *msg,
      video_frame_info_t *video_info, bool full_window)
{
#ifdef HAVE_MENU
   bool menu_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE)
      ? true : false;
   if (menu_alive && !sw->menu_texture.enable)
      menu_driver_frame(menu_alive, video_info);
   if (!menu_alive)
#endif
   if (video_info->statistics_show && full_window)
      font_driver_render_msg(sw, video_info->stat_text,
            video_info->stat_text_len,
            (const struct font_params*)&video_info->osd_stat_params, NULL);
#ifdef HAVE_GFX_WIDGETS
   if (video_info->widgets_active && full_window)
      gfx_widgets_frame(video_info);
#endif
   if (msg && *msg)
      font_driver_render_msg(sw, msg, strlen(msg), NULL, NULL);
}

static bool switch_frame(void *data, const void *frame,
      unsigned dims,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   uint32_t stride;
   switch_video_t   *sw = data;
   uint32_t *out_buffer = NULL;
   bool       ffwd_mode = video_info->input_driver_nonblock_state;
   bool full_window     = false;
#ifdef HAVE_MENU
   bool menu_is_alive   = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

#ifdef HAVE_MENU
   /* Travels with the frame, for set_aspect_ratio() to read rather
    * than the setting the menu writes */
   if (sw)
      sw->frame_scale_integer = video_info->scale_integer;
#endif

#ifdef SWITCH_NX_GPU
   if (sw->gpu)
      return nxg_frame(sw, frame, dims, pitch, msg, video_info);
#endif

   if (!frame)
      return true;

   if (ffwd_mode && !sw->is_threaded)
   {
      /* render every 4th frame when in ffwd mode and not threaded */
      if ((frame_count % 4) != 0)
         return true;
   }

   if (     sw->should_resize
         || (sw->last_dims != dims))
   {
      switch_update_viewport(sw);

      /* Sanity check */
      sw->vp.dims   = VIDEO_SCALE_PACK(MIN(VIDEO_SCALE_W(sw->vp.dims), VIDEO_SCALE_W(sw->vp.full_dims)),
            MIN(VIDEO_SCALE_H(sw->vp.dims), VIDEO_SCALE_H(sw->vp.full_dims)));

      scaler_ctx_gen_reset(&sw->scaler);

      sw->scaler.in_width  = width;
      sw->scaler.in_height = height;
      sw->scaler.in_stride = pitch;
      sw->scaler.in_fmt    = sw->rgb32
         ? SCALER_FMT_ARGB8888
         : SCALER_FMT_RGB565;

      if (!sw->smooth)
      {
         sw->scaler.out_width  = VIDEO_SCALE_W(sw->vp.dims);
         sw->scaler.out_height = VIDEO_SCALE_H(sw->vp.dims);
         sw->scaler.out_stride = VIDEO_SCALE_W(sw->vp.full_dims) * sizeof(uint32_t);
      }
      else
      {
         float screen_ratio    = (float)VIDEO_SCALE_W(sw->vp.full_dims) / VIDEO_SCALE_H(sw->vp.full_dims);
         float tgt_ratio       = (float)VIDEO_SCALE_W(sw->vp.dims) / VIDEO_SCALE_H(sw->vp.dims);

         sw->scaler.out_width  = width;
         sw->scaler.out_height = height;
         sw->scaler.out_stride = width * sizeof(uint32_t);

         sw->hw_scale.dims     = VIDEO_SCALE_PACK(
               ceil(screen_ratio / tgt_ratio * sw->scaler.out_width),
               sw->scaler.out_height);
         sw->hw_scale.x_offset = ceil(
               (VIDEO_SCALE_W(sw->hw_scale.dims)
                - sw->scaler.out_width) / 2.0);
#ifdef HAVE_MENU
         if (!menu_is_alive)
#endif
         {
            clear_screen(sw);
            nwindowSetDimensions(sw->win,
                  VIDEO_SCALE_W(sw->hw_scale.dims),
                  VIDEO_SCALE_H(sw->hw_scale.dims));
         }
      }

      sw->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      sw->scaler.scaler_type = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(&sw->scaler))
         return false;

      sw->last_dims          = dims;

      sw->should_resize      = false;
   }

   out_buffer     = (uint32_t*)framebufferBegin(&sw->fb, &stride);
   sw->out_buffer = out_buffer;
   sw->stride     = stride;
   sw->out_dims   = sw->vp.full_dims;

#ifdef HAVE_MENU
   if (sw->in_menu && !menu_is_alive && sw->smooth)
   {
      memset(out_buffer, 0, stride * VIDEO_SCALE_H(sw->vp.full_dims));
      nwindowSetDimensions(sw->win,
            VIDEO_SCALE_W(sw->hw_scale.dims),
            VIDEO_SCALE_H(sw->hw_scale.dims));
   }

   /* A menu drawn through gfx_display needs the whole window, as
    * RGUI's texture does */
   if (menu_is_alive && !sw->menu_texture.enable && sw->smooth)
   {
      if (!sw->in_menu)
         nwindowSetDimensions(sw->win, VIDEO_SCALE_W(sw->vp.full_dims),
               VIDEO_SCALE_H(sw->vp.full_dims));
      memset(out_buffer, 0, stride * VIDEO_SCALE_H(sw->vp.full_dims));
      full_window = true;
   }

   sw->in_menu = menu_is_alive;

   if (full_window)
      ;
   else if (sw->menu_texture.enable)
   {
      menu_driver_frame(menu_is_alive, video_info);

      if (sw->menu_texture.pixels)
      {
         unsigned pitch     = stride / sizeof(uint32_t);
         uint32_t *menu_out = out_buffer
               + ((VIDEO_SCALE_H(sw->vp.full_dims)
                     - VIDEO_SCALE_H(sw->menu_texture.tgt_dims)) / 2)
               * pitch
               + ((VIDEO_SCALE_W(sw->vp.full_dims)
                     - VIDEO_SCALE_W(sw->menu_texture.tgt_dims)) / 2);
         memset(out_buffer, 0, stride * VIDEO_SCALE_H(sw->vp.full_dims));
         sw->menu_texture.scaler.out_stride = stride;
         scaler_ctx_scale(&sw->menu_texture.scaler, menu_out,
               sw->menu_texture.pixels);
         switch_menu_mask(menu_out, pitch,
               VIDEO_SCALE_W(sw->menu_texture.tgt_dims),
               VIDEO_SCALE_H(sw->menu_texture.tgt_dims));
      }
   }
   else
#endif
      if (sw->smooth) /* bilinear */
      {
         /* The frame at its own size at x_offset: the compositor
          * scales the window up */
         struct scaler_ctx *ctx = &sw->scaler;
         ctx->out_stride        = stride;
         scaler_ctx_scale_direct(ctx, out_buffer + sw->hw_scale.x_offset,
               frame);
      }
      else
      {
         struct scaler_ctx *ctx = &sw->scaler;
         unsigned pitch         = stride / sizeof(uint32_t);
         switch_clear_border(out_buffer, pitch, sw->vp.full_dims,
               sw->vp.pos, sw->vp.dims);
         ctx->out_stride        = stride;
         scaler_ctx_scale(ctx, out_buffer
               + VIDEO_POS_Y(sw->vp.pos) * pitch
               + VIDEO_POS_X(sw->vp.pos), frame);
      }

#ifdef HAVE_OVERLAY
   if (sw->overlay_enabled && sw->overlays)
   {
      bool on_menu = false;
#ifdef HAVE_MENU
      on_menu      = sw->menu_texture.enable;
#endif
      /* The scaled-up frame is the framebuffer's top left, the window
       * cropped to it; the menu and the unscaled frame fill it */
      if (!(on_menu && video_info->overlay_behind_menu))
      {
         if (on_menu || !sw->smooth)
            switch_overlay_render(sw, out_buffer, stride / sizeof(uint32_t),
                  sw->vp.full_dims, sw->vp.pos, sw->vp.dims);
         else
            switch_overlay_render(sw, out_buffer, stride / sizeof(uint32_t),
                  sw->hw_scale.dims,
                  VIDEO_POS_PACK(sw->hw_scale.x_offset, 0),
                  VIDEO_SCALE_PACK(sw->scaler.out_width,
                     sw->scaler.out_height));
      }
   }
#endif

   if (!sw->smooth)
      full_window = true;
#ifdef HAVE_MENU
   if (sw->menu_texture.enable)
      full_window = true;
#endif
   switch_frame_cpu_overdraw(sw, msg, video_info, full_window);

   framebufferEnd(&sw->fb);
   sw->out_buffer = NULL;

   return true;
}

static void switch_set_nonblock_state(void *data, bool toggle, bool c, unsigned d)
{
    switch_video_t *sw = data;
    (void)c;
    (void)d;
    if (sw->vsync != !toggle)
       sw->vsync_changed = true;
    sw->vsync = !toggle;
}

static bool switch_alive(void *data) { (void)data; return true; }
static bool switch_focus(void *data) { (void)data; return true; }
static bool switch_suppress_screensaver(void *data, bool enable) { (void)data; (void)enable; return false; }
static bool switch_has_windowed(void *data) { (void)data; return false; }

static void switch_free(void *data)
{
    switch_video_t *sw = data;

#ifdef HAVE_OVERLAY
    switch_overlay_free(sw);
#endif
#ifdef SWITCH_NX_GPU
    if (sw->gpu)
       nxg_free((nxg_t*)sw->gpu, sw->win);
    else
#endif
       framebufferClose(&sw->fb);
    gfx_display_ctx_switch.handles_transform    = true;
    gfx_display_ctx_switch.handles_vertex_strip = false;
    scaler_ctx_gen_reset(&sw->scaler);
    scaler_ctx_gen_reset(&sw->menu_texture.scaler);

    if (sw->menu_texture.pixels)
        free(sw->menu_texture.pixels);

    free(sw);
}

static bool switch_set_shader(void *data,
            enum rarch_shader_type type, const char *path)
{
    (void)data;
    (void)type;
    (void)path;

    return false;
}

static void switch_set_rotation(void *data, unsigned rotation)
{
    switch_video_t *sw = data;
    if (!sw)
        return;
    sw->rotation = rotation;
}

static void switch_viewport_info(void *data, struct video_viewport *vp)
{
    switch_video_t *sw = data;
    *vp = sw->vp;
}

static void switch_set_texture_frame(
    void *data, const void *frame, bool rgb32,
    unsigned dims, float alpha)
{
    switch_video_t *sw = data;
    size_t sz = VIDEO_SCALE_AREA(dims) * (rgb32 ? 4 : 2);

    if (!sw || !frame || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
        return;
#ifdef HAVE_MENU
    sw->menu_texture.alpha = alpha;
#ifdef SWITCH_NX_GPU
    if (sw->gpu)
    {
        nxg_set_texture_frame(sw, frame, rgb32, dims);
        return;
    }
#endif
#endif

    if (   !sw->menu_texture.pixels
        || (sw->menu_texture.dims != dims))
    {
        int xsf, ysf, sf;
        struct scaler_ctx *sctx = NULL;

        /* realloc-to-tmp: on OOM the old buffer is kept, not leaked */
        if (sw->menu_texture.pixels)
        {
            void *tmp = realloc(sw->menu_texture.pixels, sz);
            if (!tmp)
                return;
            sw->menu_texture.pixels = tmp;
        }
        else
            sw->menu_texture.pixels = malloc(sz);

        if (!sw->menu_texture.pixels)
            return;

        xsf = 1280 / VIDEO_SCALE_W(dims);
        ysf = 720 / VIDEO_SCALE_H(dims);
        sf  = xsf;

        if (ysf < sf)
            sf = ysf;

        sw->menu_texture.dims     = dims;
        sw->menu_texture.tgt_dims = VIDEO_SCALE_PACK(
              VIDEO_SCALE_W(dims) * sf, VIDEO_SCALE_H(dims) * sf);

        sctx                    = &sw->menu_texture.scaler;
        scaler_ctx_gen_reset(sctx);

        sctx->in_width          = VIDEO_SCALE_W(dims);
        sctx->in_height         = VIDEO_SCALE_H(dims);
        sctx->in_stride         = VIDEO_SCALE_W(dims) * (rgb32 ? 4 : 2);
        sctx->in_fmt            = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
        sctx->out_width         = VIDEO_SCALE_W(sw->menu_texture.tgt_dims);
        sctx->out_height        = VIDEO_SCALE_H(sw->menu_texture.tgt_dims);
        sctx->out_stride        = 1280 * 4;
        sctx->out_fmt           = SCALER_FMT_ABGR8888;

        sctx->scaler_type       = SCALER_TYPE_POINT;

        if (!scaler_ctx_gen_filter(sctx))
            return;
    }

    memcpy(sw->menu_texture.pixels, frame, sz);
}

static void switch_apply_state_changes(void *data)
{
   switch_video_t *sw = (switch_video_t*)data;
   if (sw)
      sw->should_resize = true;
}

static void switch_set_texture_enable(void *data, bool enable, bool full_screen)
{
    switch_video_t *sw = data;
    if (!sw)
       return;
    if (!sw->gpu)
    {
       if (!sw->menu_texture.enable && enable)
           nwindowSetDimensions(sw->win, VIDEO_SCALE_W(sw->vp.full_dims), VIDEO_SCALE_H(sw->vp.full_dims));
       else if (!enable && sw->menu_texture.enable && sw->smooth)
       {
           clear_screen(sw);
           nwindowSetDimensions(sw->win,
                 VIDEO_SCALE_W(sw->hw_scale.dims),
                 VIDEO_SCALE_H(sw->hw_scale.dims));
       }
    }
    sw->menu_texture.enable = enable;
    sw->menu_texture.fullscreen = full_screen;
}

static uint32_t switch_get_flags(void *data)
{
   (void)data;
   return 0;
}

#ifdef HAVE_OVERLAY
static const video_overlay_interface_t switch_overlay = {
    switch_overlay_enable,
    switch_overlay_load,
    NULL, /* load_textures */
    switch_overlay_tex_geom,
    switch_overlay_vertex_geom,
    switch_overlay_full_screen,
    switch_overlay_set_alpha,
};

static void switch_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
    switch_video_t *swa = (switch_video_t *)data;
    if (!swa)
        return;
    *iface = &switch_overlay;
}

#endif

static bool switch_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
#ifdef SWITCH_NX_GPU
   return nxg_get_current_sw_framebuffer(data, fb);
#else
   (void)data; (void)fb;
   return false;
#endif
}

static void *switch_frame_mem_alloc(void *data, size_t size, uintptr_t *id,
      bool *cached, unsigned *pitch_align)
{
#ifdef SWITCH_NX_GPU
   return nxg_frame_mem_alloc(data, size, id, cached, pitch_align);
#else
   (void)data; (void)size; (void)id; (void)cached; (void)pitch_align;
   return NULL;
#endif
}

static bool switch_frame_mem_ready(void *data, uintptr_t id)
{
#ifdef SWITCH_NX_GPU
   return nxg_frame_mem_ready(data, id);
#else
   (void)data; (void)id;
   return true;
#endif
}

static void switch_frame_mem_free(void *data, uintptr_t id)
{
#ifdef SWITCH_NX_GPU
   nxg_frame_mem_free(data, id);
#else
   (void)data; (void)id;
#endif
}

static const video_poke_interface_t switch_poke_interface = {
   switch_get_flags,
   switch_load_texture,
   switch_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   switch_set_aspect_ratio,
   switch_apply_state_changes,
   switch_set_texture_frame,
   switch_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   switch_get_current_sw_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL, /* get_last_present_time */
   NULL, /* hw_ring_install */
   NULL, /* hw_ring_fence_new */
   NULL, /* hw_ring_fence_free */
   NULL, /* hw_ring_fence_signal */
   NULL, /* hw_ring_fence_wait */
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
   switch_frame_mem_alloc,
   switch_frame_mem_ready,
   switch_frame_mem_free
};

static void switch_get_poke_interface(void *data,
            const video_poke_interface_t **iface)
{
    (void)data;
    *iface = &switch_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool switch_gfx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

static font_renderer_t switch_font =
{
   switch_font_init,
   switch_font_free,
   switch_font_render_msg,
   "switch",
   switch_font_get_glyph,
   NULL, /* bind_block  */
   NULL, /* flush_block */
   switch_font_get_message_width,
   switch_font_get_line_metrics
};

video_driver_t video_switch = {
   switch_init,
   switch_frame,
   switch_set_nonblock_state,
   switch_alive,
   switch_focus,
   switch_suppress_screensaver,
   switch_has_windowed,
   switch_set_shader,
   switch_free,
   "switch",
   NULL, /* set_viewport */
   switch_set_rotation,
   switch_viewport_info,
   NULL, /* read_viewport  */
#ifdef HAVE_OVERLAY
   switch_get_overlay_interface,
#endif
   switch_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   switch_gfx_widgets_enabled,
#endif
   NULL, /* invalidate_hw_render_cache */
   NULL, /* read_viewport_hdr */
   &switch_font
};

gfx_display_ctx_driver_t gfx_display_ctx_switch = {
   gfx_display_switch_draw,
   gfx_display_switch_draw_pipeline,
   gfx_display_switch_blend_begin,
   gfx_display_switch_blend_end,
   gfx_display_switch_get_default_mvp,
   gfx_display_switch_get_default_vertices,
   gfx_display_switch_get_default_tex_coords,
   &switch_font,
   GFX_VIDEO_DRIVER_SWITCH,
   "switch",
   /* As the 2D engine draws: rotation and scale as values, a quad at a
    * time. With the 3D engine up, matrices and strips (switch_init). */
   true,  /* handles_transform */
   false, /* handles_vertex_strip */
   gfx_display_switch_scissor_begin,
   gfx_display_switch_scissor_end,
   NULL,  /* mesh_draw */
   NULL,  /* bloom_begin: no glow */
   NULL,  /* bloom_end */
   NULL,  /* bloom_composite */
#ifdef NXG_3D_FX
   gfx_display_switch_effect_install
#else
   NULL   /* effect_install */
#endif
};
