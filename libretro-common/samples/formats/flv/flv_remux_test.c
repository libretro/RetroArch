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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned read_be(const unsigned char *p, unsigned n)
{
   unsigned v = 0, i;
   for (i = 0; i < n; i++)
      v = (v << 8) | p[i];
   return v;
}

static void output(rflv_mux_t *m, FILE *f)
{
   const rflv_span_t *s;
   size_t n, i;
   while (rflv_mux_get(m, &s, &n))
   {
      for (i = 0; i < n; i++)
         assert(fwrite(s[i].data, 1, s[i].size, f) == s[i].size);
      assert(rflv_mux_release(m));
   }
}

static rflv_mux_t *hevc_annexb_mux(const rflv_config_t *c)
{
   rflv_config_t config = *c;
   const unsigned char *hvcc = (const unsigned char*)c->avcc;
   unsigned char *sets;
   size_t at = 23, used = 0, size;
   unsigned arrays, count, type;
   rflv_mux_t *mux;
   assert(c->avcc_size >= 23);
   sets = (unsigned char*)malloc(c->avcc_size + 16);
   assert(sets);
   for (arrays = hvcc[22]; arrays; arrays--)
   {
      assert(c->avcc_size - at >= 3);
      type = hvcc[at++] & 63;
      count = read_be(hvcc + at, 2);
      at += 2;
      while (count--)
      {
         assert(c->avcc_size - at >= 2);
         size = read_be(hvcc + at, 2);
         at += 2;
         assert(size && size <= c->avcc_size - at);
         if (type >= 32 && type <= 34)
         {
            assert(used + size + 4 <= c->avcc_size + 16);
            memcpy(sets + used, "\0\0\0\1", 4);
            memcpy(sets + used + 4, hvcc + at, size);
            used += size + 4;
         }
         at += size;
      }
   }
   assert(at == c->avcc_size);
   config.avcc = sets;
   config.avcc_size = used;
   mux = rflv_mux_new_hevc_annexb(&config);
   free(sets);
   return mux;
}

static rflv_mux_t *annexb_mux(const rflv_config_t *c)
{
   rflv_config_t config = *c;
   const unsigned char *avcc = (const unsigned char*)c->avcc;
   unsigned char *sets;
   size_t sps, pps, at;
   rflv_mux_t *mux;
   assert(c->avcc_size >= 7 && (avcc[5] & 31) == 1);
   sps = read_be(avcc + 6, 2);
   at = 8 + sps;
   assert(at + 3 <= c->avcc_size && avcc[at] == 1);
   pps = read_be(avcc + at + 1, 2);
   assert(pps && pps <= c->avcc_size - at - 3);
   sets = (unsigned char*)malloc(sps + pps + 8);
   assert(sets);
   memcpy(sets, "\0\0\0\1", 4);
   memcpy(sets + 4, avcc + 8, sps);
   memcpy(sets + sps + 4, "\0\0\0\1", 4);
   memcpy(sets + sps + 8, avcc + at + 3, pps);
   config.avcc = sets;
   config.avcc_size = sps + pps + 8;
   mux = rflv_mux_new_annexb(&config);
   free(sets);
   return mux;
}

static void annexb_packet(unsigned char *data, size_t size)
{
   size_t at = 0, length;
   while (at < size)
   {
      assert(size - at >= 4);
      length = read_be(data + at, 4);
      assert(length && length <= size - at - 4);
      memcpy(data + at, "\0\0\0\1", 4);
      at += length + 4;
   }
}

int main(int argc, char **argv)
{
   rflv_config_t c;
   rflv_mux_t *m;
   FILE *f;
   unsigned char *data, *p;
   size_t size, pos, length;
   long bytes;
   unsigned pass, type, timestamp, prefix, packet_type;
   int result, composition;
   int64_t pts;
   bool annexb = argc == 4 && (!strcmp(argv[3], "annexb") || !strcmp(argv[3], "annexb-color"));
   bool native_av1 = argc == 4 && (!strcmp(argv[3], "obu") || !strcmp(argv[3], "obu-color"));
   bool metadata = argc == 4 && (!strcmp(argv[3], "color") || !strcmp(argv[3], "annexb-color")
         || !strcmp(argv[3], "obu-color"));
   bool hevc = false, av1 = false, keyframe;
   assert(argc == 3 || (argc == 4 && (annexb || native_av1 || metadata)));
   f = fopen(argv[1], "rb");
   assert(f && !fseek(f, 0, SEEK_END));
   bytes = ftell(f);
   assert(bytes >= 13 && bytes < 64 * 1024 * 1024 && !fseek(f, 0, SEEK_SET));
   size = (size_t)bytes;
   data = (unsigned char*)malloc(size);
   assert(data && fread(data, 1, size, f) == size && !fclose(f));
   assert(!memcmp(data, "FLV\1", 4) && read_be(data + 5, 4) == 9);
   memset(&c, 0, sizeof(c));
   c.max_packet_size = 0xfffff7;
   m = NULL;
   f = NULL;
   for (pass = 0; pass < 2; pass++)
   {
      for (pos = 13; pos < size; pos += length + 15)
      {
         assert(size - pos >= 15);
         p = data + pos;
         type = p[0];
         length = read_be(p + 1, 3);
         assert(length <= size - pos - 15);
         assert(read_be(p + 11 + length, 4) == length + 11);
         if (type != 8 && type != 9)
            continue;
         prefix = type == 9 ? 5 : 2;
         assert(length >= prefix);
         if (type == 9)
         {
            hevc = (p[11] & 0x80) && !memcmp(p + 12, "hvc1", 4);
            av1 = (p[11] & 0x80) && !memcmp(p + 12, "av01", 4);
            assert(hevc || av1 || (p[11] & 15) == 7);
            packet_type = hevc || av1 ? p[11] & 15 : p[12];
            if (hevc && packet_type == 1)
               prefix = 8;
         }
         else
         {
            assert((p[11] >> 4) == 10);
            packet_type = p[12];
         }
         if (type == 9 && packet_type == 2)
            continue;
         /* FFmpeg can emit an AV1 sequence marker before configuration. */
         if (type == 9 && av1 && packet_type == 0 && length == prefix)
            continue;
         assert(length > prefix);
         if (!pass)
         {
            if (packet_type != 0)
               continue;
            if (type == 9)
            {
               c.avcc = p + 16;
               c.avcc_size = length - 5;
               c.video_timescale = 1000;
            }
            else
            {
               c.asc = p + 13;
               c.asc_size = length - 2;
               c.audio_timescale = 1000;
            }
            continue;
         }
         if (packet_type != 1 && !(type == 9 && hevc && packet_type == 3))
            continue;
         timestamp = read_be(p + 4, 3) | ((unsigned)p[7] << 24);
         composition = type == 9 && !av1 && (!hevc || packet_type == 1)
            ? (int)read_be(p + (hevc ? 16 : 13), 3) : 0;
         if (composition & 0x800000)
            composition -= 0x1000000;
         pts = (int64_t)timestamp + composition;
         assert(pts >= 0);
         keyframe = type == 9 && ((p[11] >> 4) & 7) == 1;
         if (annexb && type == 9)
         {
            annexb_packet(p + 11 + prefix, length - prefix);
            result = rflv_mux_push_annexb(m, p + 11 + prefix, length - prefix,
                  timestamp, (uint64_t)pts, keyframe);
         }
         else
            result = rflv_mux_push(m, type == 9 ? 1 : 2,
               p + 11 + prefix, length - prefix, timestamp, (uint64_t)pts,
               keyframe);
         assert(result == RFLV_PROGRESS);
         do
            result = rflv_mux_step(m);
         while (result == RFLV_PROGRESS);
         assert(result == RFLV_PACKET);
         output(m, f);
      }
      assert(pos == size);
      if (!pass)
      {
         if (native_av1)
         {
            assert(av1 && c.avcc_size > 4);
            c.avcc = (const unsigned char*)c.avcc + 4;
            c.avcc_size -= 4;
         }
         m = annexb ? (hevc ? hevc_annexb_mux(&c) : annexb_mux(&c))
            : av1 ? native_av1 ? rflv_mux_new_av1_obu(&c) : rflv_mux_new_av1(&c)
            : hevc ? rflv_mux_new_hevc(&c) : rflv_mux_new(&c);
         if (metadata)
         {
            static const uint16_t xy[] = {34000,16000,13250,34500,7500,3000,15635,16450};
            video_color_metadata_t described;
            memset(&described, 0, sizeof(described));
            described.primaries = described.matrix = 9;
            described.transfer = 16;
            described.range = 1;
            described.max_cll = 1000; described.max_fall = 400;
            described.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
            described.mastering.luminance_max = 10000000;
            described.mastering.luminance_min = 50;
            memcpy(described.mastering.chromaticity, xy, sizeof(xy));
            assert((hevc || av1) && rflv_mux_set_color(m, &described, 10));
         }
         f = fopen(argv[2], "wb");
         assert(m && f);
         output(m, f);
      }
   }
   assert(rflv_mux_finish(m));
   assert(!fclose(f));
   rflv_mux_free(m);
   free(data);
   return 0;
}
