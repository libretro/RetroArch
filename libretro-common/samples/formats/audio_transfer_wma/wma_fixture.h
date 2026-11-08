/* Copyright (C) 2026 The RetroArch team. MIT license. */
#ifndef RWMA_TEST_FIXTURE_H
#define RWMA_TEST_FIXTURE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static const uint8_t header_guid[16] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                                        0xa6, 0xd9, 0,    0xaa, 0,    0x62, 0xce, 0x6c};
static const uint8_t data_guid[16] = {0x36, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                                      0xa6, 0xd9, 0,    0xaa, 0,    0x62, 0xce, 0x6c};
static const uint8_t file_guid[16] = {0xa1, 0xdc, 0xab, 0x8c, 0x47, 0xa9, 0xcf, 0x11,
                                      0x8e, 0xe4, 0,    0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t stream_guid[16] = {0x91, 0x07, 0xdc, 0xb7, 0xb7, 0xa9, 0xcf, 0x11,
                                        0x8e, 0xe6, 0,    0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t audio_guid[16] = {0x40, 0x9e, 0x69, 0xf8, 0x4d, 0x5b, 0xcf, 0x11,
                                       0xa8, 0xfd, 0,    0x80, 0x5f, 0x5c, 0x44, 0x2b};

static void le32(uint8_t *p, unsigned value)
{
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      p[i] = (uint8_t)value;
      value >>= 8;
   }
}
static void bits(uint8_t *p, unsigned *at, unsigned n, unsigned value)
{
   while (n--)
   {
      unsigned mask = 1u << (7 - (*at & 7));
      p[*at / 8] = (uint8_t)((p[*at / 8] & ~mask) | (((value >> n) & 1) ? mask : 0));
      (*at)++;
   }
}
static void format(uint8_t *p)
{
   memset(p, 0, 28);
   p[0] = 0x61;
   p[1] = 1;
   p[2] = 1;
   le32(p + 4, 48000);
   le32(p + 8, 16000);
   p[12] = 32;
   p[14] = 16;
   p[16] = 10;
   p[22] = 1;
}
/* A single positive coefficient, constant envelope, no noise coding. */
static void packet_rate(uint8_t *p, unsigned rate)
{
   static const unsigned frequencies[25] = {
       100,  200,  300,  400,  510,  630,  770,  920,  1080, 1270,  1480,  1720, 2000,
       2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 24500};
   unsigned at = 0, i, previous = 0;
   unsigned n = rate <= 16000 ? 512 : rate <= 22050 ? 1024 : 2048;
   unsigned width = rate <= 16000 ? 9 : rate <= 22050 ? 10 : 11;
   memset(p, 0, 32);
   bits(p, &at, 1, 1);
   bits(p, &at, 7, 99);
   for (i = 0; i < 25; i++)
   {
      unsigned end = (n * 2 * frequencies[i] + 2 * rate) / (4 * rate) * 4;
      if (end > n)
         end = n;
      if (end > previous)
         bits(p, &at, 1, 0);
      previous = end;
      if (end == n)
         break;
   }
   bits(p, &at, 12, 3841); /* Coefficient escape, book pair 2. */
   bits(p, &at, 9, n / 8);
   bits(p, &at, width, 0);
   bits(p, &at, 1, 1);
   bits(p, &at, 6, 30); /* End of block. */
}
static void packet(uint8_t *p)
{
   packet_rate(p, 48000);
}
static size_t wave(uint8_t *p)
{
   memset(p, 0, 88);
   memcpy(p, "RIFF", 4);
   le32(p + 4, 80);
   memcpy(p + 8, "XWMAfmt ", 8);
   le32(p + 16, 28);
   format(p + 20);
   memcpy(p + 48, "data", 4);
   le32(p + 52, 32);
   packet(p + 56);
   return 88;
}
static size_t wave_rate(uint8_t *p, unsigned rate)
{
   size_t bytes = wave(p);
   le32(p + 24, rate);
   packet_rate(p + 56, rate);
   return bytes;
}
static void object_header(uint8_t *p, const uint8_t *guid, unsigned size)
{
   memcpy(p, guid, 16);
   le32(p + 16, size);
}
static void asf_packet(uint8_t *p, const uint8_t *encoded, unsigned offset,
                       unsigned length)
{
   memset(p, 0, 80);
   p[0] = 8;
   p[1] = 0x55;
   p[2] = (uint8_t)(80 - 21 - length);
   p[9] = 1;
   p[11] = (uint8_t)offset;
   p[12] = 8;
   le32(p + 13, 32);
   memcpy(p + 21, encoded + offset, length);
}
static size_t asf(uint8_t *p, int fragment)
{
   uint8_t encoded[32];
   unsigned size = fragment ? 450 : 370;
   memset(p, 0, size);
   object_header(p, header_guid, 240);
   le32(p + 24, 2);
   p[28] = 1;
   p[29] = 2;
   object_header(p + 30, file_guid, 104);
   le32(p + 30 + 24 + 68, 80);
   le32(p + 30 + 24 + 72, 80);
   object_header(p + 134, stream_guid, 106);
   memcpy(p + 158, audio_guid, 16);
   le32(p + 198, 28);
   p[206] = 1;
   format(p + 212);
   object_header(p + 240, data_guid, size - 240);
   le32(p + 280, fragment ? 2 : 1);
   p[288] = 1;
   p[289] = 1;
   packet(encoded);
   asf_packet(p + 290, encoded, 0, fragment ? 16 : 32);
   if (fragment)
      asf_packet(p + 370, encoded, 16, 16);
   return size;
}

static size_t xma_wave(uint8_t *p, unsigned channels)
{
   unsigned streams = (channels+1)/2, stream;
   size_t size = 80 + streams*2048;
   memset(p,0,size);
   memcpy(p,"RIFF",4); le32(p+4,(unsigned)size-8); memcpy(p+8,"WAVEfmt ",8);
   le32(p+16,52); p[20]=0x66; p[21]=1; p[22]=(uint8_t)channels;
   le32(p+24,48000); p[32]=(uint8_t)(channels*2); p[34]=16; p[36]=34;
   p[38]=(uint8_t)streams; le32(p+40,channels==3 ? 11:channels==2 ? 3:4);
   le32(p+44,1024); le32(p+48,streams*2048); le32(p+56,512);
   p[69]=4; p[70]=1; memcpy(p+72,"data",4); le32(p+76,streams*2048);
   for (stream=0;stream<streams;stream++)
   {
      uint8_t *packet=p+80+stream*2048;
      unsigned frame, lanes=channels-stream*2>=2 ? 2:1;
      packet[0]=8; packet[3]=(uint8_t)(streams-1);
      for (frame=0;frame<2;frame++)
      {
         unsigned pos=32+frame*40, lane;
         bits(packet,&pos,15,40); bits(packet,&pos,1,1); bits(packet,&pos,1,0);
         if(lanes==2) bits(packet,&pos,1,0);
         bits(packet,&pos,8,0); bits(packet,&pos,1,0);
         bits(packet,&pos,1,0); bits(packet,&pos,1,0);
         if(lanes==2)
         { bits(packet,&pos,1,0); bits(packet,&pos,1,1); bits(packet,&pos,1,0); }
         for(lane=0;lane<lanes;lane++) bits(packet,&pos,1,0);
         pos=32+frame*40+39; bits(packet,&pos,1,frame==0);
      }
   }
   return size;
}
#endif
