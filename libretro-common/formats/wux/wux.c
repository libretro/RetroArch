/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wux.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* No file I/O: every range of the file is asked of the caller (see
 * wux.h).
 *
 * The file, all little endian:
 *
 *    0   "WUX0"
 *    4   0x1099D02E
 *    8   sector size, 0x100 through 0x0FFFFFFF
 *    16  the WUD's size, 64-bit
 *    24  flags
 *    32  the index: per WUD sector, a 32-bit sector number in the array
 *        then the sector array, from the index's end rounded up to the
 *        sector size
 *
 * A sector the WUD holds more than once is kept once, so the index may
 * name a sector many times; a run of the WUD is one request only
 * where the array keeps its sectors in the same order. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/wux.h>

#define WUX_HEAD_SIZE   32
#define WUX_MAGIC1      0x1099D02Eu
#define WUX_SECTOR_MIN  0x100u
#define WUX_SECTOR_MAX  0x10000000u
/* index entries asked for at once */
#define WUX_WINDOW      256

#define WUX_STAGE_HEAD   0
#define WUX_STAGE_OPEN   1
#define WUX_STAGE_FAILED 2

#define WUX_TO_HEAD   0
#define WUX_TO_INDEX  1
#define WUX_TO_DATA   2

struct wux
{
   uint64_t file_size;
   uint64_t disc_size;
   uint64_t array_at;    /* the sector array, in the file */
   uint64_t rd_off;
   uint8_t *rd_dst;
   size_t   rd_len;
   size_t   rd_done;
   size_t   got;         /* of the header or the index window */
   size_t   fetch_len;
   uint32_t sector;
   uint32_t sectors;     /* the WUD's, as many as the index has */
   uint32_t win_first;   /* the first sector the window holds */
   uint32_t win_count;   /* how many it holds, 0 for none */
   uint32_t win_want;    /* how many are being read into it */
   int      stage;
   int      shift;       /* log2 of the sector size, -1 when not a power of two */
   int      to;
   uint8_t  win[WUX_WINDOW * 4];
   bool     fetching;
   bool     failed;
   bool     rd_armed;
};

static uint32_t wux_le32(const uint8_t *p)
{
   return  (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

int wux_is_wux(const uint8_t *head)
{
   return head
      && head[0] == 'W' && head[1] == 'U' && head[2] == 'X' && head[3] == '0'
      && wux_le32(head + 4) == WUX_MAGIC1;
}

wux_t *wux_new(uint64_t file_size)
{
   wux_t *w = (wux_t*)malloc(sizeof(wux_t));
   if (!w)
      return NULL;
   w->file_size = file_size;
   w->disc_size = 0;
   w->array_at  = 0;
   w->rd_off    = 0;
   w->rd_dst    = NULL;
   w->rd_len    = 0;
   w->rd_done   = 0;
   w->got       = 0;
   w->fetch_len = 0;
   w->sector    = 0;
   w->sectors   = 0;
   w->win_first = 0;
   w->win_count = 0;
   w->win_want  = 0;
   w->stage     = WUX_STAGE_HEAD;
   w->shift     = -1;
   w->to        = WUX_TO_HEAD;
   w->fetching  = false;
   w->failed    = false;
   w->rd_armed  = false;
   return w;
}

void wux_free(wux_t *w)
{
   free(w);
}

static int wux_ask(wux_t *w, wux_request_t *req, int to, uint64_t off,
      uint8_t *dst, size_t len)
{
   req->offset  = off;
   req->length  = len;
   req->dst     = dst;
   w->to        = to;
   w->fetch_len = len;
   w->fetching  = true;
   return WUX_PENDING;
}

/* The header is read into the index window, which nothing else holds
 * until the image is open. */
static int wux_parse(wux_t *w)
{
   const uint8_t *h = w->win;
   uint32_t       ss;
   uint64_t       n, at;

   if (!wux_is_wux(h))
      return WUX_ERROR_DATA;
   ss           = wux_le32(h + 8);
   w->disc_size = (uint64_t)wux_le32(h + 16)
                | ((uint64_t)wux_le32(h + 20) << 32);
   if (ss < WUX_SECTOR_MIN || ss >= WUX_SECTOR_MAX || !w->disc_size)
      return WUX_ERROR_DATA;
   n = w->disc_size / ss + (w->disc_size % ss ? 1 : 0);
   if (n > 0xFFFFFFFFu)
      return WUX_ERROR_DATA;
   at = WUX_HEAD_SIZE + n * 4;
   at = (at + ss - 1) / ss * ss;
   if (at > w->file_size)
      return WUX_ERROR_DATA;
   w->sector   = ss;
   w->sectors  = (uint32_t)n;
   w->array_at = at;
   w->shift    = -1;
   if (!(ss & (ss - 1)))
      for (w->shift = 0; (1u << w->shift) != ss; w->shift++) { }
   return WUX_OK;
}

int wux_open_step(wux_t *w, wux_request_t *req)
{
   int e;
   if (!w || !req)
      return WUX_ERROR_PARAM;
   if (w->stage == WUX_STAGE_OPEN)
      return WUX_OK;
   if (w->stage == WUX_STAGE_FAILED || w->failed)
   {
      w->stage = WUX_STAGE_FAILED;
      return WUX_ERROR_DATA;
   }
   if (w->got < WUX_HEAD_SIZE)
      return wux_ask(w, req, WUX_TO_HEAD, w->got, w->win + w->got,
            WUX_HEAD_SIZE - w->got);
   w->got   = 0;
   e        = wux_parse(w);
   w->stage = (e == WUX_OK) ? WUX_STAGE_OPEN : WUX_STAGE_FAILED;
   return e;
}

int wux_feed(wux_t *w, size_t len)
{
   if (!w || !w->fetching)
      return WUX_ERROR_STATE;
   w->fetching = false;
   if (!len)
   {
      w->failed = true;
      return WUX_ERROR_DATA;
   }
   if (len > w->fetch_len)
      len = w->fetch_len;
   if (w->to == WUX_TO_DATA)
      w->rd_done += len;
   else
      w->got     += len;
   return WUX_OK;
}

uint64_t wux_disc_size(const wux_t *w)
{
   return w ? w->disc_size : 0;
}

int wux_read_begin(wux_t *w, uint64_t off, void *dst, size_t len)
{
   if (!w || (!dst && len))
      return WUX_ERROR_PARAM;
   if (w->stage != WUX_STAGE_OPEN)
      return WUX_ERROR_STATE;
   if (off >= w->disc_size)
      len = 0;
   else if (len > w->disc_size - off)
      len = (size_t)(w->disc_size - off);
   /* a window left half read is not kept */
   if (w->fetching && w->to == WUX_TO_INDEX)
      w->win_count = 0;
   w->got      = 0;
   w->fetching = false;
   w->failed   = false;
   w->rd_dst   = (uint8_t*)dst;
   w->rd_off   = off;
   w->rd_len   = len;
   w->rd_done  = 0;
   w->rd_armed = true;
   return WUX_OK;
}

int wux_read_step(wux_t *w, wux_request_t *req)
{
   uint64_t ss;
   int      shift;
   if (!w || !req)
      return WUX_ERROR_PARAM;
   if (!w->rd_armed)
      return WUX_ERROR_STATE;
   if (w->failed)
   {
      w->rd_armed = false;
      return WUX_ERROR_DATA;
   }
   ss    = w->sector;
   shift = w->shift;
   while (w->rd_done < w->rd_len)
   {
      uint64_t off  = w->rd_off + w->rd_done;
      uint64_t left = w->rd_len - w->rd_done;
      uint64_t in, run;
      uint32_t s, e, stop, idx;

      /* 64-bit division is a library call on the 32-bit targets */
      if (shift >= 0)
      {
         s  = (uint32_t)(off >> shift);
         in = off & (ss - 1);
      }
      else
      {
         s  = (uint32_t)(off / ss);
         in = off - (uint64_t)s * ss;
      }

      /* the index window, read from @s on when it does not hold it */
      if (     !w->win_count
            || s < w->win_first
            || s - w->win_first >= w->win_count)
      {
         if (!w->got)
         {
            w->win_count = 0;
            w->win_first = s;
            w->win_want  = w->sectors - s;
            if (w->win_want > WUX_WINDOW)
               w->win_want = WUX_WINDOW;
         }
         if (w->got < (size_t)w->win_want * 4)
            return wux_ask(w, req, WUX_TO_INDEX,
                  WUX_HEAD_SIZE + (uint64_t)w->win_first * 4 + w->got,
                  w->win + w->got, (size_t)w->win_want * 4 - w->got);
         w->got       = 0;
         w->win_count = w->win_want;
      }

      /* as far as the array keeps the WUD's sectors in order, within
       * the read and the window */
      idx  = wux_le32(w->win + (size_t)(s - w->win_first) * 4);
      stop = w->win_first + w->win_count;
      e    = s + 1;
      run  = ss - in;
      while (run < left && e < stop
            && wux_le32(w->win + (size_t)(e - w->win_first) * 4)
               == idx + (e - s))
      {
         run += ss;
         e++;
      }
      if (run > left)
         run = left;

      return wux_ask(w, req, WUX_TO_DATA,
            w->array_at + (uint64_t)idx * ss + in,
            w->rd_dst + w->rd_done, (size_t)run);
   }
   w->rd_armed = false;
   return WUX_OK;
}

size_t wux_read_progress(const wux_t *w)
{
   return w ? w->rd_done : 0;
}
