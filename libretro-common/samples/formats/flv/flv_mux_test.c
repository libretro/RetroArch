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
#include <string.h>

static const unsigned char avcc[] =
{ 1, 66, 0, 30, 255, 225, 0, 4, 103, 66, 0, 30, 1, 0, 1, 104 };
static const unsigned char asc[] = { 18, 16 };
static const unsigned char idr[] =
{ 0, 0, 0, 2, 101, 136, 0, 0, 0, 2, 6, 128 };
static const unsigned char predicted[] = { 0, 0, 0, 2, 65, 154 };

static rflv_config_t config(void)
{
   rflv_config_t c;
   memset(&c, 0, sizeof(c));
   c.avcc = avcc;
   c.avcc_size = sizeof(avcc);
   c.asc = asc;
   c.asc_size = sizeof(asc);
   c.video_timescale = 90000;
   c.audio_timescale = 44100;
   c.max_packet_size = 1024;
   return c;
}

static unsigned be(const void *data, unsigned count)
{
   const unsigned char *p = (const unsigned char*)data;
   unsigned value = 0, i;
   for (i = 0; i < count; i++)
      value = (value << 8) | p[i];
   return value;
}

static void headers(rflv_mux_t *m, unsigned flags)
{
   rflv_tag_t tag;
   const rflv_span_t *s;
   const unsigned char *p;
   size_t n;
   assert(rflv_mux_get(m, &s, &n) && n == 1);
   assert(!rflv_mux_get_tag(m, &tag));
   p = (const unsigned char*)s[0].data;
   assert(s[0].size == 13 && !memcmp(p, "FLV\1", 4) && p[4] == flags);
   assert(be(p + 5, 4) == 9 && be(p + 9, 4) == 0);
   assert(!rflv_mux_finish(m));
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 0, 0, true) == RFLV_BUSY);
   assert(rflv_mux_release(m));
   if (flags & 1)
   {
      assert(rflv_mux_get(m, &s, &n) && n == 3);
      p = (const unsigned char*)s[0].data;
      assert(p[0] == 9 && p[11] == 23 && p[12] == 0);
      assert(rflv_mux_get_tag(m, &tag) && tag.type == 9 && !tag.timestamp);
      assert(tag.spans[0].data == p + 11 && tag.spans[0].size == 5);
      assert(tag.spans[1].data == s[1].data && tag.spans[1].size == sizeof(avcc));
      assert(be(p + 1, 3) == sizeof(avcc) + 5);
      assert(s[1].size == sizeof(avcc) && !memcmp(s[1].data, avcc, sizeof(avcc)));
      assert(be(s[2].data, 4) == sizeof(avcc) + 16);
      assert(rflv_mux_release(m));
   }
   if (flags & 4)
   {
      assert(rflv_mux_get(m, &s, &n) && n == 3);
      p = (const unsigned char*)s[0].data;
      assert(p[0] == 8 && p[11] == 175 && p[12] == 0);
      assert(rflv_mux_get_tag(m, &tag) && tag.type == 8 && !tag.timestamp);
      assert(tag.spans[0].data == p + 11 && tag.spans[0].size == 2);
      assert(tag.spans[1].data == s[1].data && tag.spans[1].size == sizeof(asc));
      assert(s[1].size == sizeof(asc) && !memcmp(s[1].data, asc, sizeof(asc)));
      assert(rflv_mux_release(m));
   }
   assert(rflv_mux_step(m) == RFLV_BUSY);
   assert(!rflv_mux_get_tag(m, &tag));
}

static void lifetime(void)
{
   rflv_tag_t tag;
   rflv_config_t c = config();
   unsigned char owned[sizeof(avcc)];
   rflv_mux_t *m;
   const rflv_span_t *s;
   const unsigned char *p;
   size_t n;
   memcpy(owned, avcc, sizeof(owned));
   c.avcc = owned;
   m = rflv_mux_new(&c);
   assert(m);
   assert(!rflv_mux_get_tag(NULL, &tag) && !rflv_mux_get_tag(m, NULL));
   memset(owned, 0, sizeof(owned));
   headers(m, 5);
   assert(rflv_mux_push(m, 1, predicted, sizeof(predicted), 0, 0, false) == RFLV_ERROR);
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 90000, 81000, true) == RFLV_PROGRESS);
   assert(!rflv_mux_finish(m));
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get(m, &s, &n) && n == 3 && s[1].data == idr);
   p = (const unsigned char*)s[0].data;
   assert(be(p + 4, 3) == 1000 && be(p + 13, 3) == 0xffff9c);
   assert(rflv_mux_get_tag(m, &tag) && tag.timestamp == 1000 && tag.type == 9);
   assert(tag.spans[0].data == p + 11 && tag.spans[0].size == 5);
   assert(tag.spans[1].data == idr && tag.spans[1].size == sizeof(idr));
   assert(rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_push(m, 2, asc, sizeof(asc), 0, 0, false) == RFLV_BUSY);
   assert(be(p + 13, 3) == 0xffff9c);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, predicted, sizeof(predicted), 89999, 90000, false) == RFLV_ERROR);
   assert(rflv_mux_push(m, 2, asc, sizeof(asc), 44100, 44100, false) == RFLV_PROGRESS);
   assert(rflv_mux_get(m, &s, &n) && s[1].data == asc);
   p = (const unsigned char*)s[0].data;
   assert(be(p + 4, 3) == 1000 && p[12] == 1);
   assert(rflv_mux_get_tag(m, &tag) && tag.timestamp == 1000 && tag.type == 8);
   assert(tag.spans[1].data == asc && tag.spans[1].size == sizeof(asc));
   assert(rflv_mux_release(m));
   assert(rflv_mux_finish(m) && rflv_mux_finish(m));
   assert(rflv_mux_step(m) == RFLV_END);
   assert(rflv_mux_push(m, 2, asc, sizeof(asc), 88200, 88200, false) == RFLV_ERROR);
   rflv_mux_free(m);
}

static void invalid(void)
{
   rflv_config_t c = config();
   rflv_mux_t *m;
   unsigned char bad[sizeof(idr)];
   unsigned i;
   assert(!rflv_mux_new(NULL));
   for (i = 0; i < sizeof(avcc); i++)
   {
      c.avcc_size = i;
      assert(!rflv_mux_new(&c));
   }
   c = config();
   c.max_packet_size = 0;
   assert(!rflv_mux_new(&c));
   c = config();
   c.video_timescale = 0;
   assert(!rflv_mux_new(&c));
   for (i = 0; i < 3; i++)
   {
      c = config();
      m = rflv_mux_new(&c);
      assert(m);
      headers(m, 5);
      memcpy(bad, idr, sizeof(bad));
      if (i == 0)
         bad[3] = 255;
      if (i == 1)
         bad[4] = 229;
      if (i == 2)
         bad[4] = 120;
      assert(rflv_mux_push(m, 1, bad, sizeof(bad), 0, 0, true) == RFLV_PROGRESS);
      assert(rflv_mux_step(m) == RFLV_ERROR);
      assert(!rflv_mux_finish(m));
      rflv_mux_free(m);
   }
}

static void single_track(void)
{
   rflv_config_t c = config();
   rflv_mux_t *m;
   c.avcc = NULL;
   c.avcc_size = c.video_timescale = 0;
   m = rflv_mux_new(&c);
   assert(m);
   headers(m, 4);
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 0, 0, true) == RFLV_ERROR);
   assert(rflv_mux_push(m, 2, asc, sizeof(asc), 0, 1, false) == RFLV_ERROR);
   assert(rflv_mux_finish(m));
   rflv_mux_free(m);
   c = config();
   c.asc = NULL;
   c.asc_size = c.audio_timescale = 0;
   m = rflv_mux_new(&c);
   assert(m);
   headers(m, 1);
   assert(rflv_mux_finish(m));
   rflv_mux_free(m);
}

static void timestamp_limits(void)
{
   rflv_config_t c = config();
   rflv_mux_t *m;
   const rflv_span_t *s;
   const unsigned char *p;
   size_t n;
   c.video_timescale = 1000;
   m = rflv_mux_new(&c);
   assert(m);
   headers(m, 5);
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 0, 0x800000, true) == RFLV_ERROR);
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 0x800001, 0, true) == RFLV_ERROR);
   assert(rflv_mux_push(m, 1, idr, sizeof(idr), 0x800000, 0, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get(m, &s, &n));
   p = (const unsigned char*)s[0].data;
   assert(be(p + 13, 3) == 0x800000);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, predicted, sizeof(predicted),
            0x7fffffff, 0x7fffffff, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get(m, &s, &n));
   p = (const unsigned char*)s[0].data;
   assert(p[7] == 127 && be(p + 4, 3) == 0xffffff);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, predicted, sizeof(predicted),
            (uint64_t)0x7fffffff + 1, (uint64_t)0x7fffffff + 1, false) == RFLV_ERROR);
   assert(rflv_mux_finish(m));
   rflv_mux_free(m);
}

static void annexb(void)
{
   static const unsigned char sets[] =
      { 0, 0, 0, 1, 103, 66, 0, 30, 0, 0, 1, 104 };
   static const unsigned char packet[] =
      { 0, 0, 1, 101, 136, 0, 0, 0, 1, 6, 128, 0, 0 };
   static const unsigned char bad[] = { 0, 0, 1, 0, 0, 1, 101 };
   rflv_config_t c = config();
   rflv_mux_t *m;
   rflv_tag_t tag;
   const void *converted;
   c.avcc = sets;
   c.avcc_size = sizeof(sets);
   m = rflv_mux_new_annexb(&c);
   assert(m && rflv_mux_release(m));
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[1].size == sizeof(avcc));
   assert(!memcmp(tag.spans[1].data, avcc, sizeof(avcc)));
   assert(rflv_mux_release(m) && rflv_mux_release(m));
   assert(rflv_mux_push_annexb(m, packet, sizeof(packet), 0, 0, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS);
   assert(rflv_mux_push_annexb(m, bad, sizeof(bad), 0, 0, true) == RFLV_BUSY);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[1].size == sizeof(idr));
   assert(!memcmp(tag.spans[1].data, idr, sizeof(idr)));
   converted = tag.spans[1].data;
   assert(rflv_mux_step(m) == RFLV_PACKET && rflv_mux_get_tag(m, &tag));
   assert(tag.spans[1].data == converted && !memcmp(packet,
         "\0\0\1\145\210\0\0\0\1\6\200\0\0", sizeof(packet)));
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, predicted, sizeof(predicted), 9000, 9000, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[1].data == predicted);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push_annexb(m, bad, sizeof(bad), 18000, 18000, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_ERROR && !rflv_mux_finish(m));
   rflv_mux_free(m);
   c.max_packet_size = 11;
   m = rflv_mux_new_annexb(&c);
   assert(m);
   headers(m, 5);
   assert(rflv_mux_push_annexb(m, packet, 11, 0, 0, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_ERROR);
   rflv_mux_free(m);
   c.avcc_size = 8;
   assert(!rflv_mux_new_annexb(&c));
}

static void hevc(void)
{
   unsigned char hvcc[44];
   static const unsigned char cra[] = {0, 0, 0, 2, 42, 1};
   static const unsigned char inter[] = {0, 0, 0, 2, 2, 1};
   static const unsigned char bad[] = {0, 0, 0, 2, 42, 0};
   rflv_config_t c = config();
   rflv_mux_t *m;
   rflv_tag_t tag;
   const rflv_span_t *spans;
   size_t count, i, at;
   memset(hvcc, 0, sizeof(hvcc));
   hvcc[0] = 1;
   hvcc[13] = 0xf0;
   hvcc[15] = hvcc[16] = 0xfc;
   hvcc[17] = hvcc[18] = 0xf8;
   hvcc[21] = hvcc[22] = 3;
   for (i = 0; i < 3; i++)
   {
      at = 23 + 7 * i;
      hvcc[at] = (unsigned char)(0xa0 + i);
      hvcc[at + 2] = 1;
      hvcc[at + 4] = 2;
      hvcc[at + 5] = (unsigned char)(64 + 2 * i);
      hvcc[at + 6] = 1;
   }
   c.avcc = hvcc;
   c.asc = NULL; c.asc_size = c.audio_timescale = 0;
   for (i = 0; i < sizeof(hvcc); i++)
   {
      c.avcc_size = i;
      assert(!rflv_mux_new_hevc(&c));
   }
   c.avcc_size = sizeof(hvcc);
   c.max_packet_size = 0xfffff8;
   assert(!rflv_mux_new_hevc(&c));
   c.max_packet_size = 1024;
   m = rflv_mux_new_hevc(&c);
   assert(m && rflv_mux_release(m));
   hvcc[23] = 0;
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[0].size == 5);
   assert(!memcmp(tag.spans[0].data, "\220hvc1", 5));
   assert(tag.spans[1].size == sizeof(hvcc));
   assert(((const unsigned char*)tag.spans[1].data)[23] == 0xa0);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, cra, sizeof(cra), 0, 90, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[1].data == cra);
   assert(tag.spans[0].size == 8 && !memcmp(tag.spans[0].data, "\221hvc1\0\0\1", 8));
   assert(rflv_mux_step(m) == RFLV_PACKET && rflv_mux_get_tag(m, &tag));
   assert(tag.spans[1].data == cra);
   assert(rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, inter, sizeof(inter), 90000, 90000, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get_tag(m, &tag) && tag.timestamp == 1000);
   assert(tag.spans[0].size == 5 && !memcmp(tag.spans[0].data, "\243hvc1", 5));
   assert(tag.spans[1].data == inter && rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, inter, sizeof(inter), 180000, 179910, false) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_PACKET);
   assert(rflv_mux_get_tag(m, &tag) && tag.spans[0].size == 8);
   assert(!memcmp(tag.spans[0].data, "\241hvc1\377\377\377", 8));
   assert(rflv_mux_get(m, &spans, &count) && count == 3);
   assert(be(spans[0].data, 1) == 9 && be((const unsigned char*)spans[0].data + 1, 3) == sizeof(inter) + 8);
   assert(be(spans[2].data, 4) == sizeof(inter) + 19);
   assert(rflv_mux_release(m) && rflv_mux_finish(m));
   rflv_mux_free(m);
   hvcc[23] = 0xa0;
   hvcc[13] = 0;
   assert(!rflv_mux_new_hevc(&c));
   hvcc[13] = 0xf0;
   hvcc[21] = 2;
   assert(!rflv_mux_new_hevc(&c));
   hvcc[21] = 3;
   hvcc[29] = 0;
   assert(!rflv_mux_new_hevc(&c));
   hvcc[29] = 1;
   m = rflv_mux_new_hevc(&c);
   assert(m && rflv_mux_release(m) && rflv_mux_release(m));
   assert(rflv_mux_push_annexb(m, cra, sizeof(cra), 0, 0, true) == RFLV_ERROR);
   assert(rflv_mux_push(m, 1, bad, sizeof(bad), 0, 0, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_ERROR);
   rflv_mux_free(m);
   m = rflv_mux_new_hevc(&c);
   assert(m && rflv_mux_release(m) && rflv_mux_release(m));
   assert(rflv_mux_push(m, 1, inter, sizeof(inter), 0, 0, true) == RFLV_PROGRESS);
   assert(rflv_mux_step(m) == RFLV_PROGRESS && rflv_mux_step(m) == RFLV_ERROR);
   rflv_mux_free(m);
}

static void hevc_test_bits(unsigned char *data, unsigned *at, unsigned value, unsigned count)
{
   while (count--)
   {
      assert(*at < 1024);
      data[*at / 8] |= (unsigned char)(((value >> count) & 1) << (7 - *at % 8));
      (*at)++;
   }
}

static void hevc_test_ue(unsigned char *data, unsigned *at, unsigned value)
{
   unsigned width = 0, n = ++value;
   while (n >>= 1)
      width++;
   hevc_test_bits(data, at, 0, width);
   hevc_test_bits(data, at, value, width + 1);
}

static size_t hevc_test_sets(unsigned char *data, unsigned layers, unsigned depth)
{
   unsigned char rbsp[128];
   unsigned at = 0, i, zeros = 0;
   size_t used = 13;
   memset(rbsp, 0, sizeof(rbsp));
   memcpy(data, "\0\0\0\1\100\1\252\0\0\0\1\102\1", 13);
   hevc_test_bits(rbsp, &at, (layers << 1) | 1, 8);
   for (i = 0; i < 12; i++)
      hevc_test_bits(rbsp, &at, 0x11, 8);
   for (i = 0; i < layers; i++)
      hevc_test_bits(rbsp, &at, i == 0 ? 3 : i == 1 ? 1 : 0, 2);
   if (layers)
      for (i = layers; i < 8; i++)
         hevc_test_bits(rbsp, &at, 0, 2);
   if (layers)
   {
      for (i = 0; i < 12; i++)
         hevc_test_bits(rbsp, &at, 0x11, 8);
      if (layers > 1)
         hevc_test_bits(rbsp, &at, 0x22, 8);
   }
   hevc_test_ue(rbsp, &at, 0);
   hevc_test_ue(rbsp, &at, 1);
   hevc_test_ue(rbsp, &at, 128);
   hevc_test_ue(rbsp, &at, 96);
   hevc_test_bits(rbsp, &at, layers ? 1 : 0, 1);
   if (layers)
      for (i = 0; i < 4; i++)
         hevc_test_ue(rbsp, &at, 0);
   hevc_test_ue(rbsp, &at, depth);
   hevc_test_ue(rbsp, &at, depth);
   hevc_test_bits(rbsp, &at, 1, 1);
   for (i = 0; i < (at + 7) / 8; i++)
   {
      if (zeros == 2 && rbsp[i] <= 3)
      {
         data[used++] = 3;
         zeros = 0;
      }
      data[used++] = rbsp[i];
      zeros = rbsp[i] ? 0 : zeros + 1;
   }
   memcpy(data + used, "\0\0\1\104\1\252", 6);
   return used + 6;
}

static void hevc_annexb(void)
{
   unsigned char sets[256];
   const unsigned char *hvcc;
   const rflv_span_t *s;
   size_t size, cut, n;
   unsigned layers, depth;
   rflv_config_t c = config();
   rflv_mux_t *m;
   c.avcc = sets;
   for (layers = 0; layers <= 6; layers += 2)
      for (depth = 0; depth <= 4; depth += 2)
      {
         size = hevc_test_sets(sets, layers, depth);
         c.avcc_size = size;
         m = rflv_mux_new_hevc_annexb(&c);
         assert(m && rflv_mux_release(m));
         assert(rflv_mux_get(m, &s, &n) && n == 3);
         hvcc = (const unsigned char*)s[1].data;
         assert(hvcc[1] == 0x11 && hvcc[12] == 0x11);
         assert(hvcc[16] == 0xfd && hvcc[17] == 0xf8 + depth
               && hvcc[18] == 0xf8 + depth && hvcc[21] == ((layers + 1) << 3) + 7);
         rflv_mux_free(m);
         for (cut = 0; cut < size; cut++)
         {
            c.avcc_size = cut;
            assert(!rflv_mux_new_hevc_annexb(&c));
         }
      }
   c.avcc_size = hevc_test_sets(sets, 7, 0);
   assert(!rflv_mux_new_hevc_annexb(&c));
   c.avcc_size = hevc_test_sets(sets, 0, 7);
   assert(!rflv_mux_new_hevc_annexb(&c));
   c.avcc_size = hevc_test_sets(sets, 0, 0);
   sets[11] = 0x40;
   assert(!rflv_mux_new_hevc_annexb(&c));
   puts("PASS: native HEVC SPS depth/temporal fields, emulation prevention and every header truncation");
}

int main(void)
{
   lifetime();
   invalid();
   single_track();
   timestamp_limits();
   annexb();
   hevc();
   hevc_annexb();
   puts("PASS: FLV framing, borrowed packets, clocks, admission and teardown");
   return 0;
}
