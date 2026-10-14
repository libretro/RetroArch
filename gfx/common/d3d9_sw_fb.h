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

#define D3D9_SW_FB_PARKED 4

/* The frame texture lent to a core through
 * GET_CURRENT_SOFTWARE_FRAMEBUFFER, without the threaded wrapper (which
 * lends its own buffers): locked from the lend until the frame that
 * reads it out, so the core writes where the upload would have copied
 * to. A frame read out of a loan stays where the cached frame names
 * it, so a texture holding one is parked rather than released inside
 * a frame, and let go once the cached frame is retired. */
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
      if (FAILED(IDirect3DTexture9_LockRect(tex, 0, &lr, &r,
                  D3DLOCK_NOSYSLOCK)) || !lr.pBits)
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
   IDirect3DTexture9_UnlockRect(fb->tex, 0);
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
      IDirect3DTexture9_UnlockRect(fb->tex, 0);
      lr.pBits = NULL;
      if (FAILED(IDirect3DTexture9_LockRect(fb->tex, 0, &lr, NULL,
                  D3DLOCK_NOSYSLOCK)) || lr.pBits != (void*)bits)
      {
         if (lr.pBits)
            IDirect3DTexture9_UnlockRect(fb->tex, 0);
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

   IDirect3DTexture9_UnlockRect(fb->tex, 0);
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
   if (FAILED(IDirect3DTexture9_LockRect(tex, 0, &lr, whole ? NULL : &r,
               D3DLOCK_NOSYSLOCK)) || !lr.pBits)
      return;
   if (whole)
      memset(lr.pBits, 0, (size_t)VIDEO_SCALE_H(tex_dims) * lr.Pitch);
   for (y = 0; y < height; y++)
      memcpy((uint8_t*)lr.pBits + (size_t)y * lr.Pitch,
            src + (size_t)y * pitch, row);
   IDirect3DTexture9_UnlockRect(tex, 0);
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
   if (FAILED(IDirect3DTexture9_LockRect(fb->frame_tex, 0, &lr, NULL,
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
         IDirect3DTexture9_UnlockRect(fb->frame_tex, 0);
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

#endif
