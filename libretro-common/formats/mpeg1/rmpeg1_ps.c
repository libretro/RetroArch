/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg1_ps.c).
 * ---------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* MPEG-1 Systems (Program Stream) demultiplexer.
 *
 * Structure, from ISO/IEC 11172-1:
 *
 *   ISO_11172_stream : pack+ ISO_11172_end_code
 *   pack             : pack_start_code SCR mux_rate [system_header] packet*
 *   packet           : packet_start_code_prefix stream_id packet_length
 *                      [stuffing] [STD buffer] [PTS [DTS]] data
 *
 * Everything is byte-aligned at the start-code level, which is what makes a
 * streaming parser practical: we only ever need to find the next 000001h
 * prefix and dispatch on the byte after it.
 *
 * Layout of the pieces we decode:
 *
 *   pack header (12 bytes total, after the 4-byte start code)
 *     4 bits  '0010'
 *     3 bits  SCR [32..30]        1 bit marker
 *    15 bits  SCR [29..15]        1 bit marker
 *    15 bits  SCR [14..0]         1 bit marker
 *     1 bit   marker
 *    22 bits  mux_rate            1 bit marker
 *
 *   packet header, for every stream_id except padding (BEh) and
 *   private_stream_2 (BFh):
 *     0..16 stuffing bytes, each FFh
 *     if the next 2 bits are '01': 2 bytes of STD buffer scale/size
 *     then exactly one of:
 *       '0010' + 36 bits ->  5 bytes, PTS only
 *       '0011' + 76 bits -> 10 bytes, PTS and DTS
 *       0Fh              ->  1 byte, neither
 *
 * The timestamps are 33 bits at 90 kHz, split across the fields by single
 * marker bits so that no start-code prefix can be forged inside them.
 *
 * Style: C89, no declarations after statements, no // comments.
 */

#include <stdlib.h>
#include <string.h>

#include <formats/rmpeg1_ps.h>

#define RMPEG1_PS_DEFAULT_CAPACITY (256 * 1024)

/* 6-byte packet header plus the largest 16-bit packet_length. */
#define RMPEG1_PS_MAX_PACKET       (6 + 0xFFFF)

#define RMPEG1_PS_PACK_START       0xBA
#define RMPEG1_PS_SYSTEM_HEADER    0xBB
#define RMPEG1_PS_END_CODE         0xB9
#define RMPEG1_PS_PRIVATE_1_ID     0xBD
#define RMPEG1_PS_PADDING_ID       0xBE
#define RMPEG1_PS_PRIVATE_2_ID     0xBF

struct rmpeg1_ps
{
   uint8_t  *buf;
   size_t    capacity;
   size_t    rd;          /* parse cursor        */
   size_t    wr;          /* append cursor       */

   uint64_t  scr;
   uint32_t  mux_rate;
   uint32_t  resyncs;

   bool      ended;
   bool      synced;      /* a start code has been located at least once */
};

/* --------------------------------------------------------------------- */
/* Buffer management                                                     */
/* --------------------------------------------------------------------- */

static void rmpeg1_ps_compact(rmpeg1_ps_t *ps)
{
   if (ps->rd == 0)
      return;

   if (ps->wr > ps->rd)
      memmove(ps->buf, ps->buf + ps->rd, ps->wr - ps->rd);

   ps->wr -= ps->rd;
   ps->rd  = 0;
}

rmpeg1_ps_t *rmpeg1_ps_init(size_t capacity)
{
   rmpeg1_ps_t *ps;

   if (capacity == 0)
      capacity = RMPEG1_PS_DEFAULT_CAPACITY;

   /* A caller-chosen capacity that cannot hold one maximum-size packet would
    * deadlock: next() would always want more data and write() would always be
    * full. Refuse rather than misbehave later. */
   if (capacity < RMPEG1_PS_MAX_PACKET)
      return NULL;

   ps = (rmpeg1_ps_t *)calloc(1, sizeof(*ps));
   if (!ps)
      return NULL;

   ps->buf = (uint8_t *)malloc(capacity);
   if (!ps->buf)
   {
      free(ps);
      return NULL;
   }

   ps->capacity = capacity;
   ps->scr      = RMPEG1_PS_NO_PTS;
   return ps;
}

void rmpeg1_ps_free(rmpeg1_ps_t *ps)
{
   if (!ps)
      return;
   free(ps->buf);
   free(ps);
}

void rmpeg1_ps_reset(rmpeg1_ps_t *ps)
{
   if (!ps)
      return;
   ps->rd       = 0;
   ps->wr       = 0;
   ps->scr      = RMPEG1_PS_NO_PTS;
   ps->mux_rate = 0;
   ps->ended    = false;
   ps->synced   = false;
   /* resyncs is a cumulative health counter and deliberately survives. */
}

size_t rmpeg1_ps_space(const rmpeg1_ps_t *ps)
{
   if (!ps)
      return 0;
   /* rd bytes at the front are reclaimable by compaction. */
   return ps->capacity - (ps->wr - ps->rd);
}

size_t rmpeg1_ps_write(rmpeg1_ps_t *ps, const uint8_t *data, size_t len)
{
   size_t room;

   if (!ps || !data || len == 0)
      return 0;

   if (ps->capacity - ps->wr < len)
      rmpeg1_ps_compact(ps);

   room = ps->capacity - ps->wr;
   if (len > room)
      len = room;

   if (len)
   {
      memcpy(ps->buf + ps->wr, data, len);
      ps->wr += len;
   }

   return len;
}

/* --------------------------------------------------------------------- */
/* Field decoding                                                        */
/* --------------------------------------------------------------------- */

/* 33-bit timestamp spread over 5 bytes with marker bits at 0, 23 and 39.
 * The caller has already checked that 5 bytes are present. */
static uint64_t rmpeg1_ps_read_ts(const uint8_t *p)
{
   uint64_t ts;

   ts  = (uint64_t)(p[0] & 0x0E) << 29;
   ts |= (uint64_t)(p[1])        << 22;
   ts |= (uint64_t)(p[2] & 0xFE) << 14;
   ts |= (uint64_t)(p[3])        <<  7;
   ts |= (uint64_t)(p[4] & 0xFE) >>  1;

   return ts;
}

/* Scan for the next 000001h prefix at or after ps->rd. Returns the offset of
 * the prefix, or (size_t)-1 when none is present in the buffered data.
 *
 * Leaves the trailing two bytes unconsumed on failure: a prefix may straddle
 * the end of what we have been given so far. */
static size_t rmpeg1_ps_find_start(rmpeg1_ps_t *ps)
{
   size_t i;
   size_t end;

   if (ps->wr < ps->rd + 3)
      return (size_t)-1;

   end = ps->wr - 2;

   for (i = ps->rd; i < end; i++)
   {
      if (     ps->buf[i    ] == 0x00
            && ps->buf[i + 1] == 0x00
            && ps->buf[i + 2] == 0x01)
         return i;
   }

   /* Nothing found. Everything before the last two bytes is garbage and can
    * be dropped so the buffer does not grow without bound on a broken
    * stream. */
   ps->rd = end;
   return (size_t)-1;
}

static uint8_t rmpeg1_ps_classify(uint8_t stream_id, uint8_t *index)
{
   if (stream_id >= 0xE0 && stream_id <= 0xEF)
   {
      *index = (uint8_t)(stream_id & 0x0F);
      return RMPEG1_PS_VIDEO;
   }
   if (stream_id >= 0xC0 && stream_id <= 0xDF)
   {
      *index = (uint8_t)(stream_id & 0x1F);
      return RMPEG1_PS_AUDIO;
   }

   *index = 0;

   switch (stream_id)
   {
      case RMPEG1_PS_PRIVATE_1_ID:
         return RMPEG1_PS_PRIVATE_1;
      case RMPEG1_PS_PRIVATE_2_ID:
         return RMPEG1_PS_PRIVATE_2;
      case RMPEG1_PS_PADDING_ID:
         return RMPEG1_PS_PADDING;
      default:
         break;
   }

   return RMPEG1_PS_NONE;
}

/* --------------------------------------------------------------------- */
/* Parser                                                                */
/* --------------------------------------------------------------------- */

int rmpeg1_ps_next(rmpeg1_ps_t *ps, rmpeg1_ps_packet_t *out)
{
   if (!ps || !out)
      return 0;

   for (;;)
   {
      size_t   sc;
      size_t   avail;
      uint8_t  stream_id;
      size_t   packet_len;
      size_t   pos;
      size_t   payload_end;
      uint64_t pts = RMPEG1_PS_NO_PTS;
      uint64_t dts = RMPEG1_PS_NO_PTS;
      uint8_t  type;
      uint8_t  index;
      int      stuffing;

      if (ps->ended)
         return 0;

      sc = rmpeg1_ps_find_start(ps);
      if (sc == (size_t)-1)
         return 0;

      /* A gap between the parse cursor and the start code is not necessarily
       * damage. Muxers pad a pack out to the sector size with zero bytes --
       * ffmpeg's VCD muxer does this on every audio pack -- and that fill is
       * legal and expected. Only count a resync when the skipped bytes are
       * something other than zero fill, so the counter stays meaningful as a
       * stream-health signal rather than firing once per sector. */
      if (sc != ps->rd)
      {
         size_t g;
         bool   fill = true;

         for (g = ps->rd; g < sc; g++)
         {
            if (ps->buf[g] != 0x00)
            {
               fill = false;
               break;
            }
         }

         if (!fill || !ps->synced)
            ps->resyncs++;

         ps->rd = sc;
      }

      ps->synced = true;

      avail = ps->wr - ps->rd;
      if (avail < 4)
         return 0;

      stream_id = ps->buf[ps->rd + 3];

      /* --- end code ------------------------------------------------- */
      if (stream_id == RMPEG1_PS_END_CODE)
      {
         ps->rd   += 4;
         ps->ended = true;
         return 0;
      }

      /* --- pack header ---------------------------------------------- */
      if (stream_id == RMPEG1_PS_PACK_START)
      {
         const uint8_t *p;

         if (avail < 12)
            return 0;

         p = ps->buf + ps->rd + 4;

         /* MPEG-1 packs carry '0010' here. MPEG-2 uses '01', and its pack
          * header is 14 bytes with a different SCR layout; we do not claim
          * to parse those, so resync past it rather than misread it. */
         if ((p[0] & 0xF0) != 0x20)
         {
            ps->rd += 4;
            ps->resyncs++;
            continue;
         }

         ps->scr = rmpeg1_ps_read_ts(p);

         ps->mux_rate = ((uint32_t)(p[5] & 0x7F) << 15)
                      | ((uint32_t)p[6]          <<  7)
                      | ((uint32_t)p[7]          >>  1);

         ps->rd += 12;
         continue;
      }

      /* --- everything else is length-prefixed ------------------------ */
      if (avail < 6)
         return 0;

      packet_len = ((size_t)ps->buf[ps->rd + 4] << 8) | ps->buf[ps->rd + 5];

      if (avail < 6 + packet_len)
      {
         /* Incomplete. If it can never fit, the length is corrupt: step past
          * this start code and resync rather than stalling forever. */
         if (6 + packet_len > ps->capacity)
         {
            ps->rd += 4;
            ps->resyncs++;
            continue;
         }
         return 0;
      }

      pos         = ps->rd + 6;
      payload_end = pos + packet_len;

      /* System headers describe the stream set; they carry no elementary
       * data, so record nothing and move on. */
      if (stream_id == RMPEG1_PS_SYSTEM_HEADER)
      {
         ps->rd = payload_end;
         continue;
      }

      type = rmpeg1_ps_classify(stream_id, &index);

      if (type == RMPEG1_PS_PADDING)
      {
         ps->rd = payload_end;
         continue;
      }

      /* Reserved and system stream_ids (B0h..BCh and anything else we do not
       * recognise) are skipped whole. */
      if (type == RMPEG1_PS_NONE)
      {
         ps->rd = payload_end;
         continue;
      }

      /* private_stream_2 has no header extension at all: payload starts
       * immediately. Everything else gets the stuffing/STD/timestamp
       * preamble. */
      if (type != RMPEG1_PS_PRIVATE_2)
      {
         /* Up to 16 stuffing bytes. The spec caps it; enforcing the cap
          * stops a run of FFh in corrupt data from eating the packet. */
         stuffing = 0;
         while (pos < payload_end && ps->buf[pos] == 0xFF && stuffing < 16)
         {
            pos++;
            stuffing++;
         }

         if (pos + 1 < payload_end && (ps->buf[pos] & 0xC0) == 0x40)
            pos += 2;               /* STD buffer scale and size */

         if (pos < payload_end)
         {
            uint8_t flag = (uint8_t)(ps->buf[pos] & 0xF0);

            if (flag == 0x20)
            {
               if (pos + 5 > payload_end)
               {
                  ps->rd = payload_end;
                  ps->resyncs++;
                  continue;
               }
               pts  = rmpeg1_ps_read_ts(ps->buf + pos);
               pos += 5;
            }
            else if (flag == 0x30)
            {
               if (pos + 10 > payload_end)
               {
                  ps->rd = payload_end;
                  ps->resyncs++;
                  continue;
               }
               pts  = rmpeg1_ps_read_ts(ps->buf + pos);
               dts  = rmpeg1_ps_read_ts(ps->buf + pos + 5);
               pos += 10;
            }
            else if (ps->buf[pos] == 0x0F)
               pos += 1;
            else
            {
               /* None of the three legal encodings. The header is malformed;
                * drop the packet rather than emit whatever follows as if it
                * were elementary data. */
               ps->rd = payload_end;
               ps->resyncs++;
               continue;
            }
         }
      }

      ps->rd = payload_end;

      /* A packet whose header consumed everything is legal and carries no
       * data. Nothing downstream wants a zero-length buffer, so skip it. */
      if (pos >= payload_end)
         continue;

      out->data      = ps->buf + pos;
      out->size      = payload_end - pos;
      out->pts       = pts;
      out->dts       = dts;
      out->stream_id = stream_id;
      out->type      = type;
      out->index     = index;

      return 1;
   }
}

/* --------------------------------------------------------------------- */
/* Accessors                                                             */
/* --------------------------------------------------------------------- */

uint64_t rmpeg1_ps_scr(const rmpeg1_ps_t *ps)
{
   return ps ? ps->scr : RMPEG1_PS_NO_PTS;
}

uint32_t rmpeg1_ps_mux_rate(const rmpeg1_ps_t *ps)
{
   return ps ? ps->mux_rate : 0;
}

bool rmpeg1_ps_ended(const rmpeg1_ps_t *ps)
{
   return ps ? ps->ended : false;
}

uint32_t rmpeg1_ps_resyncs(const rmpeg1_ps_t *ps)
{
   return ps ? ps->resyncs : 0;
}
