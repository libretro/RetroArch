/* SPDX-License-Identifier: MIT */
#ifndef RAV1_WEBM_FIXTURE_H
#define RAV1_WEBM_FIXTURE_H
#include "rav1_mp4_fixture.h"

static size_t element(uint32_t id)
{
   size_t at;
   if (id > 0xffffff)
      put(id >> 24);
   if (id > 0xffff)
      put(id >> 16);
   if (id > 0xff)
      put(id >> 8);
   put(id);
   at = used;
   put(0x40);
   put(0);
   return at;
}
static void end_element(size_t at)
{
   size_t n = used - at - 2;
   bytes[at] = 0x40 | (uint8_t)(n >> 8);
   bytes[at + 1] = (uint8_t)n;
}
static void number(uint32_t id, unsigned n)
{
   size_t at = element(id);
   put(n);
   end_element(at);
}

static void make_webm(int inband, unsigned matrix, unsigned transfer,
                      unsigned range)
{
   size_t b, segment, tracks, track, video, cluster;
   unsigned i;
   used = 0;
   b = element(0x1a45dfa3);
   video = element(0x4282);
   copy("webm", 4);
   end_element(video);
   end_element(b);
   segment = element(0x18538067);
   tracks = element(0x1654ae6b);
   track = element(0xae);
   number(0xd7, 1);
   number(0x83, 1);
   /* CodecPrivate may precede CodecID. */
   b = element(0x63a2);
   config_at = used;
   copy(config, inband ? 4 : sizeof(config));
   end_element(b);
   b = element(0x86);
   copy("V_AV1", 5);
   end_element(b);
   video = element(0xe0);
   number(0xb0, 64);
   number(0xba, 64);
   b = element(0x55b0);
   number(0x55b1, matrix);
   number(0x55b9, range);
   number(0x55ba, transfer);
   end_element(b);
   end_element(video);
   end_element(track);
   end_element(tracks);
   cluster = element(0x1f43b675);
   number(0xe7, 0);
   for (i = 0; i < 2; i++)
   {
      b = element(0xa3);
      put(0x81);
      put(0);
      put(i * 40);
      put(i ? 0 : 128);
      if (!i)
         media_at = used;
      if (!i && inband)
         copy(config + 4, sizeof(config) - 4);
      copy(packet, sizeof(packet));
      end_element(b);
   }
   end_element(cluster);
   end_element(segment);
}

#endif
