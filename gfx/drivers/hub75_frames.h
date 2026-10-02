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

#ifndef __HUB75_FRAMES_H
#define __HUB75_FRAMES_H

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

/* Three canvases between the video thread, which draws a frame, and
 * the refresh thread, which scans the newest finished one onto the
 * panel over and over. Each side owns one canvas outright; the third
 * waits in the middle, and a single atomic word says which it is and
 * whether it is newer than what the refresh thread is scanning. Neither
 * side ever waits for the other: a frame is handed over with one
 * exchange, and a scan picks up the newest one with another. */

#define HUB75_FRAMES_FRESH 4

typedef struct hub75_frames
{
   void *buf[3];
   retro_atomic_int_t mid;   /* index of the waiting canvas | FRESH */
   int back;                 /* the video thread's */
   int front;                /* the refresh thread's */
} hub75_frames_t;

static INLINE void hub75_frames_init(hub75_frames_t *f,
      void *a, void *b, void *c)
{
   f->buf[0] = a;
   f->buf[1] = b;
   f->buf[2] = c;
   f->back   = 0;
   f->front  = 2;
   retro_atomic_store_release_int(&f->mid, 1);
}

/* The canvas the video thread draws the next frame into. */
static INLINE void *hub75_frames_back(const hub75_frames_t *f)
{
   return f->buf[f->back];
}

/* Video thread: the frame in the back canvas is finished. */
static INLINE void hub75_frames_publish(hub75_frames_t *f)
{
   int old = retro_atomic_exchange_int(&f->mid,
         f->back | HUB75_FRAMES_FRESH);
   f->back = old & 3;
}

/* Refresh thread, before each scan: the newest finished frame. Only
 * the video thread sets FRESH and only this clears it, so once it is
 * seen the exchange takes a canvas at least that new. */
static INLINE void *hub75_frames_front(hub75_frames_t *f)
{
   if (retro_atomic_load_acquire_int(&f->mid) & HUB75_FRAMES_FRESH)
      f->front = retro_atomic_exchange_int(&f->mid, f->front) & 3;
   return f->buf[f->front];
}

#endif
