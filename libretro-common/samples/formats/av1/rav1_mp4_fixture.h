/* SPDX-License-Identifier: MIT */
#ifndef RAV1_MP4_FIXTURE_H
#define RAV1_MP4_FIXTURE_H
#include <stdint.h>
#include <string.h>

static uint8_t bytes[4096];
static size_t used, config_at, media_at, color_at;
static const uint8_t config[] = {0x81, 0x00, 0x0c, 0x00, 0x0a, 0x09, 0x00, 0x00,
                                 0x00, 0x02, 0xaf, 0xff, 0x80, 0x00, 0x01};
static const uint8_t packet[] = {0x32, 0x05, 0x0e, 0xff, 0x40,
                                 0x00, 0x80, 0x18, 0x88};

static void put(unsigned n)
{
   bytes[used++] = (uint8_t)n;
}
static void be32(uint32_t n)
{
   put(n >> 24);
   put(n >> 16);
   put(n >> 8);
   put(n);
}
static void zeros(size_t n)
{
   while (n--)
      put(0);
}
static void copy(const void *p, size_t n)
{
   memcpy(bytes + used, p, n);
   used += n;
}
static void patch32(size_t at, uint32_t n)
{
   bytes[at] = n >> 24;
   bytes[at + 1] = n >> 16;
   bytes[at + 2] = n >> 8;
   bytes[at + 3] = n;
}
static size_t box(const char *name)
{
   size_t at = used;
   be32(0);
   copy(name, 4);
   return at;
}
static void end_box(size_t at)
{
   patch32(at, (uint32_t)(used - at));
}

static void make_mp4_clip(int with_config, int hdr)
{
   static const uint8_t cll[] = {0x2a, 6, 1, 3, 232, 1, 144, 128};
   static const uint8_t final_cll[] = {0x2a, 6, 1, 15, 160, 3, 232, 128};
   static const uint8_t changed_cll[] = {0x2a, 6, 1, 7, 208, 2, 88, 128};
   static const uint8_t unknown[] = {0x2a, 2, 0, 128};
   size_t b, moov, trak, mdia, minf, stbl, stsd, entry, chunk;
   unsigned i;
   used = 0;
   config_at = 0;
   b = box("ftyp");
   copy("isom", 4);
   be32(0);
   copy("av01", 4);
   end_box(b);
   moov = box("moov");
   b = box("mvhd");
   zeros(12);
   be32(1000);
   be32(hdr >= 2 ? 120 : 80);
   zeros(80);
   end_box(b);
   trak = box("trak");
   b = box("tkhd");
   zeros(12);
   be32(1);
   zeros(68);
   end_box(b);
   mdia = box("mdia");
   b = box("mdhd");
   zeros(12);
   be32(1000);
   be32(hdr >= 2 ? 120 : 80);
   zeros(4);
   end_box(b);
   b = box("hdlr");
   zeros(8);
   copy("vide", 4);
   zeros(13);
   end_box(b);
   minf = box("minf");
   stbl = box("stbl");
   stsd = box("stsd");
   be32(0);
   be32(1);
   entry = box("av01");
   zeros(24);
   put(0);
   put(64);
   put(0);
   put(64);
   zeros(50);
   if (with_config)
   {
      b = box("av1C");
      config_at = used;
      copy(config, sizeof(config));
      end_box(b);
   }
   b = box("colr");
   copy("nclx", 4);
   color_at = used;
   put(0);
   put(1);
   put(0);
   put(2);
   put(0);
   put(2);
   put(0);
   end_box(b);
   if (!hdr)
   {
      b = box("clli"); put(4); put(0); put(1); put(0); end_box(b);
      b = box("mdcv");
      for (i = 0; i < 8; i++)
      {
         put(0x61);
         put(0xa8);
      }
      be32(10240000); be32(0); end_box(b);
   }
   end_box(entry);
   end_box(stsd);
   b = box("stts");
   be32(0);
   be32(1);
   be32(hdr >= 2 ? 3 : 2);
   be32(40);
   end_box(b);
   b = box("stsc");
   be32(0);
   be32(1);
   be32(1);
   be32(hdr >= 2 ? 3 : 2);
   be32(1);
   end_box(b);
   b = box("stsz");
   be32(0);
   be32(hdr ? 0 : sizeof(packet));
   be32(hdr >= 2 ? 3 : 2);
   if (hdr)
   {
      be32((uint32_t)(sizeof(packet) + sizeof(cll) + 40 * sizeof(unknown)
            + (hdr == 4 ? sizeof(config) - 4 : 0)));
      be32((uint32_t)(sizeof(packet) + (hdr >= 2 ? sizeof(changed_cll) : 0)));
      if (hdr >= 2) be32(sizeof(packet) + (hdr == 3 ? sizeof(final_cll) : 0));
   }
   end_box(b);
   b = box("stco");
   be32(0);
   be32(1);
   chunk = used;
   be32(0);
   end_box(b);
   b = box("stss");
   be32(0);
   be32(hdr >= 2 ? 3 : hdr ? 2 : 1);
   be32(1);
   if (hdr) be32(2);
   if (hdr >= 2) be32(3);
   end_box(b);
   end_box(stbl);
   end_box(minf);
   end_box(mdia);
   end_box(trak);
   end_box(moov);
   b = box("mdat");
   media_at = used;
   if (hdr == 4) copy(config + 4, sizeof(config) - 4);
   if (hdr)
   {
      for (i = 0; i < 40; i++) copy(unknown, sizeof(unknown));
      copy(cll, sizeof(cll));
   }
   copy(packet, sizeof(packet));
   if (hdr >= 2) copy(changed_cll, sizeof(changed_cll));
   copy(packet, sizeof(packet));
   if (hdr == 3) copy(final_cll, sizeof(final_cll));
   if (hdr >= 2) copy(packet, sizeof(packet));
   end_box(b);
   patch32(chunk, (uint32_t)media_at);
}

static void make_mp4(int with_config)
{
   make_mp4_clip(with_config, 0);
}

#endif
