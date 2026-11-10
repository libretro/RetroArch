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
   FLV_HEADER, FLV_COLOR, FLV_VIDEO_CONFIG, FLV_AUDIO_CONFIG, FLV_MEDIA, FLV_IDLE
};

struct rflv_mux
{
   uint64_t last[2], input_dts;
   rflv_span_t spans[3];
   uint8_t *config;
   uint8_t *converted;
   uint8_t *metadata;
   const uint8_t *input;
   size_t avcc_size, asc_size, limit, size, offset, span_count;
   size_t converted_size;
   size_t metadata_size;
   uint32_t timestamp;
   int32_t composition;
   unsigned timescale[2], track;
   enum flv_output output;
   uint8_t prefix[19], suffix[4];
   bool pending, scanning, failed, finished, seen[2], keyframe, vcl, idr;
   bool annexb, hevc;
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

static bool flv_hvcc(const uint8_t *data, size_t size)
{
   size_t at = 23;
   unsigned arrays, count, type, length, present = 0;
   if (!data || size < 23 || size > 65536 || data[0] != 1
         || (data[13] & 0xf0) != 0xf0 || (data[15] & 0xfc) != 0xfc
         || (data[16] & 0xfc) != 0xfc || (data[17] & 0xf8) != 0xf8
         || (data[18] & 0xf8) != 0xf8
         || (data[21] & 3) != 3 || !data[22])
      return false;
   for (arrays = data[22]; arrays; arrays--)
   {
      if (size - at < 3 || (data[at] & 0x40))
         return false;
      type = data[at++] & 63;
      count = ((unsigned)data[at] << 8) | data[at + 1];
      at += 2;
      if (!count)
         return false;
      while (count--)
      {
         if (size - at < 2)
            return false;
         length = ((unsigned)data[at] << 8) | data[at + 1];
         at += 2;
         if (length < 2 || length > size - at || (data[at] & 0x80)
               || ((data[at] >> 1) & 63) != type || !(data[at + 1] & 7))
            return false;
         at += length;
      }
      if (type >= 32 && type <= 34)
         present |= 1u << (type - 32);
   }
   return at == size && present == 7;
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
   unsigned bytes = track == 1 ? (m->hevc && !config && composition ? 8 : 5) : 2;
   memset(m->prefix, 0, sizeof(m->prefix));
   m->prefix[0] = track == 1 ? 9 : 8;
   flv_uint(m->prefix + 1, (uint32_t)size + bytes, 3);
   flv_uint(m->prefix + 4, timestamp, 3);
   m->prefix[7] = (uint8_t)(timestamp >> 24);
   m->prefix[11] = track == 1 ? (keyframe ? 0x17 : 0x27) : 0xaf;
   m->prefix[12] = config ? 0 : 1;
   if (track == 1 && m->hevc)
   {
      m->prefix[11] = (uint8_t)(0x80 | (keyframe ? 0x10 : 0x20)
            | (config ? 0 : composition ? 1 : 3));
      memcpy(m->prefix + 12, "hvc1", 4);
      if (bytes == 8)
         flv_uint(m->prefix + 16, (uint32_t)composition, 3);
   }
   else if (track == 1)
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

static rflv_mux_t *flv_new(const rflv_config_t *c, bool hevc)
{
   rflv_mux_t *m;
   if (!c || (!c->avcc && !c->asc) || (hevc && !c->avcc)
         || !c->max_packet_size || c->max_packet_size > (hevc ? 0xfffff7u : 0xfffffau)
         || (c->avcc ? !c->video_timescale
            || !(hevc ? flv_hvcc((const uint8_t*)c->avcc, c->avcc_size)
               : flv_avcc((const uint8_t*)c->avcc, c->avcc_size))
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
   m->hevc = hevc;
   memcpy(m->prefix, "FLV\1\0\0\0\0\11\0\0\0\0", 13);
   m->prefix[4] = (c->avcc ? 1 : 0) | (c->asc ? 4 : 0);
   m->spans[0].data = m->prefix;
   m->spans[0].size = 13;
   m->span_count = 1;
   m->pending = true;
   return m;
}

rflv_mux_t *rflv_mux_new(const rflv_config_t *c)
{
   return flv_new(c, false);
}

rflv_mux_t *rflv_mux_new_hevc(const rflv_config_t *c)
{
   return flv_new(c, true);
}

struct flv_metadata_writer
{
   uint8_t *data;
   size_t used;
   bool failed;
};

static void flv_metadata_bytes(struct flv_metadata_writer *w,
      const void *data, size_t size)
{
   if (w->failed || size > 512 - w->used)
   {
      w->failed = true;
      return;
   }
   memcpy(w->data + w->used, data, size);
   w->used += size;
}

static void flv_metadata_name(struct flv_metadata_writer *w,
      const char *name, bool property)
{
   uint8_t header[3];
   size_t size = strlen(name);
   header[0] = 2;
   header[1] = (uint8_t)(size >> 8);
   header[2] = (uint8_t)size;
   flv_metadata_bytes(w, header + (property ? 1 : 0), property ? 2 : 3);
   flv_metadata_bytes(w, name, size);
}

static void flv_metadata_object(struct flv_metadata_writer *w, const char *name)
{
   static const uint8_t marker = 3;
   flv_metadata_name(w, name, true);
   flv_metadata_bytes(w, &marker, 1);
}

static void flv_metadata_end(struct flv_metadata_writer *w)
{
   static const uint8_t end[] = {0, 0, 9};
   flv_metadata_bytes(w, end, sizeof(end));
}

/* Encode the nonnegative rational as binary64 without host float assumptions. */
static void flv_metadata_number(struct flv_metadata_writer *w, const char *name,
      uint32_t numerator, unsigned denominator)
{
   uint64_t n = numerator, d = denominator, fraction = 0, bits = 0, remainder;
   int exponent = 0;
   unsigned i;
   if (n)
   {
      while (n < d)
      {
         n <<= 1;
         exponent--;
      }
      while (n >= d * 2)
      {
         d <<= 1;
         exponent++;
      }
      remainder = n - d;
      for (i = 0; i < 52; i++)
      {
         remainder <<= 1;
         fraction <<= 1;
         if (remainder >= d)
         {
            remainder -= d;
            fraction++;
         }
      }
      if (remainder * 2 > d || (remainder * 2 == d && (fraction & 1)))
         fraction++;
      if (fraction == ((uint64_t)1 << 52))
      {
         fraction = 0;
         exponent++;
      }
      bits = ((uint64_t)(exponent + 1023) << 52) | fraction;
   }
   flv_metadata_name(w, name, true);
   if (w->failed || 512 - w->used < 9)
   {
      w->failed = true;
      return;
   }
   w->data[w->used++] = 0;
   for (i = 0; i < 8; i++)
      w->data[w->used++] = (uint8_t)(bits >> (56 - i * 8));
}

bool rflv_mux_set_color(rflv_mux_t *m, const video_color_metadata_t *c,
      unsigned depth)
{
   static const char *const xy[] =
   { "redX", "redY", "greenX", "greenY", "blueX", "blueY", "whitePointX", "whitePointY" };
   struct flv_metadata_writer w;
   static const uint8_t object = 3;
   unsigned i;
   if (!m || !c || !m->hevc || m->failed || m->metadata
         || m->output != FLV_HEADER || !m->pending
         || (depth != 8 && depth != 10 && depth != 12)
         || depth != 8u + (m->config[17] & 7)
         || ((m->config[16] & 3) && depth != 8u + (m->config[18] & 7))
         || c->primaries > 255 || c->transfer > 255 || c->matrix > 255
         || c->range > 2 || c->max_cll > 10000 || c->max_fall > 10000
         || (c->max_cll && c->max_fall > c->max_cll))
      return false;
   if (c->mastering.present)
   {
      if (c->mastering.present != VIDEO_MASTERING_DISPLAY_COMPLETE
            || c->mastering.luminance_max < 50000 || c->mastering.luminance_max > 100000000
            || !c->mastering.luminance_min || c->mastering.luminance_min > 50000)
         return false;
      for (i = 0; i < 8; i++)
         if (c->mastering.chromaticity[i] < 5
               || c->mastering.chromaticity[i] > (i & 1 ? 42000 : 37000))
            return false;
   }
   w.data = (uint8_t*)malloc(512);
   if (!w.data)
      return false;
   w.used = 0;
   w.failed = false;
   flv_metadata_name(&w, "colorInfo", false);
   flv_metadata_bytes(&w, &object, 1);
   flv_metadata_object(&w, "colorConfig");
   flv_metadata_number(&w, "bitDepth", depth, 1);
   if (c->primaries)
      flv_metadata_number(&w, "colorPrimaries", c->primaries, 1);
   if (c->transfer)
      flv_metadata_number(&w, "transferCharacteristics", c->transfer, 1);
   if (c->matrix)
      flv_metadata_number(&w, "matrixCoefficients", c->matrix, 1);
   flv_metadata_end(&w);
   if (c->max_cll || c->max_fall)
   {
      flv_metadata_object(&w, "hdrCll");
      if (c->max_cll)
         flv_metadata_number(&w, "maxCLL", c->max_cll, 1);
      if (c->max_fall)
         flv_metadata_number(&w, "maxFall", c->max_fall, 1);
      flv_metadata_end(&w);
   }
   if (c->mastering.present)
   {
      flv_metadata_object(&w, "hdrMdcv");
      for (i = 0; i < 8; i++)
         flv_metadata_number(&w, xy[i], (c->mastering.chromaticity[i] + 2u) / 5, 10000);
      flv_metadata_number(&w, "maxLuminance", c->mastering.luminance_max, 10000);
      flv_metadata_number(&w, "minLuminance", c->mastering.luminance_min, 10000);
      flv_metadata_end(&w);
   }
   flv_metadata_end(&w);
   if (w.failed)
   {
      free(w.data);
      return false;
   }
   m->metadata = w.data;
   m->metadata_size = w.used;
   return true;
}

static bool flv_annexb(const uint8_t *data, size_t size, size_t *offset,
      const uint8_t **nal, size_t *length)
{
   size_t start = *offset, at, zeros = 0, end;
   while (start < size && !data[start])
      start++;
   if (start - *offset < 2 || start == size || data[start] != 1)
      return false;
   start++;
   end = size;
   for (at = start; at < size; at++)
   {
      if (!data[at])
         zeros++;
      else
      {
         if (data[at] == 1 && zeros >= 2)
         {
            end = at - zeros;
            break;
         }
         zeros = 0;
      }
   }
   *offset = at < size ? end : size;
   if (at == size)
      end -= zeros;
   if (end <= start)
      return false;
   *nal = data + start;
   *length = end - start;
   return true;
}

struct flv_rbsp
{
   const uint8_t *data;
   size_t size, at;
   unsigned byte, left, zeros;
   bool failed;
};

static unsigned flv_rbsp_bits(struct flv_rbsp *b, unsigned count)
{
   unsigned value = 0, next;
   while (count--)
   {
      if (!b->left)
      {
         if (b->at == b->size)
            goto failed;
         next = b->data[b->at++];
         if (b->zeros == 2 && next == 3)
         {
            if (b->at == b->size || b->data[b->at] > 3)
               goto failed;
            b->zeros = 0;
            next = b->data[b->at++];
         }
         b->zeros = next ? 0 : b->zeros < 2 ? b->zeros + 1 : 2;
         b->byte = next;
         b->left = 8;
      }
      value = (value << 1) | ((b->byte >> --b->left) & 1);
   }
   return value;
failed:
   b->failed = true;
   return 0;
}

static unsigned flv_rbsp_ue(struct flv_rbsp *b)
{
   unsigned zeros = 0;
   while (!b->failed && !flv_rbsp_bits(b, 1))
      if (++zeros == 32)
         b->failed = true;
   if (b->failed)
      return ~0u;
   return ((1u << zeros) - 1) + flv_rbsp_bits(b, zeros);
}

static bool flv_hevc_info(const uint8_t *sps, size_t size, uint8_t *header)
{
   struct flv_rbsp b;
   unsigned layers, nested, flags = 0, i, j, chroma, luma, chroma_depth;
   memset(&b, 0, sizeof(b));
   b.data = sps + 2;
   b.size = size - 2;
   flv_rbsp_bits(&b, 4);
   layers = flv_rbsp_bits(&b, 3);
   nested = flv_rbsp_bits(&b, 1);
   if (layers > 6)
      return false;
   for (i = 1; i <= 12; i++)
      header[i] = (uint8_t)flv_rbsp_bits(&b, 8);
   for (i = 0; i < layers; i++)
      flags |= flv_rbsp_bits(&b, 2) << (i * 2);
   if (layers)
      for (i = layers; i < 8; i++)
         if (flv_rbsp_bits(&b, 2))
            return false;
   for (i = 0; i < layers; i++)
   {
      if (flags & (2u << (i * 2)))
         for (j = 0; j < 11; j++)
            flv_rbsp_bits(&b, 8);
      if (flags & (1u << (i * 2)))
         flv_rbsp_bits(&b, 8);
   }
   if (flv_rbsp_ue(&b) > 15)
      return false;
   chroma = flv_rbsp_ue(&b);
   if (chroma > 3)
      return false;
   if (chroma == 3)
      flv_rbsp_bits(&b, 1);
   if (!flv_rbsp_ue(&b) || !flv_rbsp_ue(&b))
      return false;
   if (flv_rbsp_bits(&b, 1))
      for (i = 0; i < 4; i++)
         flv_rbsp_ue(&b);
   luma = flv_rbsp_ue(&b);
   chroma_depth = flv_rbsp_ue(&b);
   if (b.failed || luma > 6 || chroma_depth > 6)
      return false;
   header[0] = 1;
   header[13] = 0xf0;
   header[15] = 0xfc;
   header[16] = (uint8_t)(0xfc | chroma);
   header[17] = (uint8_t)(0xf8 | luma);
   header[18] = (uint8_t)(0xf8 | chroma_depth);
   header[21] = (uint8_t)(((layers + 1) << 3) | (nested << 2) | 3);
   header[22] = 3;
   return true;
}

rflv_mux_t *rflv_mux_new_hevc_annexb(const rflv_config_t *c)
{
   const uint8_t *sets[3] = {NULL, NULL, NULL}, *nal;
   size_t lengths[3] = {0, 0, 0}, at = 0, size, total = 38;
   unsigned type, i;
   uint8_t *hvcc;
   rflv_config_t config;
   rflv_mux_t *m;
   if (!c || !c->avcc || !c->avcc_size || c->avcc_size > 65536
         || !c->max_packet_size || c->max_packet_size > 0xfffff7)
      return NULL;
   while (at < c->avcc_size)
   {
      if (!flv_annexb((const uint8_t*)c->avcc, c->avcc_size, &at, &nal, &size)
            || size < 3 || size > 65535 || (nal[0] & 0x81) || nal[1] != 1)
         return NULL;
      type = (nal[0] >> 1) & 63;
      if (type < 32 || type > 34 || sets[type - 32] || size > 65536 - total)
         return NULL;
      sets[type - 32] = nal;
      lengths[type - 32] = size;
      total += size;
   }
   if (!sets[0] || !sets[1] || !sets[2])
      return NULL;
   hvcc = (uint8_t*)calloc(1, total);
   if (!hvcc)
      return NULL;
   if (!flv_hevc_info(sets[1], lengths[1], hvcc))
   {
      free(hvcc);
      return NULL;
   }
   at = 23;
   for (i = 0; i < 3; i++)
   {
      hvcc[at++] = (uint8_t)(0xa0 + i);
      flv_uint(hvcc + at, 1, 2);
      flv_uint(hvcc + at + 2, (uint32_t)lengths[i], 2);
      memcpy(hvcc + at + 4, sets[i], lengths[i]);
      at += lengths[i] + 4;
   }
   config = *c;
   config.avcc = hvcc;
   config.avcc_size = total;
   m = rflv_mux_new_hevc(&config);
   free(hvcc);
   if (!m)
      return NULL;
   m->converted = (uint8_t*)malloc(m->limit);
   if (!m->converted)
   {
      rflv_mux_free(m);
      return NULL;
   }
   return m;
}

rflv_mux_t *rflv_mux_new_annexb(const rflv_config_t *c)
{
   rflv_config_t config;
   rflv_mux_t *m;
   const uint8_t *nal, *sps = NULL, *pps = NULL;
   uint8_t *avcc;
   size_t offset = 0, size, sps_size = 0, pps_size = 0;
   if (!c || !c->avcc || !c->avcc_size || c->avcc_size > 65536
         || !c->max_packet_size || c->max_packet_size > 0xfffffa)
      return NULL;
   while (offset < c->avcc_size)
   {
      unsigned type;
      if (!flv_annexb((const uint8_t*)c->avcc, c->avcc_size, &offset, &nal, &size)
            || size > 65535 || (nal[0] & 0x80))
         return NULL;
      type = nal[0] & 31;
      if (type == 7 && !sps && size >= 4)
      {
         sps = nal;
         sps_size = size;
      }
      else if (type == 8 && !pps)
      {
         pps = nal;
         pps_size = size;
      }
      else
         return NULL;
   }
   if (!sps || !pps || sps_size + pps_size > 65536 - 11)
      return NULL;
   size = sps_size + pps_size + 11;
   avcc = (uint8_t*)malloc(size);
   if (!avcc)
      return NULL;
   avcc[0] = 1;
   memcpy(avcc + 1, sps + 1, 3);
   avcc[4] = 255;
   avcc[5] = 225;
   flv_uint(avcc + 6, (uint32_t)sps_size, 2);
   memcpy(avcc + 8, sps, sps_size);
   avcc[8 + sps_size] = 1;
   flv_uint(avcc + 9 + sps_size, (uint32_t)pps_size, 2);
   memcpy(avcc + 11 + sps_size, pps, pps_size);
   config = *c;
   config.avcc = avcc;
   config.avcc_size = size;
   m = rflv_mux_new(&config);
   free(avcc);
   if (!m)
      return NULL;
   m->converted = (uint8_t*)malloc(m->limit);
   if (!m->converted)
   {
      rflv_mux_free(m);
      return NULL;
   }
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
   m->annexb = false;
   m->converted_size = 0;
   m->output = FLV_MEDIA;
   m->scanning = track == 1;
   if (track == 2)
      flv_tag(m, track, data, size, dts_ms, 0, false, false);
   return RFLV_PROGRESS;
}

int rflv_mux_push_annexb(rflv_mux_t *m, const void *data, size_t size,
      uint64_t dts, uint64_t pts, bool keyframe)
{
   int result;
   if (!m || !m->converted)
      return RFLV_ERROR;
   result = rflv_mux_push(m, 1, data, size, dts, pts, keyframe);
   if (result == RFLV_PROGRESS)
      m->annexb = true;
   return result;
}

int rflv_mux_step(rflv_mux_t *m)
{
   const uint8_t *nal;
   size_t size;
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
      flv_tag(m, 1, m->annexb ? m->converted : m->input,
            m->annexb ? m->converted_size : m->size,
            m->timestamp, m->composition, m->keyframe, false);
      return RFLV_PACKET;
   }
   if (m->annexb)
   {
      if (!flv_annexb(m->input, m->size, &m->offset, &nal, &size)
            || m->limit - m->converted_size < 4
            || size > m->limit - m->converted_size - 4)
         goto failed;
      flv_uint(m->converted + m->converted_size, (uint32_t)size, 4);
      memcpy(m->converted + m->converted_size + 4, nal, size);
      m->converted_size += size + 4;
   }
   else
   {
      if (m->size - m->offset < 4)
         goto failed;
      length = ((uint32_t)m->input[m->offset] << 24) | ((uint32_t)m->input[m->offset + 1] << 16)
         | ((uint32_t)m->input[m->offset + 2] << 8) | m->input[m->offset + 3];
      m->offset += 4;
      if (!length || length > m->size - m->offset)
         goto failed;
      nal = m->input + m->offset;
      size = length;
      m->offset += length;
   }
   if (nal[0] & 0x80)
      goto failed;
   if (m->hevc)
   {
      if (size < 2 || !(nal[1] & 7))
         goto failed;
      type = (nal[0] >> 1) & 63;
      m->vcl = m->vcl || type <= 31;
      m->idr = m->idr || (type >= 16 && type <= 23);
      return RFLV_PROGRESS;
   }
   type = nal[0] & 31;
   if (!type || type >= 24)
      goto failed;
   m->vcl = m->vcl || type <= 5;
   m->idr = m->idr || type == 5;
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

bool rflv_mux_get_tag(rflv_mux_t *m, rflv_tag_t *tag)
{
   if (!m || !tag || !m->pending || m->output == FLV_HEADER)
      return false;
   tag->spans[0].data = m->prefix + 11;
   tag->spans[0].size = m->spans[0].size - 11;
   tag->spans[1] = m->spans[1];
   tag->timestamp = m->timestamp;
   tag->type = m->prefix[0];
   return true;
}

bool rflv_mux_release(rflv_mux_t *m)
{
   if (!m || !m->pending)
      return false;
   m->pending = false;
   if (m->output == FLV_HEADER && m->metadata)
   {
      m->output = FLV_COLOR;
      flv_tag(m, 1, m->metadata, m->metadata_size, 0, 0, true, true);
      /* Information-frame flags preserve existing demuxer interoperability. */
      m->prefix[11] = 0xd4;
   }
   else if ((m->output == FLV_HEADER || m->output == FLV_COLOR) && m->avcc_size)
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
   free(m->converted);
   free(m->metadata);
   free(m);
}
