/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rcontainer.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* AVI and ASF headers: see formats/rcontainer.h. */

#include <string.h>

#include <formats/rcontainer.h>

static uint32_t rc_le16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t rc_le32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rc_le64(const uint8_t *p)
{
   return (uint64_t)rc_le32(p) | ((uint64_t)rc_le32(p + 4) << 32);
}

/* a WAVEFORMATEX into an audio stream */
static void rc_wave(rcontainer_stream *s, const uint8_t *p, size_t n)
{
   s->type = RCONTAINER_STREAM_AUDIO;
   if (n < 16)
      return;
   s->format_tag  = rc_le16(p);
   s->channels    = rc_le16(p + 2);
   s->sample_rate = rc_le32(p + 4);
   s->bitrate     = rc_le32(p + 8) * 8;
   /* WAVE_FORMAT_EXTENSIBLE: the sub-format's first two bytes */
   if (s->format_tag == 0xFFFE && n >= 26 && rc_le16(p + 16) >= 22)
      s->format_tag = rc_le16(p + 24);
}

/* a BITMAPINFOHEADER into a video stream */
static void rc_bitmap(rcontainer_stream *s, const uint8_t *p, size_t n)
{
   int32_t h;
   s->type = RCONTAINER_STREAM_VIDEO;
   if (n < 20)
      return;
   s->width  = rc_le32(p + 4);
   h         = (int32_t)rc_le32(p + 8);
   s->height = (unsigned)(h < 0 ? -h : h);
   s->fourcc = rc_le32(p + 16);
}

/* ---------------------------------------------------------------- */
/* AVI                                                              */
/* ---------------------------------------------------------------- */

/* the chunk of @id among those in [@at, @end): its body */
static bool rc_chunk(const uint8_t *p, size_t at, size_t end, const char *id,
      const char *list, size_t *body, size_t *body_end)
{
   while (at + 8 <= end)
   {
      size_t len = rc_le32(p + at + 4);
      if (len > end - at - 8)
         len = end - at - 8;
      if (!memcmp(p + at, id, 4) && (!list || (len >= 4 && !memcmp(p + at + 8, list, 4))))
      {
         *body     = at + 8 + (list ? 4 : 0);
         *body_end = at + 8 + len;
         return true;
      }
      at += 8 + len + (len & 1);
   }
   return false;
}

static bool rc_avi(const uint8_t *p, size_t n, rcontainer_info *info)
{
   size_t   hdrl, hdrl_end, b, b_end, at;
   uint64_t frames = 0, period = 0, bytes[RCONTAINER_MAX_STREAMS];
   double   seconds = 0;
   unsigned i;

   if (!rc_chunk(p, 12, n, "LIST", "hdrl", &hdrl, &hdrl_end))
      return false;
   info->kind = RCONTAINER_AVI;
   if (rc_chunk(p, hdrl, hdrl_end, "avih", NULL, &b, &b_end) && b_end - b >= 20)
   {
      period = rc_le32(p + b);        /* microseconds a frame */
      frames = rc_le32(p + b + 16);
   }
   /* each stream list in turn */
   for (at = hdrl; at + 8 <= hdrl_end; )
   {
      size_t len = rc_le32(p + at + 4), sl, sl_end, h, h_end, f, f_end;
      if (len > hdrl_end - at - 8)
         len = hdrl_end - at - 8;
      if (!memcmp(p + at, "LIST", 4) && len >= 4 && !memcmp(p + at + 8, "strl", 4)
            && info->num_streams < RCONTAINER_MAX_STREAMS)
      {
         rcontainer_stream *s = &info->streams[info->num_streams++];
         sl     = at + 12;
         sl_end = at + 8 + len;
         if (rc_chunk(p, sl, sl_end, "strh", NULL, &h, &h_end) && h_end - h >= 36)
         {
            uint32_t scale = rc_le32(p + h + 20), rate = rc_le32(p + h + 24);
            uint32_t length = rc_le32(p + h + 32);
            if (rc_chunk(p, sl, sl_end, "strf", NULL, &f, &f_end))
            {
               if (!memcmp(p + h, "vids", 4))
                  rc_bitmap(s, p + f, f_end - f);
               else if (!memcmp(p + h, "auds", 4))
                  rc_wave(s, p + f, f_end - f);
            }
            /* the longest stream's length is the file's, as FFmpeg
             * gives it */
            if (scale && rate && (double)length * scale / rate > seconds)
               seconds = (double)length * scale / rate;
         }
      }
      at += 8 + len + (len & 1);
   }
   if (!seconds && frames && period)
      seconds = (double)frames * period / 1e6;
   info->duration_us = (int64_t)(seconds * 1e6 + 0.5);

   /* each stream's bytes, from idx1: "NNxx" ids, NN the stream */
   memset(bytes, 0, sizeof(bytes));
   if (seconds > 0 && rc_chunk(p, 12, n, "idx1", NULL, &b, &b_end))
   {
      for (at = b; at + 16 <= b_end; at += 16)
      {
         unsigned d0 = p[at], d1 = p[at + 1], k;
         if (d0 < '0' || d0 > '9' || d1 < '0' || d1 > '9')
            continue;
         k = (d0 - '0') * 10 + (d1 - '0');
         if (k < info->num_streams)
            bytes[k] += rc_le32(p + at + 12);
      }
      for (i = 0; i < info->num_streams; i++)
         if (bytes[i] && info->streams[i].type == RCONTAINER_STREAM_VIDEO)
            info->streams[i].bitrate = (uint32_t)(bytes[i] * 8 / seconds);
   }
   return info->num_streams > 0;
}

/* ---------------------------------------------------------------- */
/* ASF                                                              */
/* ---------------------------------------------------------------- */

static const uint8_t rc_asf_header[16]  = { 0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C };
static const uint8_t rc_asf_file[16]    = { 0xA1,0xDC,0xAB,0x8C,0x47,0xA9,0xCF,0x11,0x8E,0xE4,0x00,0xC0,0x0C,0x20,0x53,0x65 };
static const uint8_t rc_asf_stream[16]  = { 0x91,0x07,0xDC,0xB7,0xB7,0xA9,0xCF,0x11,0x8E,0xE6,0x00,0xC0,0x0C,0x20,0x53,0x65 };
static const uint8_t rc_asf_audio[16]   = { 0x40,0x9E,0x69,0xF8,0x4D,0x5B,0xCF,0x11,0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B };
static const uint8_t rc_asf_video[16]   = { 0xC0,0xEF,0x19,0xBC,0x4D,0x5B,0xCF,0x11,0xA8,0xFD,0x00,0x80,0x5F,0x5C,0x44,0x2B };
static const uint8_t rc_asf_bitrate[16] = { 0xCE,0x75,0xF8,0x7B,0x8D,0x46,0xD1,0x11,0x8D,0x82,0x00,0x60,0x97,0xC9,0xA2,0xB2 };

static bool rc_asf(const uint8_t *p, size_t n, rcontainer_info *info)
{
   uint64_t size   = rc_le64(p + 16);
   size_t   end    = size < n ? (size_t)size : n, at = 30;
   unsigned number[RCONTAINER_MAX_STREAMS], i;
   uint32_t rates[128];

   info->kind = RCONTAINER_ASF;
   memset(rates, 0, sizeof(rates));
   while (at + 24 <= end)
   {
      uint64_t osize = rc_le64(p + at + 16);
      size_t   body = at + 24, oend;
      if (osize < 24 || osize > end - at)
         break;
      oend = at + (size_t)osize;
      if (!memcmp(p + at, rc_asf_file, 16) && oend - body >= 68)
      {
         uint64_t play = rc_le64(p + body + 40), preroll = rc_le64(p + body + 56);
         if (!(rc_le32(p + body + 64) & 1) && preroll < play / 10000)
            info->duration_us = (int64_t)((play - preroll * 10000) / 10);
      }
      else if (!memcmp(p + at, rc_asf_stream, 16) && oend - body >= 54
            && info->num_streams < RCONTAINER_MAX_STREAMS)
      {
         rcontainer_stream *s   = &info->streams[info->num_streams];
         uint32_t           tlen = rc_le32(p + body + 40);
         size_t             t    = body + 54;
         number[info->num_streams++] = rc_le16(p + body + 48) & 0x7f;
         if (tlen > oend - t)
            tlen = (uint32_t)(oend - t);
         if (!memcmp(p + body, rc_asf_audio, 16))
            rc_wave(s, p + t, tlen);
         else if (!memcmp(p + body, rc_asf_video, 16))
         {
            /* width, height, a flag byte, the format data's size, then
             * a BITMAPINFOHEADER */
            s->type = RCONTAINER_STREAM_VIDEO;
            if (tlen >= 11)
               rc_bitmap(s, p + t + 11, tlen - 11);
            if (tlen >= 8)
            {
               s->width  = rc_le32(p + t);
               s->height = rc_le32(p + t + 4);
            }
         }
      }
      else if (!memcmp(p + at, rc_asf_bitrate, 16) && oend - body >= 2)
      {
         unsigned count = rc_le16(p + body), k;
         for (k = 0; k < count && body + 2 + k * 6 + 6 <= oend; k++)
         {
            const uint8_t *r = p + body + 2 + k * 6;
            rates[rc_le16(r) & 0x7f] = rc_le32(r + 2);
         }
      }
      at = oend;
   }
   for (i = 0; i < info->num_streams; i++)
      if (rates[number[i]])
         info->streams[i].bitrate = rates[number[i]];
   return info->num_streams > 0;
}

bool rcontainer_probe(const uint8_t *data, size_t len, rcontainer_info *info)
{
   if (!info)
      return false;
   memset(info, 0, sizeof(*info));
   if (!data)
      return false;
   if (len >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "AVI ", 4))
      return rc_avi(data, len, info);
   if (len >= 30 && !memcmp(data, rc_asf_header, 16))
      return rc_asf(data, len, info);
   return false;
}
