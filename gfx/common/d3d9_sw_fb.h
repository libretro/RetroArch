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

/* GET_CURRENT_SOFTWARE_FRAMEBUFFER for the D3D9 drivers, included by
 * d3d9_common.h so each driver has it in its own translation unit. */

#ifndef _D3D9_SW_FB_H
#define _D3D9_SW_FB_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>
#include <libretro.h>

#include "../video_driver.h"

#if defined(_WIN32) && !defined(_XBOX)
#include <malloc.h>
#endif

#define D3D9_SW_FB_PARKED 4

/* The frame texture lent to a core through
 * GET_CURRENT_SOFTWARE_FRAMEBUFFER, without the threaded wrapper (which
 * lends its own buffers): locked from the lend until the frame that
 * reads it out, so the core writes where the upload would have copied
 * to. A frame read out of a loan stays where the cached frame names
 * it, so a texture holding one is parked rather than released inside
 * a frame, and let go once the cached frame is retired. */
#define D3D9_FRAME_MEM_MAX 4

/* Frame memory for the threaded wrapper, on a Direct3D9Ex device: its
 * frame slots live in these blocks, and a frame inside one goes up with
 * UpdateSurface from a system-memory texture made over it - the GPU
 * reads it where the core or the wrapper wrote it, and the CPU copies
 * nothing. An event query says when the GPU is done with a block. */
typedef struct d3d9_frame_mem_blk
{
   uint8_t           *ptr;
   size_t             size;
   IDirect3DQuery9   *query;
   LPDIRECT3DTEXTURE9 user;        /* over the last frame sent from it */
   const uint8_t     *user_at;
   unsigned           user_pitch;
   unsigned           user_height;
   D3DFORMAT          user_fmt;
   bool               user_failed; /* that texture could not be made */
   bool               pending;     /* a GPU read may be in flight */
   bool               freeing;     /* given back while one was */
} d3d9_frame_mem_blk_t;

typedef struct d3d9_sw_fb
{
   LPDIRECT3DTEXTURE9 tex;         /* lent and locked */
   LPDIRECT3DTEXTURE9 frame_tex;   /* holds the last frame lent */
   LPDIRECT3DTEXTURE9 parked[D3D9_SW_FB_PARKED];
   const uint8_t     *frame;       /* where that frame was pushed from */
   uint8_t           *bits;
   unsigned           pitch;
   unsigned           dims;        /* the loan's size, packed */
   unsigned           tex_dims;
   d3d9_frame_mem_blk_t mem[D3D9_FRAME_MEM_MAX];
} d3d9_sw_fb_t;

static INLINE void d3d9_sw_fb_drop(d3d9_sw_fb_t *fb);
static INLINE void d3d9_sw_fb_collect(d3d9_sw_fb_t *fb);

static INLINE bool d3d9_sw_fb_lend(d3d9_sw_fb_t *fb, LPDIRECT3DTEXTURE9 tex,
      unsigned tex_dims, unsigned bpp, struct retro_framebuffer *out)
{
   D3DLOCKED_RECT lr;
   unsigned dims;

   if (!tex || !out || !out->width || !out->height
         || out->width  > VIDEO_SCALE_W(tex_dims)
         || out->height > VIDEO_SCALE_H(tex_dims))
      return false;
   dims = VIDEO_SCALE_PACK(out->width, out->height);

   /* Locked as large as asked: a managed texture's runtime uploads
    * what was locked, so a frame smaller than its texture sends only
    * itself */
   if (     fb->tex == tex
         && (     out->width  > VIDEO_SCALE_W(fb->dims)
               || out->height > VIDEO_SCALE_H(fb->dims)))
      d3d9_sw_fb_drop(fb);
   if (fb->tex != tex)
   {
      RECT r;
      d3d9_sw_fb_drop(fb);
      d3d9_sw_fb_collect(fb);
      /* Rewritten from here: the cached frame no longer lies in it */
      if (fb->frame_tex == tex)
      {
         fb->frame     = NULL;
         fb->frame_tex = NULL;
      }
      r.left   = 0;
      r.top    = 0;
      r.right  = (LONG)out->width;
      r.bottom = (LONG)out->height;
      lr.pBits = NULL;
      if (FAILED(d3d9_tex_lock(tex, 0, &lr, &r, D3DLOCK_NOSYSLOCK))
            || !lr.pBits)
         return false;
      fb->tex      = tex;
      fb->bits     = (uint8_t*)lr.pBits;
      fb->pitch    = (unsigned)lr.Pitch;
      fb->tex_dims = tex_dims;
      fb->dims     = dims;
   }

   out->data         = fb->bits;
   out->pitch        = fb->pitch;
   out->format       = (bpp == 4)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   out->memory_flags = RETRO_MEMORY_TYPE_CACHED;
   return true;
}

static INLINE void d3d9_sw_fb_drop(d3d9_sw_fb_t *fb)
{
   if (!fb || !fb->tex)
      return;
   /* What the core wrote goes up with the next upload into it, if any */
   d3d9_tex_unlock(fb->tex, 0, false);
   fb->tex = NULL;
}

static INLINE void d3d9_sw_fb_settle(d3d9_sw_fb_t *fb, const void *frame,
      unsigned width, unsigned height, unsigned pitch, unsigned bpp,
      unsigned last_dims)
{
   unsigned y;
   size_t row, tex_row, tex_h, span, lend_row, off;
   bool in_loan, move, clear;
   const uint8_t *src = (const uint8_t*)frame;
   uint8_t       *bits;

   if (!fb->tex)
      return;
   bits     = fb->bits;
   row      = (size_t)width * bpp;
   tex_row  = (size_t)VIDEO_SCALE_W(fb->tex_dims) * bpp;
   tex_h    = VIDEO_SCALE_H(fb->tex_dims);
   span     = (size_t)fb->pitch * tex_h;
   lend_row = (size_t)VIDEO_SCALE_W(fb->dims) * bpp;

   /* Not from the texture's memory: the upload copies it */
   if (     !src || !height || src < bits
         || pitch < row || row > tex_row || height > tex_h
         || (size_t)(src - bits) + (size_t)(height - 1) * pitch + row
            > span)
   {
      d3d9_sw_fb_drop(fb);
      return;
   }
   off     = (size_t)(src - bits);
   /* Inside what was locked: rows at the loan's pitch, in its size */
   in_loan = pitch == fb->pitch
      && off % fb->pitch + row <= lend_row
      && off / fb->pitch + height <= VIDEO_SCALE_H(fb->dims);
   move    = src != bits || pitch != fb->pitch;
   /* Past the frame is cleared as the upload clears it, when the size
    * changed or the core wrote there */
   clear   = move
      || last_dims != VIDEO_SCALE_PACK(width, height)
      || fb->dims  != VIDEO_SCALE_PACK(width, height);

   /* Anything past the loan is written under a lock of the whole
    * texture, which then goes up whole: a new size, a window, a frame
    * at another pitch - not the frame after frame of a steady core */
   if (!in_loan || clear)
   {
      D3DLOCKED_RECT lr;
      d3d9_tex_unlock(fb->tex, 0, false);
      lr.pBits = NULL;
      if (FAILED(d3d9_tex_lock(fb->tex, 0, &lr, NULL, D3DLOCK_NOSYSLOCK))
            || lr.pBits != (void*)bits)
      {
         if (lr.pBits)
            d3d9_tex_unlock(fb->tex, 0, false);
         fb->tex = NULL;
         return;
      }
   }

   if (move)
   {
      if (pitch >= fb->pitch)
      {
         /* Each row lands at or before where it is read */
         for (y = 0; y < height; y++)
            memmove(bits + (size_t)y * fb->pitch,
                  src + (size_t)y * pitch, row);
      }
      else
      {
         uint8_t *tmp = (uint8_t*)malloc(row * height);
         if (tmp)
         {
            for (y = 0; y < height; y++)
               memcpy(tmp + y * row, src + (size_t)y * pitch, row);
            for (y = 0; y < height; y++)
               memcpy(bits + (size_t)y * fb->pitch, tmp + y * row, row);
            free(tmp);
         }
      }
   }

   if (clear)
   {
      if (row < tex_row)
         for (y = 0; y < height; y++)
            memset(bits + (size_t)y * fb->pitch + row, 0, tex_row - row);
      for (y = height; y < tex_h; y++)
         memset(bits + (size_t)y * fb->pitch, 0, tex_row);
   }

   d3d9_tex_unlock(fb->tex, 0, true);
   fb->frame     = src;
   fb->frame_tex = fb->tex;
   fb->tex       = NULL;
}

/* @height rows of @width pixels, @pitch apart at @src, into the top
 * left of @tex, @tex_dims in size, which last held a frame of
 * @last_dims. Only the frame's rectangle is locked, so a managed
 * texture's runtime uploads only it; a new size locks the whole
 * texture and clears what is past the frame. */
static INLINE void d3d9_frame_upload(LPDIRECT3DTEXTURE9 tex,
      unsigned tex_dims, unsigned last_dims, const uint8_t *src,
      unsigned pitch, unsigned width, unsigned height, unsigned bpp)
{
   D3DLOCKED_RECT lr;
   RECT r;
   unsigned y;
   size_t row = (size_t)width * bpp;
   bool whole = last_dims != VIDEO_SCALE_PACK(width, height);

   if (!tex || !src || !width || !height)
      return;
   r.left   = 0;
   r.top    = 0;
   r.right  = (LONG)width;
   r.bottom = (LONG)height;
   lr.pBits = NULL;
   if (FAILED(d3d9_tex_lock(tex, 0, &lr, whole ? NULL : &r,
               D3DLOCK_NOSYSLOCK)) || !lr.pBits)
      return;
   if (whole)
      memset(lr.pBits, 0, (size_t)VIDEO_SCALE_H(tex_dims) * lr.Pitch);
   for (y = 0; y < height; y++)
      memcpy((uint8_t*)lr.pBits + (size_t)y * lr.Pitch,
            src + (size_t)y * pitch, row);
   d3d9_tex_unlock(tex, 0, true);
}

static INLINE const uint8_t *d3d9_sw_fb_src_begin(d3d9_sw_fb_t *fb, const void *frame,
      LPDIRECT3DTEXTURE9 dst, unsigned *pitch)
{
   D3DLOCKED_RECT lr;

   if (!fb || !fb->frame_tex || (const uint8_t*)frame != fb->frame)
      return (const uint8_t*)frame;
   if (fb->frame_tex == dst)
      return NULL;
   lr.pBits = NULL;
   if (FAILED(d3d9_tex_lock(fb->frame_tex, 0, &lr, NULL,
               D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)) || !lr.pBits)
      return (const uint8_t*)frame;
   *pitch = (unsigned)lr.Pitch;
   return (const uint8_t*)lr.pBits;
}

static INLINE void d3d9_sw_fb_src_end(d3d9_sw_fb_t *fb, const void *frame,
      LPDIRECT3DTEXTURE9 dst)
{
   if (!fb || !fb->frame_tex)
      return;
   if ((const uint8_t*)frame == fb->frame)
   {
      if (fb->frame_tex != dst)
      {
         d3d9_tex_unlock(fb->frame_tex, 0, false);
         /* The newest copy is the one replayed from next */
         fb->frame_tex = dst;
      }
   }
   else if (fb->frame_tex == dst)
   {
      /* Overwritten with another frame */
      fb->frame     = NULL;
      fb->frame_tex = NULL;
   }
}

static INLINE void d3d9_sw_fb_release(d3d9_sw_fb_t *fb, LPDIRECT3DTEXTURE9 tex)
{
   unsigned i;

   if (!tex)
      return;
   if (fb)
   {
      if (tex == fb->tex)
         d3d9_sw_fb_drop(fb);
      if (tex == fb->frame_tex)
      {
         for (i = 0; i < D3D9_SW_FB_PARKED; i++)
         {
            if (!fb->parked[i])
            {
               fb->parked[i] = tex;
               return;
            }
         }
         /* Nowhere to park it: nothing replays out of it from here */
         video_driver_cached_frame_invalidate();
         fb->frame     = NULL;
         fb->frame_tex = NULL;
      }
   }
   IDirect3DTexture9_Release(tex);
}

static INLINE void d3d9_sw_fb_collect(d3d9_sw_fb_t *fb)
{
   unsigned i;

   if (!fb || !fb->parked[0])
      return;
   video_driver_cached_frame_retire();
   for (i = 0; i < D3D9_SW_FB_PARKED && fb->parked[i]; i++)
   {
      if (fb->parked[i] == fb->frame_tex)
      {
         fb->frame     = NULL;
         fb->frame_tex = NULL;
      }
      IDirect3DTexture9_Release(fb->parked[i]);
      fb->parked[i] = NULL;
   }
}

#ifndef _XBOX
/* The device objects of block @b, and with @all its memory too */
static INLINE void d3d9_frame_mem_blk_release(d3d9_frame_mem_blk_t *b,
      bool all)
{
   if (b->user)
      IDirect3DTexture9_Release(b->user);
   if (b->query)
      IDirect3DQuery9_Release(b->query);
   b->user        = NULL;
   b->query       = NULL;
   b->user_at     = NULL;
   b->user_failed = false;
   b->pending     = false;
   if (all)
   {
#ifdef _WIN32
      if (b->ptr)
         _aligned_free(b->ptr);
#endif
      memset(b, 0, sizeof(*b));
   }
}

/* frame_mem_alloc: NULL where the device makes no texture over memory */
static INLINE void *d3d9_frame_mem_alloc(d3d9_sw_fb_t *fb,
      LPDIRECT3DDEVICE9 dev, size_t size, uintptr_t *id, bool *cached,
      unsigned *pitch_align)
{
#ifdef _WIN32
   unsigned i;
   if (!fb || !size || !d3d9_dev_is_ex(dev))
      return NULL;
   for (i = 0; i < D3D9_FRAME_MEM_MAX; i++)
   {
      if (fb->mem[i].ptr)
         continue;
      /* Page aligned, as the memory a texture is made over is */
      if (!(fb->mem[i].ptr = (uint8_t*)_aligned_malloc(size, 4096)))
         return NULL;
      fb->mem[i].size = size;
      *id             = (uintptr_t)i + 1;
      *cached         = true;
      *pitch_align    = 64;
      return fb->mem[i].ptr;
   }
#endif
   return NULL;
}

/* frame_mem_ready: never waits */
static INLINE bool d3d9_frame_mem_ready(d3d9_sw_fb_t *fb, uintptr_t id)
{
   d3d9_frame_mem_blk_t *b;
   HRESULT hr;
   if (!fb || !id || id > D3D9_FRAME_MEM_MAX)
      return true;
   b = &fb->mem[id - 1];
   if (!b->pending)
      return true;
   if (!b->query)
   {
      b->pending = false;
      return true;
   }
   /* FLUSH sends what is queued on, without waiting for it */
   hr = IDirect3DQuery9_GetData(b->query, NULL, 0, D3DGETDATA_FLUSH);
   if (hr == S_FALSE)
      return false;
   b->pending = false;
   return true;
}

/* Blocks given back while the GPU read them, let go once it is done */
static INLINE void d3d9_frame_mem_collect(d3d9_sw_fb_t *fb)
{
   unsigned i;
   for (i = 0; i < D3D9_FRAME_MEM_MAX; i++)
      if (fb->mem[i].freeing && d3d9_frame_mem_ready(fb, (uintptr_t)i + 1))
         d3d9_frame_mem_blk_release(&fb->mem[i], true);
}

/* frame_mem_free */
static INLINE void d3d9_frame_mem_free(d3d9_sw_fb_t *fb, uintptr_t id)
{
   if (!fb || !id || id > D3D9_FRAME_MEM_MAX || !fb->mem[id - 1].ptr)
      return;
   if (d3d9_frame_mem_ready(fb, id))
      d3d9_frame_mem_blk_release(&fb->mem[id - 1], true);
   else
      fb->mem[id - 1].freeing = true;
}

/* The device going: its objects let go. The memory stays: the
 * wrapper's, or one given back, let go by the next collect, the GPU
 * being done with it by then */
static INLINE void d3d9_frame_mem_device_lost(d3d9_sw_fb_t *fb)
{
   unsigned i;
   for (i = 0; i < D3D9_FRAME_MEM_MAX; i++)
      d3d9_frame_mem_blk_release(&fb->mem[i], false);
}

/* A frame inside a block, into the top left of @tex as
 * d3d9_frame_upload would put it, the GPU reading it where it lies;
 * false when it is not in one or cannot go that way, and the caller
 * copies it */
static INLINE bool d3d9_frame_mem_upload(d3d9_sw_fb_t *fb,
      LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 tex, unsigned tex_dims,
      unsigned last_dims, const uint8_t *src, unsigned pitch,
      unsigned width, unsigned height, unsigned bpp)
{
   unsigned i;
   d3d9_frame_mem_blk_t *b = NULL;
   D3DSURFACE_DESC desc;
   LPDIRECT3DSURFACE9 from = NULL;
   LPDIRECT3DSURFACE9 to   = NULL;
   RECT  rect;
   POINT at;
   HRESULT hr;

   if (!fb || !src || !width || !height || !bpp || pitch % bpp)
      return false;
   for (i = 0; i < D3D9_FRAME_MEM_MAX; i++)
   {
      const uint8_t *p = fb->mem[i].ptr;
      if (     p && src >= p
            && (size_t)(src - p) + (size_t)(height - 1) * pitch
               + (size_t)width * bpp <= fb->mem[i].size)
      {
         b = &fb->mem[i];
         break;
      }
   }
   if (!b || FAILED(IDirect3DTexture9_GetLevelDesc(tex, 0, &desc)))
      return false;

   /* The texture over the frame: kept while frames come from the same
    * place in the same shape, which they do frame after frame, and not
    * tried again for a shape it failed for */
   if (     b->user_at != src || b->user_pitch != pitch
         || b->user_height != height || b->user_fmt != desc.Format)
   {
      HANDLE over = (HANDLE)src;
      if (b->user)
         IDirect3DTexture9_Release(b->user);
      b->user        = NULL;
      b->user_at     = src;
      b->user_pitch  = pitch;
      b->user_height = height;
      b->user_fmt    = desc.Format;
      b->user_failed = FAILED(IDirect3DDevice9_CreateTexture(dev,
               pitch / bpp, height, 1, 0, desc.Format,
               D3DPOOL_SYSTEMMEM, &b->user, &over)) || !b->user;
   }
   if (b->user_failed || !b->user)
      return false;

   /* A new size clears what is past the frame first, as a copy does */
   if (last_dims != VIDEO_SCALE_PACK(width, height))
   {
      D3DLOCKED_RECT lr;
      lr.pBits = NULL;
      if (FAILED(d3d9_tex_lock(tex, 0, &lr, NULL, D3DLOCK_NOSYSLOCK))
            || !lr.pBits)
         return false;
      memset(lr.pBits, 0, (size_t)VIDEO_SCALE_H(tex_dims) * lr.Pitch);
      d3d9_tex_unlock(tex, 0, true);
   }

   rect.left   = 0;
   rect.top    = 0;
   rect.right  = (LONG)width;
   rect.bottom = (LONG)height;
   at.x        = 0;
   at.y        = 0;
   if (     FAILED(IDirect3DTexture9_GetSurfaceLevel(b->user, 0, &from))
         || FAILED(IDirect3DTexture9_GetSurfaceLevel(tex, 0, &to)))
   {
      if (from)
         IDirect3DSurface9_Release(from);
      return false;
   }
   hr = IDirect3DDevice9_UpdateSurface(dev, from, &rect, to, &at);
   IDirect3DSurface9_Release(from);
   IDirect3DSurface9_Release(to);
   if (FAILED(hr))
      return false;

   /* The block is the GPU's until this is through */
   if (!b->query && FAILED(IDirect3DDevice9_CreateQuery(dev,
               D3DQUERYTYPE_EVENT, &b->query)))
      b->query = NULL;
   if (b->query)
      IDirect3DQuery9_Issue(b->query, D3DISSUE_END);
   b->pending = true;
   return true;
}

#endif

/* A frame into @tex: from frame memory where it lies there, else
 * copied */
static INLINE void d3d9_frame_put(d3d9_sw_fb_t *fb, LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DTEXTURE9 tex, unsigned tex_dims, unsigned last_dims,
      const uint8_t *src, unsigned pitch, unsigned width, unsigned height,
      unsigned bpp)
{
#ifndef _XBOX
   if (d3d9_frame_mem_upload(fb, dev, tex, tex_dims, last_dims, src,
            pitch, width, height, bpp))
      return;
#endif
   d3d9_frame_upload(tex, tex_dims, last_dims, src, pitch, width,
         height, bpp);
}

#endif
