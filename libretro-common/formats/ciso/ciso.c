/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ciso.c).
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
 * ciso.h).
 *
 * The file is a 32 KiB head - "CISO", the block size (little endian)
 * and one octet per block, 1 for stored and 0 for left out - then the
 * stored blocks in disc order. Consecutive stored blocks are
 * consecutive in the file, so a read asks for one range per run.
 *
 * The head is read into the top of the block map's own storage and
 * the map built over it from the bottom: entry i is written at octets
 * 2i and 2i+1, never above where the head's octet for block i sat,
 * which has been read by then, so no other buffer is needed. */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/ciso.h>

#define CISO_HEAD_SIZE   0x8000
#define CISO_MAP_SIZE    (CISO_HEAD_SIZE - 8)
#define CISO_SECTOR      0x8000
#define CISO_UNUSED      0xFFFF
/* where the head lands in the map's storage */
#define CISO_HEAD_AT     (CISO_MAP_SIZE * 2 - CISO_HEAD_SIZE)

#define CISO_STAGE_HEAD   0
#define CISO_STAGE_OPEN   1
#define CISO_STAGE_FAILED 2

struct ciso
{
   uint64_t disc_size;
   uint64_t rd_off;
   uint8_t *rd_dst;
   size_t   rd_len;
   size_t   rd_done;
   size_t   head_got;
   size_t   fetch_len;
   uint32_t block_size;
   uint32_t n_blocks;    /* through the last stored block */
   int      stage;
   int      shift;       /* log2 of the block size, -1 when not a power of two */
   uint16_t map[CISO_MAP_SIZE]; /* a block's place in the file */
   bool     fetching;
   bool     failed;
   bool     rd_armed;
};

ciso_t *ciso_new(void)
{
   ciso_t *c = (ciso_t*)malloc(sizeof(ciso_t));
   if (!c)
      return NULL;
   c->disc_size  = 0;
   c->rd_off     = 0;
   c->rd_dst     = NULL;
   c->rd_len     = 0;
   c->rd_done    = 0;
   c->head_got   = 0;
   c->fetch_len  = 0;
   c->block_size = 0;
   c->n_blocks   = 0;
   c->stage      = CISO_STAGE_HEAD;
   c->shift      = -1;
   c->fetching   = false;
   c->failed     = false;
   c->rd_armed   = false;
   return c;
}

void ciso_free(ciso_t *c)
{
   free(c);
}

static int ciso_parse(ciso_t *c)
{
   const uint8_t *h = (const uint8_t*)c->map + CISO_HEAD_AT;
   uint16_t *map    = c->map;
   uint32_t i;
   uint32_t n       = 0;
   uint32_t last    = 0;
   uint32_t bs      = (uint32_t)h[4]
                    | ((uint32_t)h[5] << 8)
                    | ((uint32_t)h[6] << 16)
                    | ((uint32_t)h[7] << 24);

   if (     h[0] != 'C' || h[1] != 'I' || h[2] != 'S' || h[3] != 'O'
         || !bs || (bs % CISO_SECTOR))
      return CISO_ERROR_DATA;

   h += 8;
   for (i = 0; i < CISO_MAP_SIZE; i++)
   {
      uint8_t v = h[i];
      if (v == 1)
      {
         map[i] = (uint16_t)n++;
         last   = i;
      }
      else if (!v)
         map[i] = CISO_UNUSED;
      else
         return CISO_ERROR_DATA;
   }
   if (!n)
      return CISO_ERROR_DATA;

   c->block_size = bs;
   c->shift      = -1;
   if (!(bs & (bs - 1)))
      for (c->shift = 0; (1u << c->shift) != bs; c->shift++) { }
   c->n_blocks   = last + 1;
   c->disc_size  = (uint64_t)c->n_blocks * bs;
   return CISO_OK;
}

int ciso_open_step(ciso_t *c, ciso_request_t *req)
{
   int e;
   if (!c || !req)
      return CISO_ERROR_PARAM;
   if (c->stage == CISO_STAGE_OPEN)
      return CISO_OK;
   if (c->stage == CISO_STAGE_FAILED || c->failed)
   {
      c->stage = CISO_STAGE_FAILED;
      return CISO_ERROR_DATA;
   }
   if (c->head_got < CISO_HEAD_SIZE)
   {
      req->offset  = c->head_got;
      req->length  = CISO_HEAD_SIZE - c->head_got;
      req->dst     = (uint8_t*)c->map + CISO_HEAD_AT + c->head_got;
      c->fetch_len = req->length;
      c->fetching  = true;
      return CISO_PENDING;
   }
   e        = ciso_parse(c);
   c->stage = (e == CISO_OK) ? CISO_STAGE_OPEN : CISO_STAGE_FAILED;
   return e;
}

int ciso_feed(ciso_t *c, size_t len)
{
   if (!c || !c->fetching)
      return CISO_ERROR_STATE;
   c->fetching = false;
   if (!len)
   {
      c->failed = true;
      return CISO_ERROR_DATA;
   }
   if (len > c->fetch_len)
      len = c->fetch_len;
   if (c->stage == CISO_STAGE_OPEN)
      c->rd_done  += len;
   else
      c->head_got += len;
   return CISO_OK;
}

uint32_t ciso_block_size(const ciso_t *c)
{
   return c ? c->block_size : 0;
}

uint64_t ciso_disc_size(const ciso_t *c)
{
   return c ? c->disc_size : 0;
}

int ciso_read_begin(ciso_t *c, uint64_t off, void *dst, size_t len)
{
   if (!c || (!dst && len))
      return CISO_ERROR_PARAM;
   if (c->stage != CISO_STAGE_OPEN)
      return CISO_ERROR_STATE;
   if (off >= c->disc_size)
      len = 0;
   else if (len > c->disc_size - off)
      len = (size_t)(c->disc_size - off);
   c->fetching = false;
   c->failed   = false;
   c->rd_dst   = (uint8_t*)dst;
   c->rd_off   = off;
   c->rd_len   = len;
   c->rd_done  = 0;
   c->rd_armed = true;
   return CISO_OK;
}

int ciso_read_step(ciso_t *c, ciso_request_t *req)
{
   uint64_t bs;
   int      shift;
   if (!c || !req)
      return CISO_ERROR_PARAM;
   if (!c->rd_armed)
      return CISO_ERROR_STATE;
   if (c->failed)
   {
      c->rd_armed = false;
      return CISO_ERROR_DATA;
   }
   bs    = c->block_size;
   shift = c->shift;
   while (c->rd_done < c->rd_len)
   {
      uint64_t off  = c->rd_off + c->rd_done;
      uint64_t left = c->rd_len - c->rd_done;
      uint64_t in, stop, run;
      uint32_t blk, end;
      uint16_t idx;

      /* 64-bit division is a library call on the 32-bit targets */
      if (shift >= 0)
      {
         blk  = (uint32_t)(off >> shift);
         in   = off & (bs - 1);
         stop = blk + ((in + left + bs - 1) >> shift);
      }
      else
      {
         blk  = (uint32_t)(off / bs);
         in   = off - (uint64_t)blk * bs;
         stop = blk + (in + left + bs - 1) / bs;
      }
      if (stop > c->n_blocks)
         stop = c->n_blocks;
      idx = c->map[blk];
      end = blk + 1;
      if (idx == CISO_UNUSED)
         while (end < stop && c->map[end] == CISO_UNUSED)
            end++;
      else
         while (end < stop && c->map[end] != CISO_UNUSED)
            end++;
      run = (uint64_t)end * bs - off;
      if (run > left)
         run = left;

      if (idx == CISO_UNUSED)
      {
         memset(c->rd_dst + c->rd_done, 0, (size_t)run);
         c->rd_done += (size_t)run;
         continue;
      }

      req->offset  = CISO_HEAD_SIZE + (uint64_t)idx * bs + in;
      req->length  = (size_t)run;
      req->dst     = c->rd_dst + c->rd_done;
      c->fetch_len = req->length;
      c->fetching  = true;
      return CISO_PENDING;
   }
   c->rd_armed = false;
   return CISO_OK;
}

size_t ciso_read_progress(const ciso_t *c)
{
   return c ? c->rd_done : 0;
}
