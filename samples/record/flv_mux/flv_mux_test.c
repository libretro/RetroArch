/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_mux_flv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const unsigned char avcc[] =
{ 1, 66, 0, 30, 255, 225, 0, 4, 103, 66, 0, 30, 1, 0, 1, 104 };
static const unsigned char asc[] = { 18, 16 };
static const unsigned char idr[] =
{ 0, 0, 0, 2, 101, 136, 0, 0, 0, 2, 6, 128 };
static const unsigned char predicted[] = { 0, 0, 0, 2, 65, 154 };

static record_flv_config_t config(void)
{
   record_flv_config_t c;
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

static void headers(record_mux_flv_t *m, unsigned flags)
{
   const record_flv_span_t *s;
   const unsigned char *p;
   size_t n;
   assert(record_mux_flv_get(m, &s, &n) && n == 1);
   p = (const unsigned char*)s[0].data;
   assert(s[0].size == 13 && !memcmp(p, "FLV\1", 4) && p[4] == flags);
   assert(be(p + 5, 4) == 9 && be(p + 9, 4) == 0);
   assert(!record_mux_flv_finish(m));
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 0, 0, true) == RECORD_FLV_BUSY);
   assert(record_mux_flv_release(m));
   if (flags & 1)
   {
      assert(record_mux_flv_get(m, &s, &n) && n == 3);
      p = (const unsigned char*)s[0].data;
      assert(p[0] == 9 && p[11] == 23 && p[12] == 0);
      assert(be(p + 1, 3) == sizeof(avcc) + 5);
      assert(s[1].size == sizeof(avcc) && !memcmp(s[1].data, avcc, sizeof(avcc)));
      assert(be(s[2].data, 4) == sizeof(avcc) + 16);
      assert(record_mux_flv_release(m));
   }
   if (flags & 4)
   {
      assert(record_mux_flv_get(m, &s, &n) && n == 3);
      p = (const unsigned char*)s[0].data;
      assert(p[0] == 8 && p[11] == 175 && p[12] == 0);
      assert(s[1].size == sizeof(asc) && !memcmp(s[1].data, asc, sizeof(asc)));
      assert(record_mux_flv_release(m));
   }
   assert(record_mux_flv_step(m) == RECORD_FLV_BUSY);
}

static void lifetime(void)
{
   record_flv_config_t c = config();
   unsigned char owned[sizeof(avcc)];
   record_mux_flv_t *m;
   const record_flv_span_t *s;
   const unsigned char *p;
   size_t n;
   memcpy(owned, avcc, sizeof(owned));
   c.avcc = owned;
   m = record_mux_flv_new(&c);
   assert(m);
   memset(owned, 0, sizeof(owned));
   headers(m, 5);
   assert(record_mux_flv_push(m, 1, predicted, sizeof(predicted), 0, 0, false) == RECORD_FLV_ERROR);
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 90000, 81000, true) == RECORD_FLV_PROGRESS);
   assert(!record_mux_flv_finish(m));
   assert(record_mux_flv_step(m) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PACKET);
   assert(record_mux_flv_get(m, &s, &n) && n == 3 && s[1].data == idr);
   p = (const unsigned char*)s[0].data;
   assert(be(p + 4, 3) == 1000 && be(p + 13, 3) == 0xffff9c);
   assert(record_mux_flv_step(m) == RECORD_FLV_PACKET);
   assert(record_mux_flv_push(m, 2, asc, sizeof(asc), 0, 0, false) == RECORD_FLV_BUSY);
   assert(be(p + 13, 3) == 0xffff9c);
   assert(record_mux_flv_release(m));
   assert(record_mux_flv_push(m, 1, predicted, sizeof(predicted), 89999, 90000, false) == RECORD_FLV_ERROR);
   assert(record_mux_flv_push(m, 2, asc, sizeof(asc), 44100, 44100, false) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_get(m, &s, &n) && s[1].data == asc);
   p = (const unsigned char*)s[0].data;
   assert(be(p + 4, 3) == 1000 && p[12] == 1);
   assert(record_mux_flv_release(m));
   assert(record_mux_flv_finish(m) && record_mux_flv_finish(m));
   assert(record_mux_flv_step(m) == RECORD_FLV_END);
   assert(record_mux_flv_push(m, 2, asc, sizeof(asc), 88200, 88200, false) == RECORD_FLV_ERROR);
   record_mux_flv_free(m);
}

static void invalid(void)
{
   record_flv_config_t c = config();
   record_mux_flv_t *m;
   unsigned char bad[sizeof(idr)];
   unsigned i;
   assert(!record_mux_flv_new(NULL));
   for (i = 0; i < sizeof(avcc); i++)
   {
      c.avcc_size = i;
      assert(!record_mux_flv_new(&c));
   }
   c = config();
   c.max_packet_size = 0;
   assert(!record_mux_flv_new(&c));
   c = config();
   c.video_timescale = 0;
   assert(!record_mux_flv_new(&c));
   for (i = 0; i < 3; i++)
   {
      c = config();
      m = record_mux_flv_new(&c);
      assert(m);
      headers(m, 5);
      memcpy(bad, idr, sizeof(bad));
      if (i == 0)
         bad[3] = 255;
      if (i == 1)
         bad[4] = 229;
      if (i == 2)
         bad[4] = 120;
      assert(record_mux_flv_push(m, 1, bad, sizeof(bad), 0, 0, true) == RECORD_FLV_PROGRESS);
      assert(record_mux_flv_step(m) == RECORD_FLV_ERROR);
      assert(!record_mux_flv_finish(m));
      record_mux_flv_free(m);
   }
}

static void single_track(void)
{
   record_flv_config_t c = config();
   record_mux_flv_t *m;
   c.avcc = NULL;
   c.avcc_size = c.video_timescale = 0;
   m = record_mux_flv_new(&c);
   assert(m);
   headers(m, 4);
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 0, 0, true) == RECORD_FLV_ERROR);
   assert(record_mux_flv_push(m, 2, asc, sizeof(asc), 0, 1, false) == RECORD_FLV_ERROR);
   assert(record_mux_flv_finish(m));
   record_mux_flv_free(m);
   c = config();
   c.asc = NULL;
   c.asc_size = c.audio_timescale = 0;
   m = record_mux_flv_new(&c);
   assert(m);
   headers(m, 1);
   assert(record_mux_flv_finish(m));
   record_mux_flv_free(m);
}

static void timestamp_limits(void)
{
   record_flv_config_t c = config();
   record_mux_flv_t *m;
   const record_flv_span_t *s;
   const unsigned char *p;
   size_t n;
   c.video_timescale = 1000;
   m = record_mux_flv_new(&c);
   assert(m);
   headers(m, 5);
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 0, 0x800000, true) == RECORD_FLV_ERROR);
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 0x800001, 0, true) == RECORD_FLV_ERROR);
   assert(record_mux_flv_push(m, 1, idr, sizeof(idr), 0x800000, 0, true) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PACKET);
   assert(record_mux_flv_get(m, &s, &n));
   p = (const unsigned char*)s[0].data;
   assert(be(p + 13, 3) == 0x800000);
   assert(record_mux_flv_release(m));
   assert(record_mux_flv_push(m, 1, predicted, sizeof(predicted),
            0x7fffffff, 0x7fffffff, false) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PROGRESS);
   assert(record_mux_flv_step(m) == RECORD_FLV_PACKET);
   assert(record_mux_flv_get(m, &s, &n));
   p = (const unsigned char*)s[0].data;
   assert(p[7] == 127 && be(p + 4, 3) == 0xffffff);
   assert(record_mux_flv_release(m));
   assert(record_mux_flv_push(m, 1, predicted, sizeof(predicted),
            (uint64_t)0x7fffffff + 1, (uint64_t)0x7fffffff + 1, false) == RECORD_FLV_ERROR);
   assert(record_mux_flv_finish(m));
   record_mux_flv_free(m);
}

int main(void)
{
   lifetime();
   invalid();
   single_track();
   timestamp_limits();
   puts("PASS: FLV framing, borrowed packets, clocks, admission and teardown");
   return 0;
}
