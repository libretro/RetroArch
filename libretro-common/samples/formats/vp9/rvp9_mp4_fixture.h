/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RVP9_MP4_FIXTURE_H
#define RVP9_MP4_FIXTURE_H
static uint8_t clip[4096];
static size_t clip_size, config_at, colr_at;
static const uint8_t *sample_data[2];
static size_t sample_size[2];
static void put(unsigned v)
{
   clip[clip_size++] = (uint8_t)v;
}
static void be16(unsigned v)
{
   put(v >> 8);
   put(v);
}
static void be32(uint32_t v)
{
   be16(v >> 16);
   be16(v);
}
static void zero(size_t n)
{
   while (n--)
      put(0);
}
static void patch(size_t at, uint32_t v)
{
   clip[at] = (uint8_t)(v >> 24);
   clip[at + 1] = (uint8_t)(v >> 16);
   clip[at + 2] = (uint8_t)(v >> 8);
   clip[at + 3] = (uint8_t)v;
}
static size_t box(const char *type)
{
   size_t at = clip_size;
   be32(0);
   memcpy(clip + clip_size, type, 4);
   clip_size += 4;
   return at;
}
static void end(size_t at)
{
   patch(at, (uint32_t)(clip_size - at));
}
static void fixture(const uint8_t *config, int with_colr)
{
   size_t b, moov, trak, mdia, minf, stbl, stsd, entry, chunk;
   unsigned i;
   clip_size = config_at = colr_at = 0;
   b = box("ftyp");
   be32(0x69736f6d);
   be32(0);
   end(b);
   moov = box("moov");
   b = box("mvhd");
   zero(12);
   be32(1000);
   be32(80);
   zero(80);
   end(b);
   trak = box("trak");
   b = box("tkhd");
   zero(12);
   be32(1);
   zero(68);
   end(b);
   mdia = box("mdia");
   b = box("mdhd");
   zero(12);
   be32(1000);
   zero(8);
   end(b);
   b = box("hdlr");
   zero(8);
   be32(0x76696465);
   zero(13);
   end(b);
   minf = box("minf");
   stbl = box("stbl");
   stsd = box("stsd");
   be32(0);
   be32(1);
   entry = box("vp09");
   zero(24);
   be16(32);
   be16(16);
   zero(50);
   if (config)
   {
      b = box("vpcC");
      config_at = clip_size;
      memcpy(clip + clip_size, config, 12);
      clip_size += 12;
      end(b);
   }
   if (with_colr)
   {
      b = box("colr");
      be32(0x6e636c78);
      colr_at = clip_size;
      be16(1);
      be16(13);
      be16(1);
      put(0);
      end(b);
   }
   b = box("clli");
   be16(1000);
   be16(400);
   end(b);
   b = box("mdcv");
   for (i = 0; i < 8; i++)
      be16(25000);
   be32(10000000);
   be32(1);
   end(b);
   end(entry);
   end(stsd);
   b = box("stts");
   be32(0);
   be32(1);
   be32(2);
   be32(40);
   end(b);
   b = box("stsc");
   be32(0);
   be32(1);
   be32(1);
   be32(2);
   be32(1);
   end(b);
   b = box("stsz");
   be32(0);
   be32(0);
   be32(2);
   be32((uint32_t)(sample_size[0] ? sample_size[0] : 1));
   be32((uint32_t)(sample_size[1] ? sample_size[1] : 1));
   end(b);
   b = box("stco");
   be32(0);
   be32(1);
   chunk = clip_size;
   be32(0);
   end(b);
   b = box("stss");
   be32(0);
   be32(1);
   be32(1);
   end(b);
   end(stbl);
   end(minf);
   end(mdia);
   end(trak);
   end(moov);
   b = box("mdat");
   patch(chunk, (uint32_t)clip_size);
   for (i = 0; i < 2; i++)
   {
      if (sample_data[i] && sample_size[i])
      {
         memcpy(clip + clip_size, sample_data[i], sample_size[i]);
         clip_size += sample_size[i];
      }
      else
         put(0);
   }
   end(b);
}
#endif
