/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <formats/rflv_mux.h>
#include <stdlib.h>
#include <string.h>

enum flv_output
{
   FLV_HEADER, FLV_VIDEO_CONFIG, FLV_AUDIO_CONFIG, FLV_MEDIA, FLV_IDLE
};

struct rflv_mux
{
   uint64_t last[2], input_dts;
   rflv_span_t spans[3];
   uint8_t *config;
   const uint8_t *input;
   size_t avcc_size, asc_size, limit, size, offset, span_count;
   uint32_t timestamp;
   int32_t composition;
   unsigned timescale[2], track;
   enum flv_output output;
   uint8_t prefix[16], suffix[4];
   bool pending, scanning, failed, finished, seen[2], keyframe, vcl, idr;
};

static void flv_uint(uint8_t *out, uint32_t value, unsigned count)
{
   unsigned i;
   for (i = 0; i < count; i++)
      out[count - i - 1] = (uint8_t)(value >> (i * 8));
}

static bool flv_sets(const uint8_t *data, size_t size, size_t *pos,
      unsigned count, unsigned type)
{
   unsigned i, length;
   for (i = 0; i < count; i++)
   {
      if (size - *pos < 2)
         return false;
      length = ((unsigned)data[*pos] << 8) | data[*pos + 1];
      *pos += 2;
      if (!length || length > size - *pos || (data[*pos] & 0x80)
            || (data[*pos] & 31) != type)
         return false;
      *pos += length;
   }
   return true;
}

static bool flv_avcc(const uint8_t *data, size_t size)
{
   size_t pos = 6;
   unsigned count;
   if (!data || size < 7 || size > 65536 || data[0] != 1
         || data[4] != 0xff || (data[5] & 0xe0) != 0xe0 || !(data[5] & 31))
      return false;
   if (!flv_sets(data, size, &pos, data[5] & 31, 7) || pos == size)
      return false;
   count = data[pos++];
   if (!count || !flv_sets(data, size, &pos, count, 8))
      return false;
   if (pos == size)
      return true;
   if (size - pos < 4 || (data[pos] & 0xfc) != 0xfc
         || (data[pos + 1] & 0xf8) != 0xf8 || (data[pos + 2] & 0xf8) != 0xf8)
      return false;
   pos += 3;
   count = data[pos++];
   return flv_sets(data, size, &pos, count, 13) && pos == size;
}

static unsigned flv_bits(const uint8_t *data, size_t size, unsigned *pos, unsigned count)
{
   unsigned i, value = 0;
   if (*pos > size * 8 || count > size * 8 - *pos)
      return ~0u;
   for (i = 0; i < count; i++, (*pos)++)
      value = (value << 1) | ((data[*pos / 8] >> (7 - *pos % 8)) & 1);
   return value;
}

static bool flv_asc(const uint8_t *data, size_t size)
{
   unsigned pos = 0, index, channels;
   if (!data || size < 2 || size > 64 || flv_bits(data, size, &pos, 5) != 2)
      return false;
   index = flv_bits(data, size, &pos, 4);
   if (index == 13 || index == 14)
      return false;
   if (index == 15)
   {
      unsigned rate = flv_bits(data, size, &pos, 24);
      if (!rate || rate == ~0u)
         return false;
   }
   channels = flv_bits(data, size, &pos, 4);
   if (!channels || channels > 7 || flv_bits(data, size, &pos, 1) == ~0u)
      return false;
   return flv_bits(data, size, &pos, 2) == 0;
}

static void flv_tag(rflv_mux_t *m, unsigned track, const void *data,
      size_t size, uint32_t timestamp, int32_t composition, bool keyframe, bool config)
{
   unsigned bytes = track == 1 ? 5 : 2;
   memset(m->prefix, 0, sizeof(m->prefix));
   m->prefix[0] = track == 1 ? 9 : 8;
   flv_uint(m->prefix + 1, (uint32_t)size + bytes, 3);
   flv_uint(m->prefix + 4, timestamp, 3);
   m->prefix[7] = (uint8_t)(timestamp >> 24);
   m->prefix[11] = track == 1 ? (keyframe ? 0x17 : 0x27) : 0xaf;
   m->prefix[12] = config ? 0 : 1;
   if (track == 1)
      flv_uint(m->prefix + 13, (uint32_t)composition, 3);
   flv_uint(m->suffix, (uint32_t)size + bytes + 11, 4);
   m->spans[0].data = m->prefix;
   m->spans[0].size = 11 + bytes;
   m->spans[1].data = data;
   m->spans[1].size = size;
   m->spans[2].data = m->suffix;
   m->spans[2].size = 4;
   m->span_count = 3;
   m->pending = true;
}

rflv_mux_t *rflv_mux_new(const rflv_config_t *c)
{
   rflv_mux_t *m;
   if (!c || (!c->avcc && !c->asc) || !c->max_packet_size || c->max_packet_size > 0xfffffa
         || (c->avcc ? !c->video_timescale || !flv_avcc((const uint8_t*)c->avcc, c->avcc_size)
            : c->avcc_size || c->video_timescale)
         || (c->asc ? !c->audio_timescale || !flv_asc((const uint8_t*)c->asc, c->asc_size)
            : c->asc_size || c->audio_timescale))
      return NULL;
   m = (rflv_mux_t*)calloc(1, sizeof(*m));
   if (!m)
      return NULL;
   m->config = (uint8_t*)malloc(c->avcc_size + c->asc_size);
   if (!m->config)
   {
      free(m);
      return NULL;
   }
   if (c->avcc_size)
      memcpy(m->config, c->avcc, c->avcc_size);
   if (c->asc_size)
      memcpy(m->config + c->avcc_size, c->asc, c->asc_size);
   m->avcc_size = c->avcc_size;
   m->asc_size = c->asc_size;
   m->timescale[0] = c->video_timescale;
   m->timescale[1] = c->audio_timescale;
   m->limit = c->max_packet_size;
   memcpy(m->prefix, "FLV\1\0\0\0\0\11\0\0\0\0", 13);
   m->prefix[4] = (c->avcc ? 1 : 0) | (c->asc ? 4 : 0);
   m->spans[0].data = m->prefix;
   m->spans[0].size = 13;
   m->span_count = 1;
   m->pending = true;
   return m;
}

static bool flv_milliseconds(uint64_t ticks, unsigned scale, uint32_t *result)
{
   uint64_t seconds = ticks / scale;
   uint64_t ms;
   if (seconds > 0x7fffffff / 1000)
      return false;
   ms = seconds * 1000 + (ticks % scale) * 1000 / scale;
   if (ms > 0x7fffffff)
      return false;
   *result = (uint32_t)ms;
   return true;
}

int rflv_mux_push(rflv_mux_t *m, unsigned track, const void *data,
      size_t size, uint64_t dts, uint64_t pts, bool keyframe)
{
   uint32_t dts_ms, pts_ms;
   int64_t composition;
   if (!m || m->failed || m->finished)
      return RFLV_ERROR;
   if (m->pending || m->scanning)
      return RFLV_BUSY;
   if (track < 1 || track > 2 || !m->timescale[track - 1] || !data || !size || size > m->limit
         || (m->seen[track - 1] && dts < m->last[track - 1])
         || !flv_milliseconds(dts, m->timescale[track - 1], &dts_ms)
         || !flv_milliseconds(pts, m->timescale[track - 1], &pts_ms))
      return RFLV_ERROR;
   composition = (int64_t)pts_ms - dts_ms;
   if ((track == 1 && (composition < -0x800000 || composition > 0x7fffff
               || (!m->seen[0] && !keyframe))) || (track == 2 && pts != dts))
      return RFLV_ERROR;
   m->track = track;
   m->input = (const uint8_t*)data;
   m->size = size;
   m->offset = 0;
   m->input_dts = dts;
   m->timestamp = dts_ms;
   m->composition = (int32_t)composition;
   m->keyframe = keyframe;
   m->vcl = m->idr = false;
   m->output = FLV_MEDIA;
   m->scanning = track == 1;
   if (track == 2)
      flv_tag(m, track, data, size, dts_ms, 0, false, false);
   return RFLV_PROGRESS;
}

int rflv_mux_step(rflv_mux_t *m)
{
   uint32_t length;
   unsigned type;
   if (!m || m->failed)
      return RFLV_ERROR;
   if (m->pending)
      return RFLV_PACKET;
   if (m->finished)
      return RFLV_END;
   if (!m->scanning)
      return RFLV_BUSY;
   if (m->offset == m->size)
   {
      if (!m->vcl || (!m->seen[0] && !m->idr))
         goto failed;
      m->scanning = false;
      flv_tag(m, 1, m->input, m->size, m->timestamp, m->composition, m->keyframe, false);
      return RFLV_PACKET;
   }
   if (m->size - m->offset < 4)
      goto failed;
   length = ((uint32_t)m->input[m->offset] << 24) | ((uint32_t)m->input[m->offset + 1] << 16)
      | ((uint32_t)m->input[m->offset + 2] << 8) | m->input[m->offset + 3];
   m->offset += 4;
   if (!length || length > m->size - m->offset || (m->input[m->offset] & 0x80))
      goto failed;
   type = m->input[m->offset] & 31;
   if (!type || type >= 24)
      goto failed;
   m->vcl = m->vcl || type <= 5;
   m->idr = m->idr || type == 5;
   m->offset += length;
   return RFLV_PROGRESS;
failed:
   m->failed = true;
   m->scanning = false;
   m->input = NULL;
   return RFLV_ERROR;
}

bool rflv_mux_get(rflv_mux_t *m, const rflv_span_t **spans, size_t *count)
{
   if (!m || !spans || !count || !m->pending)
      return false;
   *spans = m->spans;
   *count = m->span_count;
   return true;
}

bool rflv_mux_release(rflv_mux_t *m)
{
   if (!m || !m->pending)
      return false;
   m->pending = false;
   if (m->output == FLV_HEADER && m->avcc_size)
   {
      m->output = FLV_VIDEO_CONFIG;
      flv_tag(m, 1, m->config, m->avcc_size, 0, 0, true, true);
   }
   else if ((m->output == FLV_HEADER || m->output == FLV_VIDEO_CONFIG) && m->asc_size)
   {
      m->output = FLV_AUDIO_CONFIG;
      flv_tag(m, 2, m->config + m->avcc_size, m->asc_size, 0, 0, false, true);
   }
   else
   {
      if (m->output == FLV_MEDIA)
      {
         m->last[m->track - 1] = m->input_dts;
         m->seen[m->track - 1] = true;
      }
      m->output = FLV_IDLE;
      m->input = NULL;
      m->span_count = 0;
   }
   return true;
}

bool rflv_mux_finish(rflv_mux_t *m)
{
   if (!m || m->failed || m->pending || m->scanning)
      return false;
   m->finished = true;
   return true;
}

void rflv_mux_free(rflv_mux_t *m)
{
   if (!m)
      return;
   free(m->config);
   free(m);
}
